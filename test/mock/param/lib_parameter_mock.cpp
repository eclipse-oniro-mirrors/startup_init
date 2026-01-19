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
#include "lib_parameter_mock.h"

using namespace OHOS::AppSpawn;
int MockGetparameter(const char *key, const char *def, char *value, uint32_t len)
{
    return Parameter::parameterFunc->GetParameter(const char *key, const char *def, char *value, uint32_t len);
}

int MockSetparameter(const char *key, const char *value)
{
    return Parameter::parameterFunc->SetParameter(const char *key, const char *value);
}

int MockWaitparameter(const char *key, const char *value, int timeout)
{
    return Parameter::parameterFunc->WaitParameter(const char *key, const char *value, int timeout);
}

int MockWatchparameter(const char *keyPrefix, ParameterChgPtr callback, void *context)
{
    return Parameter::parameterFunc->WatchParameter(const char *keyPrefix, ParameterChgPtr callback, void *context);
}

int MockRemoveparameterwatcher(const char *keyPrefix, ParameterChgPtr callback, void *context)
{
    return Parameter::parameterFunc->RemoveParameterWatcher(const char *keyPrefix, ParameterChgPtr callback, void *context);
}

int MockSaveparameters()
{
    return Parameter::parameterFunc->SaveParameters();
}

int MockGetsdkapiversion()
{
    return Parameter::parameterFunc->GetSdkApiVersion();
}

int MockGetsdkminorapiversion()
{
    return Parameter::parameterFunc->GetSdkMinorApiVersion();
}

int MockGetsdkpatchapiversion()
{
    return Parameter::parameterFunc->GetSdkPatchApiVersion();
}

int MockGetfirstapiversion()
{
    return Parameter::parameterFunc->GetFirstApiVersion();
}

int MockGetdevudid(char *udid, int size)
{
    return Parameter::parameterFunc->GetDevUdid(char *udid, int size);
}

int MockAclgetdevudid(char *udid, int size)
{
    return Parameter::parameterFunc->AclGetDevUdid(char *udid, int size);
}

int MockAclgetdisksn(char *diskSN, int size)
{
    return Parameter::parameterFunc->AclGetDiskSN(char *diskSN, int size);
}

int MockGetperformanceclass()
{
    return Parameter::parameterFunc->GetPerformanceClass();
}

uint32_t MockFindparameter(const char *key)
{
    return Parameter::parameterFunc->FindParameter(const char *key);
}

uint32_t MockGetparametercommitid(uint32_t handle)
{
    return Parameter::parameterFunc->GetParameterCommitId(uint32_t handle);
}

int MockGetparametername(uint32_t handle, char *key, uint32_t len)
{
    return Parameter::parameterFunc->GetParameterName(uint32_t handle, char *key, uint32_t len);
}

int MockGetparametervalue(uint32_t handle, char *value, uint32_t len)
{
    return Parameter::parameterFunc->GetParameterValue(uint32_t handle, char *value, uint32_t len);
}

long long MockGetsystemcommitid()
{
    return Parameter::parameterFunc->GetSystemCommitId();
}

int32_t MockGetintparameter(const char *key, int32_t def)
{
    return Parameter::parameterFunc->GetIntParameter(const char *key, int32_t def);
}

uint32_t MockGetuintparameter(const char *key, uint32_t def)
{
    return Parameter::parameterFunc->GetUintParameter(const char *key, uint32_t def);
}

int MockGetbootcount()
{
    return Parameter::parameterFunc->GetBootCount();
}

int MockGetdistributionosapiversion()
{
    return Parameter::parameterFunc->GetDistributionOSApiVersion();
}

