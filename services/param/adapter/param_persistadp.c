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

static bool IsPrivateParam(const char *param)
{
    const char *privatePersistParams[] = {
        "persist.sys.data.dataextpath",
        "persist.sys.radio.vendorlib.path",
        "persist.sys.default_ime",
        "persist.sys.usb.config",
        "persist.hdc.daemon.cancel",
        "persist.hdc.daemon.auth_result",
        "persist.hdc.client.hostname",
        "persist.hdc.client.pubkey_sha256",
        "persist.kernel.bundle_name.clouddrive",
        "persist.kernel.bundle_name.photos",
        "persist.kernel.bundle_name.filemanager",
    };
    int size = sizeof(privatePersistParams) / sizeof(char*);
    for (int i = 0; i < size; i++) {
        if (strcmp(param, privatePersistParams[i]) == 0) {
            return true;
        }
    }
    return false;
}

static int LoadOnePublicPersistParam_(const uint32_t *context, const char *name, const char *value)
{
    if (IsPrivateParam(name)) {
        PARAM_LOGI("%s is private, ignore", name);
        return 0;
    }
    return LoadOnePersistParam_(context, name, value);
}

static void LoadPersistParam_(const bool clearFactoryPersistParams, const char *fileName,
    char *buffer, uint32_t buffSize)
{
    FILE *fp = fopen(fileName, "r");
    PARAM_WARNING_CHECK(fp != NULL, return, "No valid persist parameter file %s", fileName);
    bool isPublic = false;
    if (strcmp(fileName, PARAM_PUBLIC_PERSIST_SAVE_PATH) == 0 ||
        strcmp(fileName, PARAM_PUBLIC_PERSIST_SAVE_TMP_PATH) == 0) {
        isPublic = true;
    }
    int ret = 0;
    uint32_t paramNum = 0;
    while (fgets(buffer, buffSize, fp) != NULL) {
        buffer[buffSize - 1] = '\0';
        if (isPublic) {
            ret = SplitParamString(buffer, NULL, 0, LoadOnePublicPersistParam_, (uint32_t*)&clearFactoryPersistParams);
        } else {
            ret = SplitParamString(buffer, NULL, 0, LoadOnePersistParam_, (uint32_t*)&clearFactoryPersistParams);
        }
        PARAM_CHECK(ret == 0, continue, "Failed to set param %d %s", ret, buffer);
        paramNum++;
    }
    (void)fclose(fp);
    PARAM_LOGI("LoadPersistParam from file %s paramNum %d", fileName, paramNum);
}

static void GetPersistFilePath(char **path, char **tmpPath, int fileType)
{
    if (InUpdaterMode() == 1) {
        *path = "/param/persist_parameters";
        *tmpPath = "/param/tmp_persist_paramters";
        return;
    }
    if (fileType == PUBLIC_PERSIST_FILE) {
        if (access(PARAM_PERSIST_SAVE_PATH, F_OK) == 0 && access(PARAM_PUBLIC_PERSIST_SAVE_PATH, F_OK) != 0) {
            int ret = rename(PARAM_PERSIST_SAVE_PATH, PARAM_PUBLIC_PERSIST_SAVE_PATH);
            if (ret != 0) {
                PARAM_LOGE("rename failed %s", PARAM_PERSIST_SAVE_PATH);
            }
        } else {
            CheckAndCreateDir(PARAM_PUBLIC_PERSIST_SAVE_PATH);
        }
        *path = PARAM_PUBLIC_PERSIST_SAVE_PATH;
        *tmpPath = PARAM_PUBLIC_PERSIST_SAVE_TMP_PATH;
    } else {
        if (access(PARAM_OLD_PERSIST_SAVE_PATH, F_OK) == 0 && access(PARAM_PRIVATE_PERSIST_SAVE_PATH, F_OK) != 0) {
            int ret = rename(PARAM_OLD_PERSIST_SAVE_PATH, PARAM_PRIVATE_PERSIST_SAVE_PATH);
            if (ret != 0) {
                PARAM_LOGE("rename failed %s", PARAM_OLD_PERSIST_SAVE_PATH);
            }
        } else {
            CheckAndCreateDir(PARAM_PRIVATE_PERSIST_SAVE_PATH);
        }
        *path = PARAM_PRIVATE_PERSIST_SAVE_PATH;
        *tmpPath = PARAM_PRIVATE_PERSIST_SAVE_TMP_PATH;
    }
}

static int LoadPersistParam(int fileType)
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

    char *tmpPath = "";
    char *path = "";
    (void)GetPersistFilePath(&path, &tmpPath, fileType);
    LoadPersistParam_(clearFactoryPersistParams, path, buffer, buffSize);
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
    int ret = -1;
    if (InUpdaterMode() == 1) {
        char *path = "/param/persist_parameters";
        FILE *fp = fopen(path, "a+");
        if (fp != NULL) {
            ret = fprintf(fp, "%s=%s\n", name, value);
            (void)fclose(fp);
        }
        ParamMutexPost(&g_saveMutex);
        return ret;
    }
    const char *path[PERSIST_HANDLE_MAX] = { PARAM_PUBLIC_PERSIST_SAVE_PATH, PARAM_PRIVATE_PERSIST_SAVE_PATH };
    for (int i = 0; i < PERSIST_HANDLE_MAX; i++) {
        FILE *fp = fopen(path[i], "a+");
        if (fp != NULL) {
            ret = fprintf(fp, "%s=%s\n", name, value);
            (void)fclose(fp);
        }
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
    if (InUpdaterMode() == 1) {
        char *path = "/param/tmp_persist_parameters";
        unlink(path);
        FILE *fp = fopen(path, "w");
        if (fp == NULL) {
            ParamMutexPost(&g_saveMutex);
            PARAM_LOGE("Open file %s fail error %d", path, errno);
            return -1;
        }
        handle[0] = (PERSIST_SAVE_HANDLE)fp;
        return 0;
    }
    const char *path[PERSIST_HANDLE_MAX] = {
        PARAM_PUBLIC_PERSIST_SAVE_TMP_PATH,
        PARAM_PRIVATE_PERSIST_SAVE_TMP_PATH
    };
    for (int i = 0; i < PERSIST_HANDLE_MAX; i++) {
        unlink(path[i]);
        FILE *fp = fopen(path[i], "w");
        if (fp == NULL) {
            PARAM_LOGE("Open file %s fail error %d", path[i], errno);
        } else {
            handle[i] = (PERSIST_SAVE_HANDLE)fp;
        }
    }
    return 0;
}

static int BatchSavePersistParam(PERSIST_SAVE_HANDLE handle[], const char *name, const char *value)
{
    int ret = 0;
    for (int i = 0; i < PERSIST_HANDLE_MAX; i++) {
        FILE *fp = (FILE*)handle[i];
        if (fp != NULL) {
            ret = fprintf(fp, "%s=%s\n", name, value);
            PARAM_CHECK(ret > 0, return -1, "Batchsavepersistparam fail, error %d", errno);
        }
    }
    return (ret > 0) ? 0 : -1;
}

static void BatchSavePersistParamEnd(PERSIST_SAVE_HANDLE handle[])
{
    int ret = 0;
    if (InUpdaterMode() == 1) {
        FILE *fp = (FILE *)handle[0];
        (void)fflush(fp);
        (void)fsync(fileno(fp));
        (void)fclose(fp);
        unlink("/param/persist_parameters");
        ret = rename("/param/tmp_persist_parameters", "/param/persist_parameters");
        ParamMutexPost(&g_saveMutex);
        return;
    }
    const char *tmpPath[PERSIST_HANDLE_MAX] = {
        PARAM_PUBLIC_PERSIST_SAVE_TMP_PATH,
        PARAM_PRIVATE_PERSIST_SAVE_TMP_PATH
    };
    const char *path[PERSIST_HANDLE_MAX] = {
        PARAM_PUBLIC_PERSIST_SAVE_PATH,
        PARAM_PRIVATE_PERSIST_SAVE_PATH
    };
    for (int i = 0; i < PERSIST_HANDLE_MAX; i++) {
        if (handle[i] != NULL) {
            FILE *fp = (FILE *)handle[i];
            (void)fflush(fp);
            (void)fsync(fileno(fp));
            (void)fclose(fp);
        }
        unlink(path[i]);
        ret = rename(tmpPath[i], path[i]);
    }
    ParamMutexPost(&g_saveMutex);
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