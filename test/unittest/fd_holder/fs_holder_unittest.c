/*
* Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "fd_holder_internal.h"
#include "securec.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
class Fd_HolderUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(Fd_HolderUnitTest, Init_BuildControlMessage_001, TestSize.Level0)
{
    struct msghdr *msghdr;
    int *fds;
    int fdCount = 0;
    bool sendUcred = true;
    int ret = BuildControlMessage(msghdr, fds, fdCount, sendUcred);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(Fd_HolderUnitTest, Init_BuildControlMessage_001, TestSize.Level0)
{
    int sock;
    struct iovec iovec;
    size_t *outFdCount;
    bool nonblock;
    pid_t *requestPid;
    ReceiveFds(sock, iovec iovec, outFdCount, nonblock, requestPid);
}
}