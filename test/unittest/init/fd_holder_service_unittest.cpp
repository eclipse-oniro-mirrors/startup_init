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

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "fd_holder_internal.h"
#include "init_log.h"
#include "init_service.h"
#include "init_service_manager.h"
#include "init_utils.h"
#include "loop_event.h"
#include "securec.h"
#include "param_stub.h"
#include "init_group_manager.h"

using namespace testing::ext;
using namespace std;

namespace init_ut {
class FdHolderServiceUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {
        InitServiceSpace();
    }
    static void TearDownTestCase(void) {
        CloseParamWorkSpace();
    }
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(FdHolderServiceUnitTest, TestRegisterFdHoldWatcherWithInvalidSock, TestSize.Level0)
{
}

HWTEST_F(FdHolderServiceUnitTest, TestRegisterFdHoldWatcherWithValidSock, TestSize.Level0)
{
    int sock = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (sock < 0) {
        return;
    }
    close(sock);
}

HWTEST_F(FdHolderServiceUnitTest, TestHandlerFdHolderWithNullBuffer, TestSize.Level0)
{
}

HWTEST_F(FdHolderServiceUnitTest, TestHandlerFdHolderWithInvalidMessage, TestSize.Level0)
{
}

HWTEST_F(FdHolderServiceUnitTest, TestProcessFdHoldEvent, TestSize.Level0)
{
}

} // namespace init_ut
