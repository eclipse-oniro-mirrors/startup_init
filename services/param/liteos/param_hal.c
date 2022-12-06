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
static ParamMutex g_saveMutex = {0};
#ifdef PARAM_SUPPORT_POSIX
#define MODE_READ O_RDONLY
#define MODE_APPEND (O_RDWR | O_CREAT | O_APPEND)
#define MODE_CREATE (O_RDWR | O_CREAT | O_TRUNC)
#else
#define MODE_READ O_RDONLY_FS
#define MODE_APPEND (O_RDWR_FS | O_CREAT_FS | O_APPEND_FS)
#define MODE_CREATE (O_RDWR_FS | O_CREAT_FS | O_TRUNC_FS)
#endif

static int ParamFileOpen(const char* path, int oflag, int mode)
{
#ifdef PARAM_SUPPORT_POSIX
    return open(path, oflag, mode);
#else
    return UtilsFileOpen(path, oflag, mode);
#endif
}

static int ParamFileClose(int fd)
{
#ifdef PARAM_SUPPORT_POSIX
    return close(fd);
#else
    return UtilsFileClose(fd);
#endif
}

static int ParamFileRead(int fd, char* buf, unsigned int len)
{
#ifdef PARAM_SUPPORT_POSIX
    return read(fd, buf, len);
#else
    return UtilsFileRead(fd, buf, len);
#endif
}

static int ParamFileWrite(int fd, const char* buf, unsigned int len)
{
#ifdef PARAM_SUPPORT_POSIX
    return write(fd, buf, len);
#else
    return UtilsFileWrite(fd, buf, len);
#endif
}

static int ParamFileDelete(const char* path)
{
#ifdef PARAM_SUPPORT_POSIX
    return unlink(path);
#else
    return UtilsFileDelete(path);
#endif
}

static int ParamFileStat(const char* path, unsigned int* fileSize)
{
#ifdef PARAM_SUPPORT_POSIX
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
	*fileSize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    return 0;
#else
    return UtilsFileStat(path, fileSize);
#endif
}

static void ParamFileSync(int ft)
{
#ifdef PARAM_SUPPORT_POSIX
    fsync(ft);
#else
    (void)ft;
#endif
}

static int LoadOnePersistParam_(const uint32_t *context, const char *name, const char *value)
{
    (void)context;
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
        fd = ParamFileOpen(path, MODE_READ, 0);
        if (fd < 0) {
            path = PARAM_PERSIST_SAVE_PATH;
            fd = ParamFileOpen(path, MODE_READ, 0);
            PARAM_LOGI("LoadPersistParam open file %s", path);
        }
        PARAM_CHECK(fd >= 0, break, "No valid persist parameter file %s", path);
        // read file
        uint32_t fileSize = 0;
        int ret = ParamFileStat(path, &fileSize);
        PARAM_CHECK(ret == 0, break, "Failed to get file state %s", path);
        buffer = malloc(fileSize);
        PARAM_CHECK(buffer != NULL, break, "Failed to get file");
        ret = ParamFileRead(fd, buffer, fileSize);
        PARAM_CHECK(ret >= 0, break, "Failed to read file %s", path);

        uint32_t currLen = 0;
        char *tmp = buffer;
        while (currLen < fileSize) {
            if (buffer[currLen] == '\n') { // split line
                buffer[currLen] = '\0';
                ret = SplitParamString(tmp, NULL, 0, LoadOnePersistParam_, NULL);
                PARAM_CHECK(ret == 0, continue, "Failed to set param %d %s", ret, buffer);
                paramNum++;
                if (currLen + 1 >= fileSize) {
                    break;
                }
                tmp = buffer + currLen + 1;
            }
            currLen++;
        }
    } while (0);
    if (fd > 0) {
        ParamFileClose(fd);
    }
    if (buffer != NULL) {
        free(buffer);
    }
    PARAM_LOGI("LoadPersistParam paramNum %d", paramNum);
    return 0;
}

static int PersistWrite(int fd, const char *name, const char *value)
{
    int ret = ParamFileWrite(fd, name, strlen(name));
    if (ret <= 0) {
        PARAM_LOGE("Failed to save persist param %s", name);
    }
    ret = ParamFileWrite(fd, "=", strlen("="));
    if (ret <= 0) {
        PARAM_LOGE("Failed to save persist param %s", name);
    }
    ret = ParamFileWrite(fd, value, strlen(value));
    if (ret <= 0) {
        PARAM_LOGE("Failed to save persist param %s", name);
    }
    ret = ParamFileWrite(fd, "\n", strlen("\n"));
    if (ret <= 0) {
        PARAM_LOGE("Failed to save persist param %s", name);
    }
    return 0;
}

static int SavePersistParam(const char *name, const char *value)
{
    ParamMutexPend(&g_saveMutex);
    int ret = -1;
    int fd = ParamFileOpen(PARAM_PERSIST_SAVE_PATH, MODE_APPEND, 0);
    if (fd > 0) {
        ret = PersistWrite(fd, name, value);
        ParamFileSync(fd);
        ParamFileClose(fd);
    }
    if (ret != 0) {
        PARAM_LOGE("SavePersistParam %s errno %d", name, errno);
    }
    ParamMutexPost(&g_saveMutex);
    return ret;
}

static int BatchSavePersistParamBegin(PERSIST_SAVE_HANDLE *handle)
{
    ParamMutexPend(&g_saveMutex);
    int fd = ParamFileOpen(PARAM_PERSIST_SAVE_PATH, MODE_CREATE, 0);
    if (fd < 0) {
        ParamMutexPost(&g_saveMutex);
        PARAM_LOGE("Open file %s fail error %d", PARAM_PERSIST_SAVE_PATH, errno);
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
    ParamFileSync(fd);
    ret = ParamFileClose(fd);
    ParamMutexPost(&g_saveMutex);
    PARAM_CHECK(ret == 0, return, "BatchSavePersistParamEnd fail error %d", errno);
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
