/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include "param_persist.h"

#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "init_param.h"
#include "init_utils.h"
#include "param_manager.h"
#include "param_trie.h"

static ParamPersistWorkSpace g_persistWorkSpace = {0, 0, NULL, 0, {}};
static int IsNeedToSave(const char *name)
{
#if defined(__LITEOS_M__) || defined(__LITEOS_A__)
    return IS_READY_ONLY(name) ? 0 : 1;
#else
    return (strncmp(name, PARAM_PERSIST_PREFIX, strlen(PARAM_PERSIST_PREFIX)) == 0) ? 1 : 0;
#endif
}

static int SavePersistParam(const WorkSpace *workSpace, const ParamTrieNode *node, const void *cookie)
{
    ParamTrieNode *current = (ParamTrieNode *)node;
    if (current == NULL || current->dataIndex == 0) {
        return 0;
    }
    ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, current->dataIndex);
    if (entry == NULL) {
        return 0;
    }

    if (!IsNeedToSave(entry->data)) {
        return 0;
    }
    static char name[PARAM_NAME_LEN_MAX] = {0};
    int ret = memcpy_s(name, PARAM_NAME_LEN_MAX - 1, entry->data, entry->keyLength);
    PARAM_CHECK(ret == EOK, return -1, "Failed to read param name %s", entry->data);
    name[entry->keyLength] = '\0';
    ret = g_persistWorkSpace.persistParamOps.batchSave(
        (PERSIST_SAVE_HANDLE)cookie, name, entry->data + entry->keyLength + 1);
    PARAM_CHECK(ret == 0, return -1, "Failed to write param %s", current->key);
    return ret;
}

static int BatchSavePersistParam()
{
    PARAM_LOGV("BatchSavePersistParam");
    if (g_persistWorkSpace.persistParamOps.batchSaveBegin == NULL ||
        g_persistWorkSpace.persistParamOps.batchSave == NULL ||
        g_persistWorkSpace.persistParamOps.batchSaveEnd == NULL) {
        return 0;
    }

    PERSIST_SAVE_HANDLE handle;
    int ret = g_persistWorkSpace.persistParamOps.batchSaveBegin(&handle);
    PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_NAME, "Failed to save persist");
#if defined(__LITEOS_M__) || defined(__LITEOS_A__)
    const char *prefix = "";
#else
    const char *prefix = PARAM_PERSIST_PREFIX;
#endif
    // walk and save persist param
    WorkSpace *workSpace = GetFirstWorkSpace();
    while (workSpace != NULL) {
        WorkSpace *next = GetNextWorkSpace(workSpace);
        ParamTrieNode *root = FindTrieNode(workSpace, prefix, strlen(prefix), NULL);
        PARAMSPACE_AREA_RD_LOCK(workSpace);
        TraversalTrieNode(workSpace, root, SavePersistParam, (void *)handle);
        PARAMSPACE_AREA_RW_UNLOCK(workSpace);
        workSpace = next;
    }
    g_persistWorkSpace.persistParamOps.batchSaveEnd(handle);
    PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_NAME, "Save persist param fail");

    PARAM_CLEAR_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_UPDATE);
    (void)time(&g_persistWorkSpace.lastSaveTimer);
    return ret;
}

int InitPersistParamWorkSpace()
{
    if (PARAM_TEST_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_INIT)) {
        return 0;
    }
    (void)time(&g_persistWorkSpace.lastSaveTimer);
    RegisterPersistParamOps(&g_persistWorkSpace.persistParamOps);
    PARAM_SET_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_INIT);
    return 0;
}

void ClosePersistParamWorkSpace(void)
{
    if (g_persistWorkSpace.saveTimer != NULL) {
        ParamTimerClose(g_persistWorkSpace.saveTimer);
    }
    g_persistWorkSpace.flags = 0;
}

PARAM_STATIC void TimerCallbackForSave(const ParamTaskPtr timer, void *context)
{
    UNUSED(context);
    UNUSED(timer);
    // for liteos，we must cycle check
#ifndef PARAM_SUPPORT_CYCLE_CHECK
    ParamTimerClose(g_persistWorkSpace.saveTimer);
    g_persistWorkSpace.saveTimer = NULL;
    if (!PARAM_TEST_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_UPDATE)) {
        return;
    }
#else
    // check commit
    long long commit =  GetPersistCommitId();
    PARAM_LOGV("TimerCallbackForSave commit %lld %lld", commit, g_persistWorkSpace.commitId);
    if (g_persistWorkSpace.commitId == commit) {
        return;
    }
    g_persistWorkSpace.commitId = commit;
#endif
    (void)BatchSavePersistParam();
}

int WritePersistParam(const char *name, const char *value)
{
    PARAM_CHECK(value != NULL && name != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param");
    if (!IsNeedToSave(name)) {
        return 0;
    }
    PARAM_LOGV("WritePersistParam name %s ", name);
    if (g_persistWorkSpace.persistParamOps.save != NULL) {
        g_persistWorkSpace.persistParamOps.save(name, value);
    }
    // update commit for check
    UpdatePersistCommitId();

    if (!PARAM_TEST_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_LOADED)) {
        PARAM_LOGE("Can not save persist param before load %s ", name);
        return 0;
    }
    if (g_persistWorkSpace.persistParamOps.batchSave == NULL) {
        return 0;
    }

    // check timer for save all
    time_t currTimer;
    (void)time(&currTimer);
    uint32_t diff = Difftime(currTimer, g_persistWorkSpace.lastSaveTimer);
    if (diff > PARAM_MUST_SAVE_PARAM_DIFF) {
        if (g_persistWorkSpace.saveTimer != NULL) {
            ParamTimerClose(g_persistWorkSpace.saveTimer);
            g_persistWorkSpace.saveTimer = NULL;
        }
        return BatchSavePersistParam();
    }
#ifndef PARAM_SUPPORT_CYCLE_CHECK
    PARAM_SET_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_UPDATE);
    if (g_persistWorkSpace.saveTimer == NULL) {
        ParamTimerCreate(&g_persistWorkSpace.saveTimer, TimerCallbackForSave, NULL);
        ParamTimerStart(g_persistWorkSpace.saveTimer, PARAM_MUST_SAVE_PARAM_DIFF * MS_UNIT, MS_UNIT);
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

int LoadParamFromCmdLine(void)
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

void LoadParamFromBuild(void)
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

int LoadPersistParams(void)
{
    // get persist parameter
    int ret = InitPersistParamWorkSpace();
    PARAM_CHECK(ret == 0, return ret, "Failed to init persist param");
#ifndef STARTUP_INIT_TEST
    if (PARAM_TEST_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_LOADED)) {
        PARAM_LOGE("Persist param has been loaded");
        return 0;
    }
#endif
    if (g_persistWorkSpace.persistParamOps.load != NULL) {
        ret = g_persistWorkSpace.persistParamOps.load();
        PARAM_SET_FLAG(g_persistWorkSpace.flags, WORKSPACE_FLAGS_LOADED);
    }
    // save new persist param
    ret = BatchSavePersistParam();
    PARAM_CHECK(ret == 0, return ret, "Failed to load persist param");
#ifdef PARAM_SUPPORT_CYCLE_CHECK
    if (g_persistWorkSpace.saveTimer == NULL) {
        ParamTimerCreate(&g_persistWorkSpace.saveTimer, TimerCallbackForSave, NULL);
        ParamTimerStart(g_persistWorkSpace.saveTimer, PARAM_MUST_SAVE_PARAM_DIFF * MS_UNIT, MS_UNIT);
    }
#endif
    return 0;
}