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

#include "param_manager.h"
#include "param_trie.h"
#ifdef SUPPORT_PARAM_LOAD_HOOK
#include "init_module_engine.h"
#endif

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
#define OHOS_CMDLINE_PARA_PREFIX_LEN    10

typedef struct cmdLineInfo {
    const char *name;
    int (*processor)(const char *name, const char *value);
} cmdLineInfo;

typedef struct cmdLineIteratorCtx {
    char *cmdline;
    bool gotSn;
} cmdLineIteratorCtx;

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

static void CmdlineIterator(const NAME_VALUE_PAIR *nv, void *context)
{
    int ret;
    const char *name;
    const char *matched;
    char fullName[PARAM_NAME_LEN_MAX];
    cmdLineIteratorCtx *ctx = (cmdLineIteratorCtx *)context;
    char *data = (char *)ctx->cmdline;
    static const cmdLineInfo cmdLines[] = {
        { "hardware", CommonDealFun },
        { "bootgroup", CommonDealFun },
        { "reboot_reason", CommonDealFun },
        { "bootslots", CommonDealFun },
        { "sn", SnDealFun },
        { "eng_mode", CommonDealFun },
        { "serialno", SnDealFun }
    };

    data[nv->name_end - data] = '\0';
    data[nv->value_end - data] = '\0';
    PARAM_LOGV("proc cmdline: name [%s], value [%s]", nv->name, nv->value);

    // Get name without prefix
    name = nv->name;
    if (strncmp(name, OHOS_CMDLINE_PARA_PREFIX, OHOS_CMDLINE_PARA_PREFIX_LEN) == 0) {
        name = name + OHOS_CMDLINE_PARA_PREFIX_LEN;
    }

    // Matching reserved cmdlines
    for (size_t i = 0; i < ARRAY_LENGTH(cmdLines); i++) {
        // Check exact match
        if (strcmp(name, cmdLines[i].name) != 0) {
            // Check if contains ".xxx" for compatibility
            ret = snprintf_s(fullName, sizeof(fullName), sizeof(fullName) - 1, ".%s", cmdLines[i].name);
            matched = strstr(name, fullName);
            if (matched == NULL) {
                continue;
            }
            // Check if it is ended with pattern
            if (matched[ret] != '\0') {
                continue;
            }
        }
        snprintf_s(fullName, sizeof(fullName), sizeof(fullName) - 1, OHOS_CMDLINE_PARA_PREFIX "%s", cmdLines[i].name);
        PARAM_LOGV("proc cmdline %s matched.", fullName);
        ret = cmdLines[i].processor(fullName, nv->value);
        if ((ret == 0) && (SnDealFun == cmdLines[i].processor)) {
            ctx->gotSn = true;
        }
        return;
    }

    if (name == nv->name) {
        return;
    }

    // cmdline with prefix but not matched, add to param by default
    PARAM_LOGE("add proc cmdline param %s by default.", nv->name);
    CommonDealFun(nv->name, nv->value);
}

static void GenerateSnByDefault() {
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
    cmdLineIteratorCtx ctx;

    ctx.gotSn = false;
    ctx.cmdline = ReadFileData(BOOT_CMD_LINE);
    PARAM_CHECK(ctx.cmdline != NULL, return -1, "Failed to read file %s", BOOT_CMD_LINE);

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
        ops->securityGetLabel(fileName);
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

static int LoadDefaultParam_(const char *fileName, uint32_t mode,
    const char *exclude[], uint32_t count, int (*loadOneParam)(const uint32_t *, const char *, const char *))
{
    uint32_t paramNum = 0;
    FILE *fp = fopen(fileName, "r");
    PARAM_CHECK(fp != NULL, return -1, "Failed to open file '%s' error:%d ", fileName, errno);

    const int buffSize = PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX + 10;  // 10 max len
    char *buffer = malloc(buffSize);
    PARAM_CHECK(buffer != NULL, (void)fclose(fp);
        return -1, "Failed to alloc memory");

    while (fgets(buffer, buffSize, fp) != NULL) {
        buffer[buffSize - 1] = '\0';
        int ret = SplitParamString(buffer, exclude, count, loadOneParam, &mode);
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
    return LoadDefaultParam_(fileName, mode, exclude, ARRAY_LENGTH(exclude), LoadOneParam_);
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
        (void)ProcessParamFile(fileName, &mode);
    } else {
        (void)ReadFileInDir(fileName, ".para", ProcessParamFile, &mode);
    }

    // load security label
    return LoadSecurityLabel(fileName);
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
    int ret = CheckParamName(name, 0);
    if (ret != 0) {
        return 0;
    }
    ret = CheckParamValue(NULL, name, value, PARAM_TYPE_INT);
    PARAM_CHECK(ret == 0, return 0, "Invalid value %s for %s", value, name);
    PARAM_LOGV("LoadOneParamAreaSize_ [%s] [%s]", name, value);
    char buffer[PARAM_NAME_LEN_MAX] = {0};
    int len = sprintf_s(buffer, sizeof(buffer), "const.%s", name);
    PARAM_CHECK(len > 0, return 0, "Failed to format value %s for %s", value, name);
    return AddParamEntry(WORKSPACE_INDEX_BASE, PARAM_TYPE_INT, buffer, value);
}

INIT_LOCAL_API void LoadParamAreaSize(void)
{
    LoadDefaultParam_("/sys_prod/etc/param/ohos.para.size", 0, NULL, 0, LoadOneParamAreaSize_);
    LoadDefaultParam_(PARAM_AREA_SIZE_CFG, 0, NULL, 0, LoadOneParamAreaSize_);
}
