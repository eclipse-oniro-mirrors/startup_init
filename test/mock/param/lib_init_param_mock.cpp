/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#include "lib_init_param_mock.h"

using namespace OHOS::AppSpawn;
int MockInitparamservice()
{
    return InitParam::initParamFunc->InitParamService();
}

void MockLoadspecialparam()
{
    InitParam::initParamFunc->LoadSpecialParam();
}

int MockStartparamservice()
{
    return InitParam::initParamFunc->StartParamService();
}

void MockStopparamservice()
{
    InitParam::initParamFunc->StopParamService();
}

int MockLoaddefaultparams(const char *fileName, uint32_t mode)
{
    return InitParam::initParamFunc->LoadDefaultParams(const char *fileName, uint32_t mode);
}

int MockLoadpersistparams()
{
    return InitParam::initParamFunc->LoadPersistParams();
}

int MockLoadprivatepersistparams()
{
    return InitParam::initParamFunc->LoadPrivatePersistParams();
}

int MockSystemwriteparam(const char *name, const char *value)
{
    return InitParam::initParamFunc->SystemWriteParam(const char *name, const char *value);
}

int MockSystemupdateconstparam(const char *name, const char *value)
{
    return InitParam::initParamFunc->SystemUpdateConstParam(const char *name, const char *value);
}

void MockPosttrigger(EventType type, const char *content, uint32_t contentLen)
{
    InitParam::initParamFunc->PostTrigger(EventType type, const char *content, uint32_t contentLen);
}

void MockDotriggerexec(const char *triggerName)
{
    InitParam::initParamFunc->DoTriggerExec(const char *triggerName);
}

void MockDojobexecnow(const char *triggerName)
{
    InitParam::initParamFunc->DoJobExecNow(const char *triggerName);
}

int MockAddcompletejob(const char *name, const char *condition, const char *cmdContent)
{
    return InitParam::initParamFunc->AddCompleteJob(const char *name, const char *condition, const char *cmdContent);
}

int MockSystemsetparameter(const char *name, const char *value)
{
    return InitParam::initParamFunc->SystemSetParameter(const char *name, const char *value);
}

int MockSystemsaveparameters()
{
    return InitParam::initParamFunc->SystemSaveParameters();
}

int MockSystemreadparam(const char *name, char *value, uint32_t *len)
{
    return InitParam::initParamFunc->SystemReadParam(const char *name, char *value, uint32_t *len);
}

int MockSystemwaitparameter(const char *name, const char *value, int32_t timeout)
{
    return InitParam::initParamFunc->SystemWaitParameter(const char *name, const char *value, int32_t timeout);
}

int MockSystemwatchparameter(const char *keyprefix, ParameterChangePtr change, void *context)
{
    return InitParam::initParamFunc->SystemWatchParameter(const char *keyprefix, ParameterChangePtr change, void *context);
}

int MockSystemcheckparamexist(const char *name)
{
    return InitParam::initParamFunc->SystemCheckParamExist(const char *name);
}

int MockWatchparamcheck(const char *keyprefix)
{
    return InitParam::initParamFunc->WatchParamCheck(const char *keyprefix);
}

void MockResetparamsecuritylabel()
{
    InitParam::initParamFunc->ResetParamSecurityLabel();
}

