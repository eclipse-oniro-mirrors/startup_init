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
#ifndef APPSPAWN_INIT_PARAM_MOCK_H
#define APPSPAWN_INIT_PARAM_MOCK_H

int MockInitparamservice();

void MockLoadspecialparam();

int MockStartparamservice();

void MockStopparamservice();

int MockLoaddefaultparams(const char *fileName, uint32_t mode);

int MockLoadpersistparams();

int MockLoadprivatepersistparams();

int MockSystemwriteparam(const char *name, const char *value);

int MockSystemupdateconstparam(const char *name, const char *value);

void MockPosttrigger(EventType type, const char *content, uint32_t contentLen);

void MockDotriggerexec(const char *triggerName);

void MockDojobexecnow(const char *triggerName);

int MockAddcompletejob(const char *name, const char *condition, const char *cmdContent);

int MockSystemsetparameter(const char *name, const char *value);

int MockSystemsaveparameters();

int MockSystemreadparam(const char *name, char *value, uint32_t *len);

int MockSystemwaitparameter(const char *name, const char *value, int32_t timeout);

int MockSystemwatchparameter(const char *keyprefix, ParameterChangePtr change, void *context);

int MockSystemcheckparamexist(const char *name);

int MockWatchparamcheck(const char *keyprefix);

void MockResetparamsecuritylabel();

namespace OHOS {
namespace AppSpawn {
class InitParam {
public:
    virtual ~InitParam() = default;
    virtual int InitParamService() = 0;

    virtual void LoadSpecialParam() = 0;

    virtual int StartParamService() = 0;

    virtual void StopParamService() = 0;

    virtual int LoadDefaultParams(const char *fileName, uint32_t mode) = 0;

    virtual int LoadPersistParams() = 0;

    virtual int LoadPrivatePersistParams() = 0;

    virtual int SystemWriteParam(const char *name, const char *value) = 0;

    virtual int SystemUpdateConstParam(const char *name, const char *value) = 0;

    virtual void PostTrigger(EventType type, const char *content, uint32_t contentLen) = 0;

    virtual void DoTriggerExec(const char *triggerName) = 0;

    virtual void DoJobExecNow(const char *triggerName) = 0;

    virtual int AddCompleteJob(const char *name, const char *condition, const char *cmdContent) = 0;

    virtual int SystemSetParameter(const char *name, const char *value) = 0;

    virtual int SystemSaveParameters() = 0;

    virtual int SystemReadParam(const char *name, char *value, uint32_t *len) = 0;

    virtual int SystemWaitParameter(const char *name, const char *value, int32_t timeout) = 0;

    virtual int SystemWatchParameter(const char *keyprefix, ParameterChangePtr change, void *context) = 0;

    virtual int SystemCheckParamExist(const char *name) = 0;

    virtual int WatchParamCheck(const char *keyprefix) = 0;

    virtual void ResetParamSecurityLabel() = 0;

public:
    static inline std::shared_ptr<InitParam> initParamFunc = nullptr;
};

class InitParamMock : public InitParam {
public:

    MOCK_METHOD(int, InitParamService, ());

    MOCK_METHOD(void, LoadSpecialParam, ());

    MOCK_METHOD(int, StartParamService, ());

    MOCK_METHOD(void, StopParamService, ());

    MOCK_METHOD(int, LoadDefaultParams, (const char *fileName, uint32_t mode));

    MOCK_METHOD(int, LoadPersistParams, ());

    MOCK_METHOD(int, LoadPrivatePersistParams, ());

    MOCK_METHOD(int, SystemWriteParam, (const char *name, const char *value));

    MOCK_METHOD(int, SystemUpdateConstParam, (const char *name, const char *value));

    MOCK_METHOD(void, PostTrigger, (EventType type, const char *content, uint32_t contentLen));

    MOCK_METHOD(void, DoTriggerExec, (const char *triggerName));

    MOCK_METHOD(void, DoJobExecNow, (const char *triggerName));

    MOCK_METHOD(int, AddCompleteJob, (const char *name, const char *condition, const char *cmdContent));

    MOCK_METHOD(int, SystemSetParameter, (const char *name, const char *value));

    MOCK_METHOD(int, SystemSaveParameters, ());

    MOCK_METHOD(int, SystemReadParam, (const char *name, char *value, uint32_t *len));

    MOCK_METHOD(int, SystemWaitParameter, (const char *name, const char *value, int32_t timeout));

    MOCK_METHOD(int, SystemWatchParameter, (const char *keyprefix, ParameterChangePtr change, void *context));

    MOCK_METHOD(int, SystemCheckParamExist, (const char *name));

    MOCK_METHOD(int, WatchParamCheck, (const char *keyprefix));

    MOCK_METHOD(void, ResetParamSecurityLabel, ());

};
} // namespace AppSpawn
} // namespace OHOS

#endif // APPSPAWN_INIT_PARAM_MOCK_H
