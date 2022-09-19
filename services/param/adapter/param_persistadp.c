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
    uint32_t dataIndex = 0;
    int ret = WriteParam(name, value, &dataIndex, 0);
    PARAM_CHECK(ret == 0, return ret, "Failed to write param %d name:%s %s", ret, name, value);
    return 0;
}

static int LoadPersistParam()
{
    CheckAndCreateDir(PARAM_PERSIST_SAVE_PATH);
    int updaterMode = InUpdaterMode();
    char *tmpPath = (updaterMode == 0) ? PARAM_PERSIST_SAVE_TMP_PATH : "/param/tmp_persist_parameters";
    FILE *fp = fopen(tmpPath, "r");
    if (fp == NULL) {
        tmpPath = (updaterMode == 0) ? PARAM_PERSIST_SAVE_PATH : "/param/persist_parameters";
        fp = fopen(tmpPath, "r");
        PARAM_LOGI("Load persist param, from file %s", tmpPath);
    }
    PARAM_CHECK(fp != NULL, return -1, "No valid persist parameter file %s", PARAM_PERSIST_SAVE_PATH);

    const int buffSize = PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX + 10;  // 10 max len
    char *buffer = malloc(buffSize);
    if (buffer == NULL) {
        (void)fclose(fp);
        return -1;
    }
    uint32_t paramNum = 0;
    while (fgets(buffer, buffSize, fp) != NULL) {
        buffer[buffSize - 1] = '\0';
        int ret = SplitParamString(buffer, NULL, 0, LoadOnePersistParam_, NULL);
        PARAM_CHECK(ret == 0, continue, "Failed to set param %d %s", ret, buffer);
        paramNum++;
    }
    (void)fclose(fp);
    free(buffer);
    PARAM_LOGI("LoadPersistParam from file %s paramNum %d", tmpPath, paramNum);
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
    PARAM_CHECK(ret > 0, return -1, "Failed to write param");
    PARAM_LOGV("BatchSavePersistParam %s=%s", name, value);
    return 0;
}

static void BatchSavePersistParamEnd(PERSIST_SAVE_HANDLE handle)
{
    int ret;
    FILE *fp = (FILE *)handle;
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