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
#include <limits.h>

#include "param_manager.h"
#include "param_trie.h"

static int LoadSecurityLabel(const char *fileName)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return -1, "Invalid space");
    PARAM_CHECK(fileName != NULL, return -1, "Invalid fielname for load");
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
        PARAM_LOGI("Add param from cmdline %s %s", name, value);
        ret = CheckParamName(name, 0);
        PARAM_CHECK(ret == 0, return ret, "Invalid name %s", name);
        PARAM_LOGV("**** name %s, value %s", name, value);
        ret = WriteParam(name, value, NULL, 0);
        PARAM_CHECK(ret == 0, return ret, "Failed to write param %s %s", name, value);
    } else {
        PARAM_LOGE("Can not find arrt %s", name);
    }
    return ret;
}

static int SnDealFun(const char *name, const char *value, int res)
{
#ifdef USE_MTK_EMMC
    static const char SN_FILE[] = {"/proc/bootdevice/cid"};
#else
    static const char SN_FILE[] = {"/sys/block/mmcblk0/device/cid"};
#endif
    int ret = CheckParamName(name, 0);
    PARAM_CHECK(ret == 0, return ret, "Invalid name %s", name);
    char *data = NULL;
    if (res != 0) {  // if cmdline not set sn or set sn value is null,read sn from default file
        data = ReadFileData(SN_FILE);
        if (data == NULL) {
            PARAM_LOGE("Error, Read sn from default file failed!");
            return -1;
        }
    } else if (value[0] == '/') {
        data = ReadFileData(value);
        if (data == NULL) {
            PARAM_LOGE("Error, Read sn from cmdline file failed!");
            return -1;
        }
    } else {
        PARAM_LOGV("**** name %s, value %s", name, value);
        ret = WriteParam(name, value, NULL, 0);
        PARAM_CHECK(ret == 0, return ret, "Failed to write param %s %s", name, value);
        return ret;
    }

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
    ret = WriteParam(name, data, NULL, 0);
    PARAM_CHECK(ret == 0, free(data);
        return ret, "Failed to write param %s %s", name, data);
    free(data);

    return ret;
}

INIT_LOCAL_API int LoadParamFromCmdLine(void)
{
    int ret;
    static const cmdLineInfo cmdLines[] = {
        {OHOS_BOOT"hardware", CommonDealFun
        },
        {OHOS_BOOT"bootgroup", CommonDealFun
        },
        {OHOS_BOOT"reboot_reason", CommonDealFun
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

static int LoadDefaultParam_(const char *fileName, uint32_t mode, const char *exclude[], uint32_t count)
{
    uint32_t paramNum = 0;
    FILE *fp = fopen(fileName, "r");
    if (fp == NULL) {
        return -1;
    }
    const int buffSize = PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX + 10;  // 10 max len
    char *buffer = malloc(buffSize);
    if (buffer == NULL) {
        (void)fclose(fp);
        return -1;
    }
    while (fgets(buffer, buffSize, fp) != NULL) {
        buffer[buffSize - 1] = '\0';
        int ret = SpliteString(buffer, exclude, count, LoadOneParam_, &mode);
        PARAM_CHECK(ret == 0, continue, "Failed to set param '%s' error:%d ", buffer, ret);
        paramNum++;
    }
    (void)fclose(fp);
    free(buffer);
    PARAM_LOGI("Load parameters success %s total %u", fileName, paramNum);
    return 0;
}

static int ProcessParamFile(const char *fileName, void *context)
{
    static const char *exclude[] = {"ctl.", "selinux.restorecon_recursive"};
    uint32_t mode = *(int *)context;
    return LoadDefaultParam_(fileName, mode, exclude, ARRAY_LENGTH(exclude));
}

int LoadDefaultParams(const char *fileName, uint32_t mode)
{
    PARAM_CHECK(fileName != NULL, return -1, "Invalid filename for load");
    PARAM_LOGI("load default parameters %s.", fileName);
    int ret = 0;
    struct stat st;
    if ((stat(fileName, &st) == 0) && !S_ISDIR(st.st_mode)) {
        ret = ProcessParamFile(fileName, &mode);
    } else {
        ret = ReadFileInDir(fileName, ".para", ProcessParamFile, &mode);
    }

    // load security label
    return LoadSecurityLabel(fileName);
}

INIT_LOCAL_API void LoadParamFromBuild(void)
{
    PARAM_LOGI("load parameters from build ");
#ifdef INCREMENTAL_VERSION
    WriteParam("const.product.incremental.version", INCREMENTAL_VERSION, NULL, LOAD_PARAM_NORMAL);
#endif
#ifdef BUILD_TYPE
    WriteParam("const.product.build.type", BUILD_TYPE, NULL, LOAD_PARAM_NORMAL);
#endif
#ifdef BUILD_USER
    WriteParam("const.product.build.user", BUILD_USER, NULL, LOAD_PARAM_NORMAL);
#endif
#ifdef BUILD_TIME
    PARAM_LOGI("const.product.build.date %s", BUILD_TIME);
    WriteParam("const.product.build.date", BUILD_TIME, NULL, LOAD_PARAM_NORMAL);
#endif
#ifdef BUILD_HOST
    WriteParam("const.product.build.host", BUILD_HOST, NULL, LOAD_PARAM_NORMAL);
#endif
#ifdef BUILD_ROOTHASH
    WriteParam("const.ohos.buildroothash", BUILD_ROOTHASH, NULL, LOAD_PARAM_NORMAL);
#endif
}

