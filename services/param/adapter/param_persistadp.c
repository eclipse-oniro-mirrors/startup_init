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
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "init_utils.h"
#include "param_persist.h"
#include "param_utils.h"

typedef struct {
    void *context;
    PersistParamGetPtr persistParamGet;
} PersistAdpContext;

static int LoadPersistParam(PersistParamGetPtr persistParamGet, void *context)
{
    CheckAndCreateDir(PARAM_PERSIST_SAVE_PATH);
    int updaterMode = InUpdaterMode();
    char *tmpPath = (updaterMode == 0) ? PARAM_PERSIST_SAVE_TMP_PATH : "/param/tmp_persist_parameters";
    FILE *fp = fopen(tmpPath, "r");
    if (fp == NULL) {
        fp = fopen((updaterMode == 0) ? PARAM_PERSIST_SAVE_PATH : "/param/persist_parameters", "r");
        PARAM_LOGI("LoadPersistParam open file %s", PARAM_PERSIST_SAVE_PATH);
    }
    PARAM_CHECK(fp != NULL, return -1, "No valid persist parameter file %s", PARAM_PERSIST_SAVE_PATH);

    char *buff = (char *)calloc(1, PARAM_BUFFER_SIZE);
    SubStringInfo *info = calloc(1, sizeof(SubStringInfo) * (SUBSTR_INFO_VALUE + 1));
    while (info != NULL && buff != NULL && fgets(buff, PARAM_BUFFER_SIZE, fp) != NULL) {
        buff[PARAM_BUFFER_SIZE - 1] = '\0';
        int subStrNumber = GetSubStringInfo(buff, strlen(buff), '=', info, SUBSTR_INFO_VALUE + 1);
        if (subStrNumber <= SUBSTR_INFO_VALUE) {
            continue;
        }
        int ret = persistParamGet(info[0].value, info[1].value, context);
        PARAM_CHECK(ret == 0, continue, "Failed to set param %d %s", ret, buff);
    }
    if (info) {
        free(info);
    }
    if (buff) {
        free(buff);
    }
    (void)fclose(fp);
    return 0;
}

static int SavePersistParam(const char *name, const char *value)
{
    PARAM_LOGD("SavePersistParam %s=%s", name, value);
    return 0;
}

static int BatchSavePersistParamBegin(PERSIST_SAVE_HANDLE *handle)
{
    FILE *fp = fopen((InUpdaterMode() == 0) ? PARAM_PERSIST_SAVE_TMP_PATH : "/param/tmp_persist_parameters", "w");
    PARAM_CHECK(fp != NULL, return -1, "Open file %s fail error %d", PARAM_PERSIST_SAVE_TMP_PATH, errno);
    *handle = (PERSIST_SAVE_HANDLE)fp;
    return 0;
}

static int BatchSavePersistParam(PERSIST_SAVE_HANDLE handle, const char *name, const char *value)
{
    FILE *fp = (FILE *)handle;
    int ret = fprintf(fp, "%s=%s\n", name, value);
    PARAM_CHECK(ret > 0, return -1, "Failed to write param");
    PARAM_LOGD("BatchSavePersistParam %s=%s", name, value);
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
    PARAM_CHECK(ret == 0, return, "BatchSavePersistParamEnd %s fail error %d", PARAM_PERSIST_SAVE_TMP_PATH, errno);
}

int RegisterPersistParamOps(PersistParamOps *ops)
{
    PARAM_CHECK(ops != NULL, return -1, "Invalid ops");
    ops->save = SavePersistParam;
    ops->load = LoadPersistParam;
    ops->batchSaveBegin = BatchSavePersistParamBegin;
    ops->batchSave = BatchSavePersistParam;
    ops->batchSaveEnd = BatchSavePersistParamEnd;
    return 0;
}
