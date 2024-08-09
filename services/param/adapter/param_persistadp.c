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

#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "init_utils.h"
#include "param_manager.h"
#include "param_persist.h"
#include "param_utils.h"

// for linux, no mutex
static ParamMutex g_saveMutex = {};

static int LoadOnePersistParam_(const uint32_t *context, const char *name, const char *value)
{
    bool clearFactoryPersistParams = *(bool*)context;
    uint32_t dataIndex = 0;
    int mode = 0;

    if (!clearFactoryPersistParams) {
        mode |= LOAD_PARAM_PERSIST;
        return WriteParam(name, value, &dataIndex, mode);
    }

    char persetValue[PARAM_VALUE_LEN_MAX] = {0};
    uint32_t len = PARAM_VALUE_LEN_MAX;
    int ret = SystemReadParam(name, persetValue, &len);
    if (ret != 0) {
        mode |= LOAD_PARAM_PERSIST;
        return WriteParam(name, value, &dataIndex, mode);
    }

    if ((strcmp(persetValue, value) != 0)) {
        PARAM_LOGI("%s value is different, preset value is:%s, persist value is:%s", name, persetValue, value);
        mode |= LOAD_PARAM_PERSIST;
        return WriteParam(name, value, &dataIndex, mode);
    }

    return 0;
}

static void LoadPersistParam_(const bool clearFactoryPersistParams, const char *fileName,
    char *buffer, uint32_t buffSize)
{
    FILE *fp = fopen(fileName, "r");
    PARAM_WARNING_CHECK(fp != NULL, return, "No valid persist parameter file %s", fileName);

    uint32_t paramNum = 0;
    while (fgets(buffer, buffSize, fp) != NULL) {
        buffer[buffSize - 1] = '\0';
        int ret = SplitParamString(buffer, NULL, 0, LoadOnePersistParam_, (uint32_t*)&clearFactoryPersistParams);
        PARAM_CHECK(ret == 0, continue, "Failed to set param %d %s", ret, buffer);
        paramNum++;
    }
    (void)fclose(fp);
    PARAM_LOGI("LoadPersistParam from file %s paramNum %d", fileName, paramNum);
}

static int LoadPersistParam(void)
{
    CheckAndCreateDir(PARAM_PERSIST_SAVE_PATH);
    bool clearFactoryPersistParams = false;
    char value[PARAM_VALUE_LEN_MAX] = {0};
    uint32_t len = PARAM_VALUE_LEN_MAX;
    int ret = SystemReadParam("const.startup.param.version", value, &len);
    if ((ret != 0 || strcmp(value, "1") != 0) &&
        (access(PERSIST_PARAM_FIXED_FLAGS, F_OK) != 0)) {
        clearFactoryPersistParams = true;
    }
    const uint32_t buffSize = PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX + 10;  // 10 max len
    char *buffer = malloc(buffSize);
    PARAM_CHECK(buffer != NULL, return -1, "Failed to alloc");

    int updaterMode = InUpdaterMode();
    char *tmpPath = (updaterMode == 0) ? PARAM_PERSIST_SAVE_PATH : "/param/persist_parameters";
    LoadPersistParam_(clearFactoryPersistParams, tmpPath, buffer, buffSize);
    tmpPath = (updaterMode == 0) ? PARAM_PERSIST_SAVE_TMP_PATH : "/param/tmp_persist_parameters";
    LoadPersistParam_(clearFactoryPersistParams, tmpPath, buffer, buffSize);
    free(buffer);
    if (clearFactoryPersistParams && access(PARAM_PERSIST_SAVE_PATH, F_OK) == 0) {
        FILE *fp = fopen(PERSIST_PARAM_FIXED_FLAGS, "w");
        PARAM_CHECK(fp != NULL, return -1, "create file %s fail error %d", PERSIST_PARAM_FIXED_FLAGS, errno);
        (void)fclose(fp);
    }
    return 0;
}

static int SavePersistParam(const char *name, const char *value)
{
    ParamMutexPend(&g_saveMutex);
    char *path = (InUpdaterMode() == 0) ? PARAM_PERSIST_SAVE_PATH : "/param/persist_parameters";
    FILE *fp = fopen(path, "a+");
    int ret = -1;
    if (fp != NULL) {
        ret = fprintf(fp, "%s=%s\n", name, value);
        (void)fclose(fp);
    }
    ParamMutexPost(&g_saveMutex);
    if (ret <= 0) {
        PARAM_LOGE("Failed to save persist param %s", name);
    }
    return ret;
}

static int BatchSavePersistParamBegin(PERSIST_SAVE_HANDLE *handle)
{
    ParamMutexPend(&g_saveMutex);
    char *path = (InUpdaterMode() == 0) ? PARAM_PERSIST_SAVE_TMP_PATH : "/param/tmp_persist_parameters";
    unlink(path);
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        ParamMutexPost(&g_saveMutex);
        PARAM_LOGE("Open file %s fail error %d", path, errno);
        return -1;
    }
    *handle = (PERSIST_SAVE_HANDLE)fp;
    return 0;
}

static int BatchSavePersistParam(PERSIST_SAVE_HANDLE handle, const char *name, const char *value)
{
    FILE *fp = (FILE *)handle;
    int ret = fprintf(fp, "%s=%s\n", name, value);
    PARAM_LOGV("BatchSavePersistParam %s=%s", name, value);
    return (ret > 0) ? 0 : -1;
}

static void BatchSavePersistParamEnd(PERSIST_SAVE_HANDLE handle)
{
    int ret;
    FILE *fp = (FILE *)handle;
    (void)fflush(fp);
    (void)fsync(fileno(fp));
    (void)fclose(fp);
    if (InUpdaterMode() == 0) {
        unlink(PARAM_PERSIST_SAVE_PATH);
        ret = rename(PARAM_PERSIST_SAVE_TMP_PATH, PARAM_PERSIST_SAVE_PATH);
    } else {
        unlink("/param/persist_parameters");
        ret = rename("/param/tmp_persist_parameters", "/param/persist_parameters");
    }
    ParamMutexPost(&g_saveMutex);
    PARAM_CHECK(ret == 0, return, "BatchSavePersistParamEnd %s fail error %d", PARAM_PERSIST_SAVE_TMP_PATH, errno);
}

int RegisterPersistParamOps(PersistParamOps *ops)
{
    ParamMutexCreate(&g_saveMutex);
    PARAM_CHECK(ops != NULL, return -1, "Invalid ops");
    ops->save = SavePersistParam;
    ops->load = LoadPersistParam;
    ops->batchSaveBegin = BatchSavePersistParamBegin;
    ops->batchSave = BatchSavePersistParam;
    ops->batchSaveEnd = BatchSavePersistParamEnd;
    return 0;
}