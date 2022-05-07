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

#include "init_param.h"
#include "init_utils.h"
#include "param_manager.h"

void InitParamService(void)
{
    PARAM_LOGI("InitParamService");
    // param space
    int ret = InitParamWorkSpace(0);
    PARAM_CHECK(ret == 0, return, "Init parameter workspace fail");
    ret = InitPersistParamWorkSpace();
    PARAM_CHECK(ret == 0, return, "Init persist parameter workspace fail");
    // read cmd to param
    LoadParamFromCmdLine();
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
    PARAM_CHECK(ret == 0, return ret, "Forbit to set parameter %s", name);
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
