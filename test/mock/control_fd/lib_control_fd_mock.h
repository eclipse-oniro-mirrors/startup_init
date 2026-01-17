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
#ifndef APPSPAWN_CONTROL_FD_MOCK_H
#define APPSPAWN_CONTROL_FD_MOCK_H

void MockCmdserviceinit(const char *socketPath, CallbackControlFdProcess func, LoopHandle loop);

void MockCmdclientinit(const char *socketPath, uint16_t type, const char *cmd, CallbackSendMsgProcess callback);

void MockCmdserviceprocessdelclient(pid_t pid);

void MockCmdserviceprocessdestroyclient();

int MockInitptyinterface(CmdAgent *agent, uint16_t type, const char *cmd, CallbackSendMsgProcess callback);

namespace OHOS {
namespace AppSpawn {
class ControlFd {
public:
    virtual ~ControlFd() = default;
    virtual void CmdServiceInit(const char *socketPath, CallbackControlFdProcess func, LoopHandle loop) = 0;

    virtual void CmdClientInit(const char *socketPath, uint16_t type, const char *cmd, CallbackSendMsgProcess callback) = 0;

    virtual void CmdServiceProcessDelClient(pid_t pid) = 0;

    virtual void CmdServiceProcessDestroyClient() = 0;

    virtual int InitPtyInterface(CmdAgent *agent, uint16_t type, const char *cmd, CallbackSendMsgProcess callback) = 0;

public:
    static inline std::shared_ptr<ControlFd> controlFdFunc = nullptr;
};

class ControlFdMock : public ControlFd {
public:

    MOCK_METHOD(void, CmdServiceInit, (const char *socketPath, CallbackControlFdProcess func, LoopHandle loop));

    MOCK_METHOD(void, CmdClientInit, (const char *socketPath, uint16_t type, const char *cmd, CallbackSendMsgProcess callback));

    MOCK_METHOD(void, CmdServiceProcessDelClient, (pid_t pid));

    MOCK_METHOD(void, CmdServiceProcessDestroyClient, ());

    MOCK_METHOD(int, InitPtyInterface, (CmdAgent *agent, uint16_t type, const char *cmd, CallbackSendMsgProcess callback));

};
} // namespace AppSpawn
} // namespace OHOS

#endif // APPSPAWN_CONTROL_FD_MOCK_H
