/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "ueventd_parameter.h"

#include <pthread.h>
#include <sys/time.h>

#define INIT_LOG_TAG "ueventd"
#include "init_log.h"
#include "list.h"
#include "init_param.h"
#include "ueventd.h"
#include "ueventd_read_cfg.h"
#include "securec.h"

typedef struct {
    int inited;
    int shutdown;
    int empty;
    pthread_mutex_t lock;
    pthread_cond_t hasData;
    pthread_mutex_t parameterLock;
    struct ListNode parameterList;
    pthread_t threadId;
} DeviceParameterCtrl;

static DeviceParameterCtrl g_parameterCtrl = {
    .inited = 1,
    .shutdown = 0,
    .empty = 1,
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .hasData = PTHREAD_COND_INITIALIZER,
    .parameterLock = PTHREAD_MUTEX_INITIALIZER,
    .parameterList = {&g_parameterCtrl.parameterList, &g_parameterCtrl.parameterList},
    .threadId = 0
};

static struct DeviceUdevConf *GetFristParameter(DeviceParameterCtrl *parameterCtrl)
{
    struct DeviceUdevConf *conf = NULL;
    pthread_mutex_lock(&(parameterCtrl->parameterLock));
    if (!ListEmpty(parameterCtrl->parameterList)) {
        conf = ListEntry(parameterCtrl->parameterList.next, struct DeviceUdevConf, paramNode);
        OH_ListRemove(&conf->paramNode);
        OH_ListInit(&conf->paramNode);
    }
    pthread_mutex_unlock(&(parameterCtrl->parameterLock));
    return conf;
}

static void *ThreadRun(void *data)
{
    DeviceParameterCtrl *parameterCtrl = (DeviceParameterCtrl *)data;
    INIT_LOGV("[uevent] ThreadRun %d %d", parameterCtrl->empty, parameterCtrl->shutdown);
    char paramName[PARAM_NAME_LEN_MAX] = {0};
    while (1) {
        pthread_mutex_lock(&(parameterCtrl->lock));
        if (parameterCtrl->empty) {
            struct timespec abstime = {};
            struct timeval now = {};
            const long timeout = 5000; // wait time 5000ms
            gettimeofday(&now, NULL);
            long nsec = now.tv_usec * 1000 + (timeout % 1000) * 1000000; // 1000 unit 1000000 unit nsec
            abstime.tv_sec = now.tv_sec + nsec / 1000000000 + timeout / 1000; // 1000 unit 1000000000 unit nsec
            abstime.tv_nsec = nsec % 1000000000; // 1000000000 unit nsec
            pthread_cond_timedwait(&(parameterCtrl->hasData), &(parameterCtrl->lock), &abstime);
        }
        if (parameterCtrl->shutdown) {
            break;
        }
        pthread_mutex_unlock(&(parameterCtrl->lock));
        struct DeviceUdevConf *config = GetFristParameter(parameterCtrl);
        if (config == NULL) {
            parameterCtrl->empty = 1;
            continue;
        }
        parameterCtrl->empty = 0;
        const char *paramValue = (config->action == ACTION_ADD) ? "added" : "removed";
        INIT_LOGI("[uevent] SystemSetParameter %s act %s", config->parameter, paramValue);
        int len = sprintf_s(paramName, sizeof(paramName), "startup.uevent.%s", config->parameter);
        if ((len <= 0) || (SystemSetParameter(paramName, paramValue) != 0)) {
            INIT_LOGE("[uevent] SystemSetParameter %s failed", config->parameter);
            pthread_mutex_lock(&(parameterCtrl->parameterLock));
            OH_ListAddTail(&parameterCtrl->parameterList, &config->paramNode);
            pthread_mutex_unlock(&(parameterCtrl->parameterLock));
            parameterCtrl->empty = 1;
        }
    }
    return NULL;
}

static void AddParameter(DeviceParameterCtrl *parameterCtrl, struct DeviceUdevConf *config)
{
    pthread_mutex_lock(&(parameterCtrl->parameterLock));
    if (ListEmpty(config->paramNode)) {
        OH_ListAddTail(&parameterCtrl->parameterList, &config->paramNode);
    }
    pthread_mutex_unlock(&(parameterCtrl->parameterLock));
    if (parameterCtrl->threadId == 0) {
        (void)pthread_create(&(parameterCtrl->threadId), NULL, ThreadRun, (void *)parameterCtrl);
    }
    pthread_mutex_unlock(&(parameterCtrl->lock));
    parameterCtrl->empty = 0;
    pthread_cond_signal(&(parameterCtrl->hasData));
    pthread_mutex_unlock(&(parameterCtrl->lock));
}

int SetUeventDeviceParameter(const char *devNode, int action)
{
    if (devNode == NULL) {
        return -1;
    }
    struct DeviceUdevConf *config = GetDeviceUdevConfByDevNode(devNode);
    if ((config != NULL) && (config->parameter != NULL)) {
        INIT_LOGI("[uevent] SetUeventDeviceParameter parameter %s", config->parameter);
        config->action = action;
        AddParameter(&g_parameterCtrl, config);
    }
    return 0;
}
