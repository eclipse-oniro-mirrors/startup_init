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
#ifndef APPSPAWN_PARAMETER_MOCK_H
#define APPSPAWN_PARAMETER_MOCK_H

int MockGetparameter(const char *key, const char *def, char *value, uint32_t len);

int MockSetparameter(const char *key, const char *value);

int MockWaitparameter(const char *key, const char *value, int timeout);

int MockWatchparameter(const char *keyPrefix, ParameterChgPtr callback, void *context);

int MockRemoveparameterwatcher(const char *keyPrefix, ParameterChgPtr callback, void *context);

int MockSaveparameters();

int MockGetsdkapiversion();

int MockGetsdkminorapiversion();

int MockGetsdkpatchapiversion();

int MockGetfirstapiversion();

int MockGetdevudid(char *udid, int size);

int MockAclgetdevudid(char *udid, int size);

int MockAclgetdisksn(char *diskSN, int size);

int MockGetperformanceclass();

uint32_t MockFindparameter(const char *key);

uint32_t MockGetparametercommitid(uint32_t handle);

int MockGetparametername(uint32_t handle, char *key, uint32_t len);

int MockGetparametervalue(uint32_t handle, char *value, uint32_t len);

long long MockGetsystemcommitid();

int32_t MockGetintparameter(const char *key, int32_t def);

uint32_t MockGetuintparameter(const char *key, uint32_t def);

int MockGetbootcount();

int MockGetdistributionosapiversion();

namespace OHOS {
namespace AppSpawn {
class Parameter {
public:
    virtual ~Parameter() = default;
    virtual int GetParameter(const char *key, const char *def, char *value, uint32_t len) = 0;

    virtual int SetParameter(const char *key, const char *value) = 0;

    virtual int WaitParameter(const char *key, const char *value, int timeout) = 0;

    virtual int WatchParameter(const char *keyPrefix, ParameterChgPtr callback, void *context) = 0;

    virtual int RemoveParameterWatcher(const char *keyPrefix, ParameterChgPtr callback, void *context) = 0;

    virtual int SaveParameters() = 0;

    virtual int GetSdkApiVersion() = 0;

    virtual int GetSdkMinorApiVersion() = 0;

    virtual int GetSdkPatchApiVersion() = 0;

    virtual int GetFirstApiVersion() = 0;

    virtual int GetDevUdid(char *udid, int size) = 0;

    virtual int AclGetDevUdid(char *udid, int size) = 0;

    virtual int AclGetDiskSN(char *diskSN, int size) = 0;

    virtual int GetPerformanceClass() = 0;

    virtual uint32_t FindParameter(const char *key) = 0;

    virtual uint32_t GetParameterCommitId(uint32_t handle) = 0;

    virtual int GetParameterName(uint32_t handle, char *key, uint32_t len) = 0;

    virtual int GetParameterValue(uint32_t handle, char *value, uint32_t len) = 0;

    virtual long long GetSystemCommitId() = 0;

    virtual int32_t GetIntParameter(const char *key, int32_t def) = 0;

    virtual uint32_t GetUintParameter(const char *key, uint32_t def) = 0;

    virtual int GetBootCount() = 0;

    virtual int GetDistributionOSApiVersion() = 0;

public:
    static inline std::shared_ptr<Parameter> parameterFunc = nullptr;
};

class ParameterMock : public Parameter {
public:

    MOCK_METHOD(int, GetParameter, (const char *key, const char *def, char *value, uint32_t len));

    MOCK_METHOD(int, SetParameter, (const char *key, const char *value));

    MOCK_METHOD(int, WaitParameter, (const char *key, const char *value, int timeout));

    MOCK_METHOD(int, WatchParameter, (const char *keyPrefix, ParameterChgPtr callback, void *context));

    MOCK_METHOD(int, RemoveParameterWatcher, (const char *keyPrefix, ParameterChgPtr callback, void *context));

    MOCK_METHOD(int, SaveParameters, ());

    MOCK_METHOD(int, GetSdkApiVersion, ());

    MOCK_METHOD(int, GetSdkMinorApiVersion, ());

    MOCK_METHOD(int, GetSdkPatchApiVersion, ());

    MOCK_METHOD(int, GetFirstApiVersion, ());

    MOCK_METHOD(int, GetDevUdid, (char *udid, int size));

    MOCK_METHOD(int, AclGetDevUdid, (char *udid, int size));

    MOCK_METHOD(int, AclGetDiskSN, (char *diskSN, int size));

    MOCK_METHOD(int, GetPerformanceClass, ());

    MOCK_METHOD(uint32_t, FindParameter, (const char *key));

    MOCK_METHOD(uint32_t, GetParameterCommitId, (uint32_t handle));

    MOCK_METHOD(int, GetParameterName, (uint32_t handle, char *key, uint32_t len));

    MOCK_METHOD(int, GetParameterValue, (uint32_t handle, char *value, uint32_t len));

    MOCK_METHOD(long long, GetSystemCommitId, ());

    MOCK_METHOD(int32_t, GetIntParameter, (const char *key, int32_t def));

    MOCK_METHOD(uint32_t, GetUintParameter, (const char *key, uint32_t def));

    MOCK_METHOD(int, GetBootCount, ());

    MOCK_METHOD(int, GetDistributionOSApiVersion, ());

};
} // namespace AppSpawn
} // namespace OHOS

#endif // APPSPAWN_PARAMETER_MOCK_H
