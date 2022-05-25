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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "init_utils.h"
#include "param_manager.h"
#include "param_persist.h"
#include "param_utils.h"
#include "securec.h"
#include "utils_file.h"

// for linux, no mutex
static ParamMutex g_saveMutex = {};

static int LoadOnePersistParam_(const uint32_t *context, const char *name, const char *value)
{
    uint32_t dataIndex = 0;
    int ret = WriteParam(name, value, &dataIndex, 0);
    PARAM_CHECK(ret == 0, return ret, "Failed to write param %d name:%s %s", ret, name, value);
    return 0;
}

static int LoadPersistParam(void)
{
    const char *path = PARAM_PERSIST_SAVE_TMP_PATH;
    CheckAndCreateDir(path);
    char *buffer = NULL;
    int fd = -1;
    uint32_t paramNum = 0;
    do {
        fd = UtilsFileOpen(path, O_RDONLY_FS, 0);
        if (fd < 0) {
            path = PARAM_PERSIST_SAVE_PATH;
            fd = UtilsFileOpen(path, O_RDONLY_FS, 0);
            PARAM_LOGI("LoadPersistParam open file %s", path);
        }
        PARAM_CHECK(fd >= 0, break, "No valid persist parameter file %s", path);
        // read file
        uint32_t fileSize = 0;
        int ret = UtilsFileStat(path, &fileSize);
        PARAM_CHECK(ret == 0, break, "Failed to get file state %s", path);
        buffer = malloc(fileSize);
        PARAM_CHECK(buffer != NULL, break, "Failed to get file");
        ret = UtilsFileRead(fd, buffer, fileSize);
        PARAM_CHECK(ret == 0, break, "Failed to get read file %s", path);

        uint32_t currLen = 0;
        while (currLen < fileSize) {
            if (buffer[currLen] == '\n') { // splite line
                buffer[currLen] = '\0';
                int ret = SpliteString(buffer, NULL, 0, LoadOnePersistParam_, NULL);
                PARAM_CHECK(ret == 0, continue, "Failed to set param %d %s", ret, buffer);
                paramNum++;
            }
            currLen++;
        }
    } while (0);
    if (fd > 0) {
        UtilsFileClose(fd);
    }
    if (buffer != NULL) {
        free(buffer);
    }
    PARAM_LOGI("LoadPersistParam from file %s paramNum %d", path, paramNum);
    return 0;
}

static int PersistWrite(int fd, const char *name, const char *value)
{
    int ret = UtilsFileWrite(fd, name, strlen(name));
    if (ret <= 0) {
        PARAM_LOGE("Failed to save persist param %s", name);
    }
    ret = UtilsFileWrite(fd, "=", strlen("="));
    if (ret <= 0) {
        PARAM_LOGE("Failed to save persist param %s", name);
    }
    ret = UtilsFileWrite(fd, value, strlen(value));
    if (ret <= 0) {
        PARAM_LOGE("Failed to save persist param %s", name);
    }
    ret = UtilsFileWrite(fd, "\n", strlen("\n"));
    if (ret <= 0) {
        PARAM_LOGE("Failed to save persist param %s", name);
    }
    return 0;
}

static int SavePersistParam(const char *name, const char *value)
{
    ParamMutexPend(&g_saveMutex);
    int ret = -1;
    int fd = UtilsFileOpen(PARAM_PERSIST_SAVE_PATH, O_RDWR_FS | O_CREAT_FS | O_APPEND_FS, 0);
    if (fd > 0) {
        ret = PersistWrite(fd, name, value);
        UtilsFileClose(fd);
    }
    ParamMutexPost(&g_saveMutex);
    return ret;
}

static int BatchSavePersistParamBegin(PERSIST_SAVE_HANDLE *handle)
{
    ParamMutexPend(&g_saveMutex);
    int fd = UtilsFileOpen(PARAM_PERSIST_SAVE_TMP_PATH, O_RDWR_FS | O_CREAT_FS | O_TRUNC_FS, 0);
    if (fd < 0) {
        ParamMutexPost(&g_saveMutex);
        PARAM_LOGE("Open file %s fail error %d", PARAM_PERSIST_SAVE_TMP_PATH, errno);
        return -1;
    }
    *handle = (PERSIST_SAVE_HANDLE)fd;
    return 0;
}

static int BatchSavePersistParam(PERSIST_SAVE_HANDLE handle, const char *name, const char *value)
{
    int fd = (int)handle;
    int ret = PersistWrite(fd, name, value);
    PARAM_CHECK(ret == 0, return -1, "Failed to write param %s", name);
    PARAM_LOGV("BatchSavePersistParam %s=%s", name, value);
    return 0;
}

static void BatchSavePersistParamEnd(PERSIST_SAVE_HANDLE handle)
{
    int ret;
    int fd = (int)handle;
    UtilsFileClose(fd);
    UtilsFileDelete(PARAM_PERSIST_SAVE_PATH);
    ret = UtilsFileMove(PARAM_PERSIST_SAVE_TMP_PATH, PARAM_PERSIST_SAVE_PATH);
    ParamMutexPost(&g_saveMutex);
    PARAM_CHECK(ret == 0, return, "BatchSavePersistParamEnd %s fail error %d", PARAM_PERSIST_SAVE_TMP_PATH, errno);
}

int RegisterPersistParamOps(PersistParamOps *ops)
{
    PARAM_LOGI("RegisterPersistParamOps");
    ParamMutexCeate(&g_saveMutex);
    PARAM_CHECK(ops != NULL, return -1, "Invalid ops");
    ops->save = SavePersistParam;
    ops->load = LoadPersistParam;
    ops->batchSaveBegin = BatchSavePersistParamBegin;
    ops->batchSave = BatchSavePersistParam;
    ops->batchSaveEnd = BatchSavePersistParamEnd;
    return 0;
}