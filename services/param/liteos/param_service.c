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
#include <errno.h>
#include <unistd.h>

#include "init_log.h"
#include "init_param.h"
#include "init_utils.h"
#include "param_manager.h"
#ifdef PARAM_LOAD_CFG_FROM_CODE
#include "param_cfg.h"
#endif
#ifdef __LITEOS_M__
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "parameter.h"
#endif

#ifdef PARAM_LOAD_CFG_FROM_CODE
static const char *StringTrim(char *buffer, int size, const char *name)
{
    char *tmp = (char *)name;
    while (*tmp != '\0' && *tmp != '"') {
        tmp++;
    }
    if (*tmp == '\0') {
        return name;
    }
    // skip "
    tmp++;
    int i = 0;
    while (*tmp != '\0' && i < size) {
        buffer[i++] = *tmp;
        tmp++;
    }
    if (i >= size) {
        return name;
    }
    while (i > 0) {
        if (buffer[i] == '"') {
            buffer[i] = '\0';
            return buffer;
        }
        i--;
    }
    return name;
}
#endif

void InitParamService(void)
{
    PARAM_LOGI("InitParamService %s", DATA_PATH);
    CheckAndCreateDir(PARAM_STORAGE_PATH "/");
    CheckAndCreateDir(DATA_PATH);
    // param space
    int ret = InitParamWorkSpace(0, NULL);
    PARAM_CHECK(ret == 0, return, "Init parameter workspace fail");
    ret = InitPersistParamWorkSpace();
    PARAM_CHECK(ret == 0, return, "Init persist parameter workspace fail");

    // from build
    LoadParamFromBuild();
#ifdef PARAM_LOAD_CFG_FROM_CODE
    char *buffer = calloc(1, PARAM_VALUE_LEN_MAX);
    PARAM_CHECK(buffer != NULL, return, "Failed to malloc for buffer");
    for (size_t i = 0; i < ARRAY_LENGTH(g_paramDefCfgNodes); i++) {
        PARAM_LOGV("InitParamService name %s = %s", g_paramDefCfgNodes[i].name, g_paramDefCfgNodes[i].value);
        uint32_t dataIndex = 0;
        ret = WriteParam(g_paramDefCfgNodes[i].name,
            StringTrim(buffer, PARAM_VALUE_LEN_MAX, g_paramDefCfgNodes[i].value), &dataIndex, 0);
        PARAM_CHECK(ret == 0, continue, "Failed to set param %d name %s %s",
            ret, g_paramDefCfgNodes[i].name, g_paramDefCfgNodes[i].value);
    }
    free(buffer);
#endif
}

int StartParamService(void)
{
    return 0;
}

void StopParamService(void)
{
    PARAM_LOGI("StopParamService.");
    ClosePersistParamWorkSpace();
    CloseParamWorkSpace();
}

int SystemWriteParam(const char *name, const char *value)
{
    int ctrlService = 0;
    int ret = CheckParameterSet(name, value, GetParamSecurityLabel(), &ctrlService);
    PARAM_CHECK(ret == 0, return ret, "Forbid to set parameter %s", name);
    PARAM_LOGV("SystemWriteParam name %s value: %s ctrlService %d", name, value, ctrlService);
    if ((ctrlService & PARAM_CTRL_SERVICE) != PARAM_CTRL_SERVICE) { // ctrl param
        uint32_t dataIndex = 0;
        ret = WriteParam(name, value, &dataIndex, 0);
        PARAM_CHECK(ret == 0, return ret, "Failed to set param %d name %s %s", ret, name, value);
        ret = WritePersistParam(name, value);
        PARAM_CHECK(ret == 0, return ret, "Failed to set persist param name %s", name);
    } else {
        PARAM_LOGE("SystemWriteParam can not support service ctrl parameter name %s", name);
    }
    return ret;
}

#ifdef __LITEOS_M__
#define OS_DELAY 1000 // * 30 // 30s
#define STACK_SIZE 1024

static void ParamServiceTask(int *arg)
{
    (void)arg;
    PARAM_LOGI("ParamServiceTask start");
    while (1) {
        CheckAndSavePersistParam();
        osDelay(OS_DELAY);
    }
}

void LiteParamService(void)
{
    static int init = 0;
    if (init) {
        return;
    }
    init = 1;
    EnableInitLog(INIT_INFO);
    InitParamService();
    // get persist param
    LoadPersistParams();
}
CORE_INIT(LiteParamService);
#endif
