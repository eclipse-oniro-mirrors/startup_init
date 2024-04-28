/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#include "param_manager.h"
#include "param_trie.h"
#ifdef SUPPORT_PARAM_LOAD_HOOK
#include "init_module_engine.h"
#endif
#include "securec.h"
#include "init_cmds.h"
#include "init_param.h"

/**
 * Loading system parameter from /proc/cmdline by the following rules:
 *   1) reserved cmdline with or without ohos.boot. prefix listed in CmdlineIterator
        will be processed by the specified processor
 *   2) cmdline not listed in CmdlineIterator but prefixed with ohos.boot will be add by default
 *
 *   Special cases for sn:
 *     a) if sn value in cmdline is started with "/", it means a file to be read as parameter value
 *     b) if sn or ohos.boot.sn are not specified, try to generate sn by GenerateSnByDefault
 */
#define OHOS_CMDLINE_PARA_PREFIX        "ohos.boot."
#define OHOS_CMDLINE_CONST_PARA_PREFIX  "const.product."
#define OHOS_CMDLINE_PARA_PREFIX_LEN    10
#define IMPORT_PREFIX_LEN               7

typedef struct CmdLineInfo {
    const char *name;
    int (*processor)(const char *name, const char *value);
} CmdLineInfo;

typedef struct CmdLineInfoContainer {
    const CmdLineInfo *cmdLineInfo;
    size_t cmdLineInfoSize;
} CmdLineInfoContainer;

typedef struct CmdLineIteratorCtx {
    char *cmdline;
    bool gotSn;
    bool *matches;
} CmdLineIteratorCtx;

static int CommonDealFun(const char *name, const char *value)
{
    int ret = 0;
    PARAM_LOGV("Add param from cmdline %s %s", name, value);
    ret = CheckParamName(name, 0);
    PARAM_CHECK(ret == 0, return ret, "Invalid param name %s", name);
    PARAM_LOGV("Param name %s, value %s", name, value);
    ret = WriteParam(name, value, NULL, 0);
    PARAM_CHECK(ret == 0, return ret, "Failed to write param %s %s", name, value);
    return ret;
}

static int ReadSnFromFile(const char *name, const char *file)
{
    char *data = ReadFileData(file);
    PARAM_CHECK(data != NULL, return -1, "Read sn from %s file failed!", file);

    int index = 0;
    for (size_t i = 0; i < strlen(data); i++) {
        // cancel \r\n
        if (*(data + i) == '\r' || *(data + i) == '\n') {
            break;
        }
        if (*(data + i) != ':') {
            *(data + index) = *(data + i);
            index++;
        }
    }
    data[index] = '\0';
    PARAM_LOGV("**** name %s, value %s", name, data);
    int ret = WriteParam(name, data, NULL, 0);
    free(data);
    PARAM_CHECK(ret == 0, return ret, "Failed to write param %s", name);
    return ret;
}

#define OHOS_SN_PARAM_NAME OHOS_CMDLINE_PARA_PREFIX"sn"

static int SnDealFun(const char *name, const char *value)
{
    int ret = CheckParamName(name, 0);
    PARAM_CHECK(ret == 0, return ret, "Invalid name %s", name);
    if (value != NULL && value[0] != '/') {
        PARAM_LOGV("**** name %s, value %s", name, value);
        ret = WriteParam(OHOS_SN_PARAM_NAME, value, NULL, 0);
        PARAM_CHECK(ret == 0, return ret, "Failed to write param %s %s", name, value);
        return ret;
    }
    if (value != NULL && value[0] == '/') {
        ret = ReadSnFromFile(OHOS_SN_PARAM_NAME, value);
        if (ret == 0) {
            return ret;
        }
    }
    return ret;
}

static int Common2ConstDealFun(const char *name, const char *value)
{
    const char *tmpName;
    tmpName = name;
    if (strncmp(tmpName, OHOS_CMDLINE_PARA_PREFIX, OHOS_CMDLINE_PARA_PREFIX_LEN) == 0) {
        tmpName = tmpName + OHOS_CMDLINE_PARA_PREFIX_LEN;
    }
    char fullName[PARAM_NAME_LEN_MAX];
    int ret = snprintf_s(fullName, sizeof(fullName), sizeof(fullName) - 1,
                         OHOS_CMDLINE_CONST_PARA_PREFIX"%s", tmpName);
    PARAM_CHECK(ret > 0, return ret, "snprinf_s failed");
    ret = CheckParamName(fullName, 0);
    PARAM_CHECK(ret == 0, return ret, "Invalid name %s", name);
    PARAM_LOGV("Param name %s, value %s", fullName, value);
    ret = WriteParam(fullName, value, NULL, 0);
    PARAM_CHECK(ret == 0, return ret, "Failed to write param %s %s", fullName, value);
    return ret;
}

static int MatchReserverCmdline(const NAME_VALUE_PAIR* nv, CmdLineIteratorCtx *ctx, const char *name,
                                CmdLineInfoContainer *container)
{
    const char* tmpName = name;
    char fullName[PARAM_NAME_LEN_MAX];
    int ret = 0;
    const char* matched;

    // Matching reserved cmdlines
    for (size_t i = 0; i < container->cmdLineInfoSize; i++) {
        // Check exact match
        if (strcmp(tmpName, (container->cmdLineInfo + i)->name) != 0) {
            // Check if contains ".xxx" for compatibility
            ret = snprintf_s(fullName, sizeof(fullName), sizeof(fullName) - 1, ".%s",
                            (container->cmdLineInfo + i)->name);
            matched = strstr(tmpName, fullName);
            if (matched == NULL) {
                continue;
            }
            // Check if it is ended with pattern
            if (matched[ret] != '\0') {
                continue;
            }
        }
        ret = snprintf_s(fullName, sizeof(fullName), sizeof(fullName) - 1,
                         OHOS_CMDLINE_PARA_PREFIX "%s", (container->cmdLineInfo + i)->name);
        if (ret <= 0) {
            continue;
        }
        if (ctx->matches[i]) {
            return PARAM_CODE_SUCCESS;
        }
        bool isSnSet = ((container->cmdLineInfo + i)->processor == SnDealFun);
        if (isSnSet && ctx->gotSn) {
            return PARAM_CODE_SUCCESS;
        }
        PARAM_LOGV("proc cmdline %s matched.", fullName);
        ret = (container->cmdLineInfo + i)->processor(fullName, nv->value);
        if (ret == 0) {
            ctx->matches[i] = true;
            if (isSnSet) {
                ctx->gotSn = true;
            }
        }
        return PARAM_CODE_SUCCESS;
    }
    return PARAM_CODE_NOT_FOUND;
}

static const CmdLineInfo CMDLINES[] = {
    { "hardware", CommonDealFun },
    { "bootgroup", CommonDealFun },
    { "reboot_reason", CommonDealFun },
    { "bootslots", CommonDealFun },
    { "sn", SnDealFun },
    { "root_package", CommonDealFun },
    { "serialno", SnDealFun },
    { "udid", Common2ConstDealFun },
    { "productid", Common2ConstDealFun }
};

static void CmdlineIterator(const NAME_VALUE_PAIR *nv, void *context)
{
    CmdLineIteratorCtx *ctx = (CmdLineIteratorCtx *)context;
    char *data = (char *)ctx->cmdline;

    data[nv->nameEnd - data] = '\0';
    data[nv->valueEnd - data] = '\0';
    PARAM_LOGV("proc cmdline: name [%s], value [%s]", nv->name, nv->value);

    // Get name without prefix
    const char *name = nv->name;
    if (strncmp(name, OHOS_CMDLINE_PARA_PREFIX, OHOS_CMDLINE_PARA_PREFIX_LEN) == 0) {
        name = name + OHOS_CMDLINE_PARA_PREFIX_LEN;
    }

    CmdLineInfoContainer container = { 0 };
    container.cmdLineInfo = CMDLINES;
    container.cmdLineInfoSize = ARRAY_LENGTH(CMDLINES);
    if (MatchReserverCmdline(nv, ctx, name, &container) == 0) {
        PARAM_LOGV("match reserver cmd line success, name: %s, value: %s", nv->name, nv->value);
        return;
    }
    if (name == nv->name) {
        return;
    }

    // cmdline with prefix but not matched, add to param by default
    PARAM_LOGI("add proc cmdline param %s by default.", nv->name);
    CommonDealFun(nv->name, nv->value);
}

static void GenerateSnByDefault(void)
{
    const char *snFileList [] = {
        "/sys/block/mmcblk0/device/cid",
        "/proc/bootdevice/cid"
    };

    for (size_t i = 0; i < ARRAY_LENGTH(snFileList); i++) {
        int ret = ReadSnFromFile(OHOS_CMDLINE_PARA_PREFIX "sn", snFileList[i]);
        if (ret == 0) {
            break;
        }
    }
}

INIT_LOCAL_API int LoadParamFromCmdLine(void)
{
    CmdLineIteratorCtx ctx;

    ctx.gotSn = false;
    ctx.cmdline = ReadFileData(BOOT_CMD_LINE);
    PARAM_CHECK(ctx.cmdline != NULL, return -1, "Failed to read file %s", BOOT_CMD_LINE);
    bool matches[ARRAY_LENGTH(CMDLINES)] = {false};
    ctx.matches = matches;
    IterateNameValuePairs(ctx.cmdline, CmdlineIterator, (void *)(&ctx));

    // sn is critical, it must be specified
    if (!ctx.gotSn) {
        PARAM_LOGE("Generate default sn now ...");
        GenerateSnByDefault();
    }

    free(ctx.cmdline);
    return 0;
}

/*
 * Load parameters from files
 */

static int LoadSecurityLabel(const char *fileName)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return -1, "Invalid space");
    PARAM_CHECK(fileName != NULL, return -1, "Invalid filename for load");
#if !(defined __LITEOS_A__ || defined __LITEOS_M__)
    // load security label
    ParamSecurityOps *ops = GetParamSecurityOps(PARAM_SECURITY_DAC);
    if (ops != NULL && ops->securityGetLabel != NULL) {
        if (ops->securityGetLabel(fileName) == PARAM_CODE_REACHED_MAX) {
            PARAM_LOGE("[startup_failed]Load Security Lable failed! system reboot! %d", SYS_PARAM_INIT_FAILED);
            ExecReboot("panic");
        };
    }
#endif
    return 0;
}

static int LoadOneParam_(const uint32_t *context, const char *name, const char *value)
{
    uint32_t mode = *(uint32_t *)context;
    int ret = CheckParamName(name, 0);
    if (ret != 0) {
        return 0;
    }

#ifdef SUPPORT_PARAM_LOAD_HOOK
    PARAM_LOAD_FILTER_CTX filter;

    // Filter by hook
    filter.name = name;
    filter.value = value;
    filter.ignored = 0;
    HookMgrExecute(GetBootStageHookMgr(), INIT_PARAM_LOAD_FILTER, (void *)&filter, NULL);

    if (filter.ignored) {
        PARAM_LOGV("Default parameter [%s] [%s] ignored", name, value);
        return 0;
    }
#endif

    PARAM_LOGV("Add default parameter [%s] [%s]", name, value);
    return WriteParam(name, value, NULL, mode & LOAD_PARAM_ONLY_ADD);
}

static int LoadFileFromImport(char *target, uint32_t mode)
{
    if (strstr(target, ".para.dac")) {
        LoadSecurityLabel(target);
    } else {
        LoadDefaultParams(target, mode);
    }
    return 0;
}

// Content format of .import.para is "import /dir/param.para"
// Use ${} to pass parameter like "import /dir/${const.product.productid}.para"
static int LoadParamFromImport_(char *buffer, const int buffSize, uint32_t mode)
{
    int spaceCount = 0;
    while (*(buffer + IMPORT_PREFIX_LEN + spaceCount) == ' ') {
        spaceCount++;
    }
    char *target = calloc(PATH_MAX, 1);
    PARAM_CHECK(target != NULL, return -1, "Failed to alloc memory");
    if (strncpy_s(target, buffSize, buffer + IMPORT_PREFIX_LEN + spaceCount, buffSize) != 0) {
        PARAM_LOGE("Failed to get value of import.");
        free(target);
        return -1;
    }
    char *tmp = NULL;
    if ((tmp = strstr(target, "\n"))) {
        *tmp = '\0';
    }
    char *tmpParamValue = calloc(PARAM_VALUE_LEN_MAX + 1, sizeof(char));
    if (tmpParamValue == NULL) {
        PARAM_LOGE("Failed to alloc memory");
        free(target);
        return -1;
    }
    int ret = GetParamValue(target, strlen(target), tmpParamValue, PARAM_VALUE_LEN_MAX);
    if (ret == 0) {
        LoadFileFromImport(tmpParamValue, mode);
    }
    PARAM_LOGI("Load params from import %s return %d.", tmpParamValue, ret);
    free(tmpParamValue);
    free(target);
    return ret;
}

static int LoadParamFromImport(const char *fileName, void *context)
{
    char realPath[PATH_MAX] = "";
    realpath(fileName, realPath);
    FILE *fp = fopen(realPath, "r");
    if (fp == NULL) {
        PARAM_LOGE("Failed to open file '%s' error:%d ", fileName, errno);
        return -1;
    }

    const int buffSize = PATH_MAX;
    char *buffer = malloc(buffSize);
    PARAM_CHECK(buffer != NULL, (void)fclose(fp);
        return -1, "Failed to alloc memory");

    uint32_t mode = *(int *)context;
    while (fgets(buffer, buffSize, fp) != NULL) {
        buffer[buffSize - 1] = '\0';
        if (!strncmp(buffer, "import ", IMPORT_PREFIX_LEN)) {
            (void)LoadParamFromImport_(buffer, buffSize, mode);
        }
    }
    (void)fclose(fp);
    free(buffer);
    return 0;
}

static int LoadDefaultParam_(const char *fileName, uint32_t mode,
    const char *exclude[], uint32_t count, int (*loadOneParam)(const uint32_t *, const char *, const char *))
{
    uint32_t paramNum = 0;
    char realPath[PATH_MAX] = "";
    realpath(fileName, realPath);
    FILE *fp = fopen(realPath, "r");
    if (fp == NULL) {
        PARAM_LOGW("Failed to open file '%s' error:%d ", fileName, errno);
        return -1;
    }

    const int buffSize = PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX + 10;  // 10 max len
    char *buffer = malloc(buffSize);
    PARAM_CHECK(buffer != NULL, (void)fclose(fp);
        return -1, "Failed to alloc memory");

    while (fgets(buffer, buffSize, fp) != NULL) {
        buffer[buffSize - 1] = '\0';
        int ret = SplitParamString(buffer, exclude, count, loadOneParam, &mode);
        PARAM_ONLY_CHECK(ret != PARAM_DEFAULT_PARAM_MEMORY_NOT_ENOUGH, return PARAM_DEFAULT_PARAM_MEMORY_NOT_ENOUGH);
        PARAM_CHECK(ret == 0, continue, "Failed to set param '%s' error:%d ", buffer, ret);
        paramNum++;
    }
    (void)fclose(fp);
    free(buffer);
    PARAM_LOGV("Load %u default parameters success from %s.", paramNum, fileName);
    return 0;
}

static int ProcessParamFile(const char *fileName, void *context)
{
    static const char *exclude[] = {"ctl.", "selinux.restorecon_recursive"};
    uint32_t mode = *(int *)context;
    int ret = LoadDefaultParam_(fileName, mode, exclude, ARRAY_LENGTH(exclude), LoadOneParam_);
    if (ret == PARAM_DEFAULT_PARAM_MEMORY_NOT_ENOUGH) {
        PARAM_LOGE("[startup_failed]default_param memory is not enough, system reboot! %d",SYS_PARAM_INIT_FAILED);
        ExecReboot("panic");
    }
    return ret;
}

int LoadParamsFile(const char *fileName, bool onlyAdd)
{
    return LoadDefaultParams(fileName, onlyAdd ? LOAD_PARAM_ONLY_ADD : LOAD_PARAM_NORMAL);
}

int LoadDefaultParams(const char *fileName, uint32_t mode)
{
    PARAM_CHECK(fileName != NULL, return -1, "Invalid filename for load");
    PARAM_LOGI("Load default parameters from %s.", fileName);
    struct stat st;
    if ((stat(fileName, &st) == 0) && !S_ISDIR(st.st_mode)) {
        if (strstr(fileName, ".para.dac")) {
            return LoadSecurityLabel(fileName);
        } else {
            return ProcessParamFile(fileName, &mode);
        }
    } else {
        (void)ReadFileInDir(fileName, ".para", ProcessParamFile, &mode);
        (void)ReadFileInDir(fileName, ".para.import", LoadParamFromImport, &mode);
        return LoadSecurityLabel(fileName);
    }
}

INIT_LOCAL_API void LoadParamFromBuild(void)
{
    PARAM_LOGI("load parameters from build ");
#ifdef INCREMENTAL_VERSION
    if (strlen(INCREMENTAL_VERSION) > 0) {
        WriteParam("const.product.incremental.version", INCREMENTAL_VERSION, NULL, LOAD_PARAM_NORMAL);
    }
#endif
#ifdef BUILD_TYPE
    if (strlen(BUILD_TYPE) > 0) {
        WriteParam("const.product.build.type", BUILD_TYPE, NULL, LOAD_PARAM_NORMAL);
    }
#endif
#ifdef BUILD_USER
    if (strlen(BUILD_USER) > 0) {
        WriteParam("const.product.build.user", BUILD_USER, NULL, LOAD_PARAM_NORMAL);
    }
#endif
#ifdef BUILD_TIME
    if (strlen(BUILD_TIME) > 0) {
        WriteParam("const.product.build.date", BUILD_TIME, NULL, LOAD_PARAM_NORMAL);
    }
#endif
#ifdef BUILD_HOST
    if (strlen(BUILD_HOST) > 0) {
        WriteParam("const.product.build.host", BUILD_HOST, NULL, LOAD_PARAM_NORMAL);
    }
#endif
#ifdef BUILD_ROOTHASH
    if (strlen(BUILD_ROOTHASH) > 0) {
        WriteParam("const.ohos.buildroothash", BUILD_ROOTHASH, NULL, LOAD_PARAM_NORMAL);
    }
#endif
}

static int LoadOneParamAreaSize_(const uint32_t *context, const char *name, const char *value)
{
    uint32_t size = (uint32_t)strtoul(value, NULL, DECIMAL_BASE);
    PARAM_LOGV("LoadOneParamAreaSize_ [%s] [%s]", name, value);
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL && paramSpace->workSpace != NULL,
        return -1, "Invalid workspace name %s", name);
    WorkSpaceSize *spaceSize = GetWorkSpaceSize(GetWorkSpace(WORKSPACE_INDEX_SIZE));
    PARAM_CHECK(spaceSize != NULL, return PARAM_CODE_ERROR, "Failed to get workspace size");
    static char buffer[SELINUX_CONTENT_LEN] = {0};
    int ret = snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, "u:object_r:%s:s0", name);
    PARAM_CHECK(ret > 0, return PARAM_CODE_ERROR, "Failed to snprintf workspace name");

    for (uint32_t i = WORKSPACE_INDEX_BASE + 1; i < spaceSize->maxLabelIndex; i++) {
        if (paramSpace->workSpace[i] == NULL) {
            continue;
        }
        if (strcmp(paramSpace->workSpace[i]->fileName, buffer) == 0) {
            spaceSize->spaceSize[i] = size;
            paramSpace->workSpace[i]->spaceSize = size;
            break;
        }
    }
    return 0;
}

INIT_LOCAL_API void LoadParamAreaSize(void)
{
    LoadDefaultParam_("/sys_prod/etc/param/ohos.para.size", 0, NULL, 0, LoadOneParamAreaSize_);
    LoadDefaultParam_(PARAM_AREA_SIZE_CFG, 0, NULL, 0, LoadOneParamAreaSize_);
}
