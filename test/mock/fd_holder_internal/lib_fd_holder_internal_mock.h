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
#ifndef APPSPAWN_FD_HOLDER_INTERNAL_MOCK_H
#define APPSPAWN_FD_HOLDER_INTERNAL_MOCK_H

int MockBuildcontrolmessage(struct msghdr *msghdr, int *fds, int fdCount, bool sendUcred);

int* MockReceivefds(int sock, struct iovec iovec, size_t *outFdCount, bool nonblock, pid_t *requestPid);

namespace OHOS {
namespace AppSpawn {
class FdHolderInternal {
public:
    virtual ~FdHolderInternal() = default;
    virtual int BuildControlMessage(struct msghdr *msghdr, int *fds, int fdCount, bool sendUcred) = 0;

    virtual int* ReceiveFds(int sock, struct iovec iovec, size_t *outFdCount, bool nonblock, pid_t *requestPid) = 0;

public:
    static inline std::shared_ptr<FdHolderInternal> fdHolderInternalFunc = nullptr;
};

class FdHolderInternalMock : public FdHolderInternal {
public:

    MOCK_METHOD(int, BuildControlMessage, (struct msghdr *msghdr, int *fds, int fdCount, bool sendUcred));

    MOCK_METHOD(int*, ReceiveFds, (int sock, struct iovec iovec, size_t *outFdCount, bool nonblock, pid_t *requestPid));

};
} // namespace AppSpawn
} // namespace OHOS

#endif // APPSPAWN_FD_HOLDER_INTERNAL_MOCK_H
