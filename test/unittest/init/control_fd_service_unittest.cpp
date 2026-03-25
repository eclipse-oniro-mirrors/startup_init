/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "control_fd.h"
#include "init_service.h"
#include "init_service_manager.h"
#include "init_utils.h"
#include "init_log.h"
#include "init_group_manager.h"
#include "init_param.h"
#include "init_cmdexecutor.h"
#include "param_stub.h"
#include "le_loop.h"
#include "init_modulemgr.h"

using namespace testing::ext;
using namespace std;

namespace init_ut {
class ControlFdServiceUnitTest : public testing::Test {
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

HWTEST_F(ControlFdServiceUnitTest, TestInitControlFd, TestSize.Level0)
{
    EXPECT_NE(LE_GetDefaultLoop(), nullptr);
}

HWTEST_F(ControlFdServiceUnitTest, TestProcessControlFdWithSandbox, TestSize.Level0)
{
    const char *serviceName = "test_service";
    Service *service = (Service *)AddService(serviceName);
    ASSERT_NE(service, nullptr);
    
    ProcessControlFd(ACTION_SANDBOX, serviceName, nullptr);
    
    ReleaseService(service);
}

HWTEST_F(ControlFdServiceUnitTest, TestProcessControlFdWithSandboxNullService, TestSize.Level0)
{
    ProcessControlFd(ACTION_SANDBOX, nullptr, nullptr);
}

HWTEST_F(ControlFdServiceUnitTest, TestProcessControlFdWithDumpAll, TestSize.Level0)
{
    ProcessControlFd(ACTION_DUMP, "all", nullptr);
}

HWTEST_F(ControlFdServiceUnitTest, TestProcessControlFdWithDumpParameterService, TestSize.Level0)
{
    ProcessControlFd(ACTION_DUMP, "parameter_service", nullptr);
}

HWTEST_F(ControlFdServiceUnitTest, TestProcessControlFdWithDumpLoop, TestSize.Level0)
{
    ProcessControlFd(ACTION_DUMP, "loop", nullptr);
}

HWTEST_F(ControlFdServiceUnitTest, TestProcessControlFdWithDumpService, TestSize.Level0)
{
    const char *serviceName = "test_dump_service";
    Service *service = (Service *)AddService(serviceName);
    ASSERT_NE(service, nullptr);
    
    ProcessControlFd(ACTION_DUMP, serviceName, nullptr);
    
    ReleaseService(service);
}

HWTEST_F(ControlFdServiceUnitTest, TestProcessControlFdWithModuleMgr, TestSize.Level0)
{
    ProcessControlFd(ACTION_MODULEMGR, "list", nullptr);
}

HWTEST_F(ControlFdServiceUnitTest, TestProcessControlFdWithInvalidType, TestSize.Level0)
{
    ProcessControlFd(ACTION_MAX, "test", nullptr);
}

HWTEST_F(ControlFdServiceUnitTest, TestProcessControlFdWithNullCmd, TestSize.Level0)
{
    ProcessControlFd(ACTION_DUMP, nullptr, nullptr);
}

} // namespace init_ut
