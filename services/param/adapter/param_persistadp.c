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
#if !(defined __LITEOFS_A__ || defined __LITEOS_M__)
#include "trigger_manager.h"
#endif

// for linux, no mutex
static ParamMutex g_saveMutex = {};

static int LoadOnePersistParam_(const uint32_t *context, const char *name, const char *value)
{
    if (strncmp(name, "persist", strlen("persist")) != 0) {
        PARAM_LOGE("%s is not persist param, do not load", name);
        return 0;
    }
    bool clearFactoryPersistParams = *(bool*)context;
    uint32_t dataIndex = 0;
    unsigned int mode = 0;
    int result = 0;
    do {
        if (!clearFactoryPersistParams) {
            mode |= LOAD_PARAM_PERSIST;
            result = WriteParam(name, value, &dataIndex, mode);
            break;
        }

        char persetValue[PARAM_VALUE_LEN_MAX] = {0};
        uint32_t len = PARAM_VALUE_LEN_MAX;
        int ret = SystemReadParam(name, persetValue, &len);
        if (ret != 0) {
            mode |= LOAD_PARAM_PERSIST;
            result = WriteParam(name, value, &dataIndex, mode);
            break;
        }

        if ((strcmp(persetValue, value) != 0)) {
            PARAM_LOGI("%s value is different, preset value is:%s, persist value is:%s", name, persetValue, value);
            mode |= LOAD_PARAM_PERSIST;
            result = WriteParam(name, value, &dataIndex, mode);
        }
    } while (0);
#if !(defined __LITEOFS_A__ || defined __LITEOS_M__)
    if (result == 0) {
        PostParamTrigger(EVENT_TRIGGER_PARAM_WATCH, name, value);
    }
#endif
    return result;
}

static bool IsPublicParam(const char *param)
{
    const char *publicPersistParams[] = {
        "persist.arkui.libace.og", "persist.ddgr.opinctype",
        "persist.dfx.leak.threshold", "persist.rosen.animationtrace.enabled",
        "persist.rosen.ddgr.opinctype.debugtype", "persist.sys.graphic.blurEnabled",
        "persist.sys.graphic.drawing.test", "persist.sys.graphic.filterCacheEnabled",
        "persist.sys.graphic.foregroundFilterEnabled", "persist.sys.graphic.GpuApitype",
        "persist.sys.graphic.hmsymbolcfg.enable", "persist.sys.graphic.hmsymboltxt.enable",
        "persist.sys.graphic.HpsBlurEnable", "persist.sys.graphic.kawaseEnable",
        "persist.sys.graphic.openDebugTrace", "persist.sys.graphic.subSurface",
        "persist.sys.text.autospacing.enable", "persist.sys.hilog.binary.forhota.on",
        "persist.sys.hilog.fuse.on", "persist.sys.xlog.debug",
        "persist.sys.hiview.testtype", "persist.security.jitfort.disabled",
        "persist.sys.prefork.enable", "persist.time.timezone",
        "persist.global.tz_override", "persist.dupdate_engine.update_type",
        "persist.update.hmos_to_next_flag", "persist.hiview.leak_detector",
        "persist.hiviewdfx.debugenv", "persist.swing.switch_enable",
        "persist.hiviewdfx.priv.diagnosis.time.deadline", "persist.hmos_fusion_mgr.ctl.support_hmos",
        "persist.init.debug.dump.trigger", "persist.init.bootchart.enabled",
        "persist.init.debug.loglevel", "persist.samgr.cache.sa",
        "persist.sys.hilog.binary.on", "persist.sys.hilog.debug.on",
        "persist.bluetooth.collaboration_service", "persist.bluetooth.switch_enable",
        "persist.edc.proxy_ap_startup", "persist.edm.edm_enable",
        "persist.moduleupdate.bms.scan", "persist.nearlink.switch_enable",
        "persist.parentcontrol.enable", "persist.samgr.moduleupdate.start",
    };
    int size = sizeof(publicPersistParams) / sizeof(char*);
    for (int i = 0; i < size; i++) {
        if (strncmp(param, publicPersistParams[i], strlen(publicPersistParams[i])) == 0) {
            return true;
        }
    }
    return false;
}

static int LoadOnePublicPersistParam_(const uint32_t *context, const char *name, const char *value)
{
    if (IsPublicParam(name)) {
        return LoadOnePersistParam_(context, name, value);
    }
    PARAM_LOGI("%s is private, ignore", name);
    return 0;
}

static void LoadPersistParam_(const bool clearFactoryPersistParams, const char *fileName,
    char *buffer, uint32_t buffSize, bool isFullLoad)
{
    FILE *fp = fopen(fileName, "r");
    PARAM_WARNING_CHECK(fp != NULL, return, "No valid persist parameter file %s", fileName);
    int ret = 0;
    uint32_t paramNum = 0;
    while (fgets(buffer, buffSize, fp) != NULL) {
        buffer[buffSize - 1] = '\0';
        if (isFullLoad) {
            ret = SplitParamString(buffer, NULL, 0, LoadOnePersistParam_, (uint32_t*)&clearFactoryPersistParams);
        } else {
            ret = SplitParamString(buffer, NULL, 0, LoadOnePublicPersistParam_, (uint32_t*)&clearFactoryPersistParams);
        }
        PARAM_CHECK(ret == 0, continue, "Failed to set param %d %s", ret, buffer);
        paramNum++;
    }
    (void)fclose(fp);
    PARAM_LOGI("LoadPersistParam from file %s paramNum %d", fileName, paramNum);
}

static bool GetPersistFilePath(char **path, char **tmpPath, int fileType)
{
    bool isFullLoad = true;
    if (InUpdaterMode() == 1) {
        *path = "/param/persist_parameters";
        *tmpPath = "/param/tmp_persist_paramters";
        return isFullLoad;
    }
    if (fileType == PUBLIC_PERSIST_FILE) {
        isFullLoad = false;
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
        return isFullLoad;
    }
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
    return isFullLoad;
}

static int LoadPersistParam(int fileType)
{
    bool clearFactoryPersistParams = false;
    char value[PARAM_VALUE_LEN_MAX] = {0};
    uint32_t len = PARAM_VALUE_LEN_MAX;
    int ret = SystemReadParam("const.startup.param.version", value, &len);
    if ((ret != 0 || strcmp(value, "1") != 0) &&
        (access(PERSIST_PARAM_FIXED_FLAGS, F_OK) != 0)) {
        clearFactoryPersistParams = true;
    }
    const uint32_t buffSize = PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX + 10;  // 10 max len
    char *buffer = calloc(1, buffSize);
    PARAM_CHECK(buffer != NULL, return -1, "Failed to alloc");

    char *tmpPath = "";
    char *path = "";
    bool isFullLoad = GetPersistFilePath(&path, &tmpPath, fileType);
    LoadPersistParam_(clearFactoryPersistParams, path, buffer, buffSize, isFullLoad);
    LoadPersistParam_(clearFactoryPersistParams, tmpPath, buffer, buffSize, isFullLoad);
    free(buffer);
    if (clearFactoryPersistParams) {
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