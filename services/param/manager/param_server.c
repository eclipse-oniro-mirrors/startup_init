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

static int GetParamValueFromBuffer(const char *name, const char *buffer, char *value, int length)
{
    size_t bootLen = strlen(OHOS_BOOT);
    const char *tmpName = name + bootLen;
    int ret = GetProcCmdlineValue(tmpName, buffer, value, length);
    return ret;
}

static int CommonDealFun(const char *name, const char *value, int res)
{
    int ret = 0;
    if (res == 0) {
        PARAM_LOGV("Add param from cmdline %s %s", name, value);
        ret = CheckParamName(name, 0);
        PARAM_CHECK(ret == 0, return ret, "Invalid param name %s", name);
        PARAM_LOGV("Param name %s, value %s", name, value);
        ret = WriteParam(name, value, NULL, 0);
        PARAM_CHECK(ret == 0, return ret, "Failed to write param %s %s", name, value);
    } else {
        PARAM_LOGE("Get %s parameter value is null.", name);
    }
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

static int SnDealFun(const char *name, const char *value, int res)
{
    const char *snFileList [] = {
        "/sys/block/mmcblk0/device/cid",
        "/proc/bootdevice/cid"
    };
    int ret = CheckParamName(name, 0);
    PARAM_CHECK(ret == 0, return ret, "Invalid name %s", name);
    if (value != NULL && res == 0 && value[0] != '/') {
        PARAM_LOGV("**** name %s, value %s", name, value);
        ret = WriteParam(name, value, NULL, 0);
        PARAM_CHECK(ret == 0, return ret, "Failed to write param %s %s", name, value);
        return ret;
    }
    if (value != NULL && value[0] == '/') {
        ret = ReadSnFromFile(name, value);
        if (ret == 0) {
            return ret;
        }
    }
    for (size_t i = 0; i < ARRAY_LENGTH(snFileList); i++) {
        ret = ReadSnFromFile(name, snFileList[i]);
        if (ret == 0) {
            break;
        }
    }
    return ret;
}

INIT_LOCAL_API int LoadParamFromCmdLine(void)
{
    static const cmdLineInfo cmdLines[] = {
        {OHOS_BOOT"hardware", CommonDealFun
        },
        {OHOS_BOOT"bootgroup", CommonDealFun
        },
        {OHOS_BOOT"reboot_reason", CommonDealFun
        },
        {OHOS_BOOT"bootslots", CommonDealFun
        },
        {OHOS_BOOT"sn", SnDealFun
        }
    };
    char *data = ReadFileData(BOOT_CMD_LINE);
    PARAM_CHECK(data != NULL, return -1, "Failed to read file %s", BOOT_CMD_LINE);
    char *value = calloc(1, PARAM_CONST_VALUE_LEN_MAX + 1);
    PARAM_CHECK(value != NULL, free(data);
        return -1, "Failed to read file %s", BOOT_CMD_LINE);

    for (size_t i = 0; i < ARRAY_LENGTH(cmdLines); i++) {
        int ret = 0;
#ifdef BOOT_EXTENDED_CMDLINE
        ret = GetParamValueFromBuffer(cmdLines[i].name, BOOT_EXTENDED_CMDLINE, value, PARAM_CONST_VALUE_LEN_MAX);
        if (ret != 0) {
            ret = GetParamValueFromBuffer(cmdLines[i].name, data, value, PARAM_CONST_VALUE_LEN_MAX);
        }
#else
        ret = GetParamValueFromBuffer(cmdLines[i].name, data, value, PARAM_CONST_VALUE_LEN_MAX);
#endif

        cmdLines[i].processor(cmdLines[i].name, value, ret);
    }
    PARAM_LOGV("Parse cmdline finish %s", BOOT_CMD_LINE);
    free(data);
    free(value);
    return 0;
}

static int LoadOneParam_(const uint32_t *context, const char *name, const char *value)
{
    uint32_t mode = *(uint32_t *)context;
    int ret = CheckParamName(name, 0);
    if (ret != 0) {
        return 0;
    }
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
    if (buffer == NULL) {
        (void)fclose(fp);
        return -1;
    }
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

    WorkSpace *workSpace = GetWorkSpace(WORKSPACE_NAME_DAC);
    ParamTrieNode *node = AddTrieNode(workSpace, name, strlen(name));
    PARAM_CHECK(node != NULL, return PARAM_CODE_REACHED_MAX, "Failed to add node");
    ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, node->dataIndex);
    if (entry == NULL) {
        uint32_t offset = AddParamNode(workSpace, PARAM_TYPE_INT, name, strlen(name), value, strlen(value));
        PARAM_CHECK(offset > 0, return PARAM_CODE_REACHED_MAX, "Failed to allocate name %s", name);
        SaveIndex(&node->dataIndex, offset);
    }
    return 0;
}

INIT_LOCAL_API void LoadParamAreaSize(void)
{
    LoadDefaultParam_(PARAM_AREA_SIZE_CFG, 0, NULL, 0, LoadOneParamAreaSize_);
}
