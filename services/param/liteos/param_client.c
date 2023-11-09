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
#include "init_log.h"
#include "init_param.h"
#include "param_init.h"
#include "param_manager.h"

#define MIN_SLEEP (100 * 1000)

#ifdef __LITEOS_A__
static int g_flags = 0;
__attribute__((constructor)) static int ClientInit(void)
{
    if (PARAM_TEST_FLAG(g_flags, WORKSPACE_FLAGS_INIT)) {
        return 0;
    }
    EnableInitLog(INIT_INFO);
    PARAM_LOGV("InitParamClient");
    int ret = InitParamWorkSpace(1, NULL);
    PARAM_CHECK(ret == 0, return -1, "Failed to init param workspace");
    PARAM_SET_FLAG(g_flags, WORKSPACE_FLAGS_INIT);
    // init persist to save
    InitPersistParamWorkSpace();
    return 0;
}

__attribute__((destructor)) static void ClientDeinit(void)
{
    if (PARAM_TEST_FLAG(g_flags, WORKSPACE_FLAGS_INIT)) {
        CloseParamWorkSpace();
    }
    PARAM_SET_FLAG(g_flags, 0);
}
#endif

int SystemSetParameter(const char *name, const char *value)
{
    PARAM_CHECK(name != NULL && value != NULL, return -1, "Invalid name or value %s", name);
    int ctrlService = 0;
    int ret = CheckParameterSet(name, value, GetParamSecurityLabel(), &ctrlService);
    PARAM_CHECK(ret == 0, return ret, "Forbid to set parameter %s", name);
    PARAM_LOGV("SystemSetParameter name %s value: %s ctrlService %d", name, value, ctrlService);
    if ((ctrlService & PARAM_CTRL_SERVICE) != PARAM_CTRL_SERVICE) { // ctrl param
        uint32_t dataIndex = 0;
        ret = WriteParam(name, value, &dataIndex, 0);
        PARAM_CHECK(ret == 0, return ret, "Failed to set param %d name %s %s", ret, name, value);
        ret = WritePersistParam(name, value);
        PARAM_CHECK(ret == 0, return ret, "Failed to set persist param name %s", name);
    } else {
        PARAM_LOGE("SystemSetParameter can not support service ctrl parameter name %s", name);
    }
    return ret;
}

int SystemWaitParameter(const char *name, const char *value, int32_t timeout)
{
    PARAM_CHECK(name != NULL && value != NULL, return PARAM_CODE_INVALID_PARAM,
        "SystemWaitParameter failed! name is: %s, errNum is: %d", name, PARAM_CODE_INVALID_PARAM);
    long long globalCommit = 0;
    // first check permission
    int ret = CheckParamPermission(GetParamSecurityLabel(), name, DAC_READ);
    PARAM_CHECK(ret == 0, return ret, "SystemWaitParameter failed! name is: %s, errNum is: %d", name, ret);
    uint32_t diff = 0;
    struct timespec startTime = {0};
    if (timeout <= 0) {
        timeout = DEFAULT_PARAM_WAIT_TIMEOUT;
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &startTime);
    do {
        long long commit = GetSystemCommitId();
        if (commit != globalCommit) {
            ParamNode *param = SystemCheckMatchParamWait(name, value);
            if (param != NULL) {
                ret = 0;
                break;
            }
        }
        globalCommit = commit;

        usleep(MIN_SLEEP);
        struct timespec finishTime = {0};
        (void)clock_gettime(CLOCK_MONOTONIC, &finishTime);
        diff = IntervalTime(&finishTime, &startTime);
        if (diff >= timeout) {
            ret = PARAM_CODE_TIMEOUT;
            break;
        }
    } while (1);
    PARAM_LOGI("SystemWaitParameter name %s value: %s diff %d timeout %d", name, value, diff, diff);
    if (ret != 0) {
        PARAM_LOGE("SystemWaitParameter failed! name is:%s, errNum is %d", name, ret);
    }
    return ret;
}

int SystemSaveParameters(void)
{
    CheckAndSavePersistParam();
    return PARAM_CODE_SUCCESS;
}

int WatchParamCheck(const char *keyprefix)
{
    (void)keyprefix;
    return PARAM_CODE_NOT_SUPPORT;
}

int SystemCheckParamExist(const char *name)
{
    return SysCheckParamExist(name);
}
