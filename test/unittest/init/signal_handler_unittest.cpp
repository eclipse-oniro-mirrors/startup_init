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

#include <signal.h>
#include <sys/wait.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "init.h"
#include "init_service.h"
#include "init_service_manager.h"
#include "init_log.h"
#include "init_param.h"
#include "init_group_manager.h"
#include "param_stub.h"

using namespace testing::ext;
using namespace std;

namespace init_ut {
class SignalHandlerUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {
    }
    static void TearDownTestCase(void) {
        CloseParamWorkSpace();
    }
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(SignalHandlerUnitTest, TestSignalInit, TestSize.Level0)
{
    SignalInit();
    EXPECT_NE(LE_GetDefaultLoop(), nullptr);
}

HWTEST_F(SignalHandlerUnitTest, TestHandleSigChild, TestSize.Level0)
{
}

HWTEST_F(SignalHandlerUnitTest, TestHandleSigChildWithNullService, TestSize.Level0)
{
}

HWTEST_F(SignalHandlerUnitTest, TestProcessSignalSIGCHLD, TestSize.Level0)
{
}

HWTEST_F(SignalHandlerUnitTest, TestProcessSignalSIGTERM, TestSize.Level0)
{
}

HWTEST_F(SignalHandlerUnitTest, TestProcessSignalUnsupported, TestSize.Level0)
{
}

HWTEST_F(SignalHandlerUnitTest, TestSignalInitWithNullHandle, TestSize.Level0)
{
}

} // namespace init_ut
