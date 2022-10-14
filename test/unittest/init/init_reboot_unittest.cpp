/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include <sys/statvfs.h>
#include "init_cmds.h"
#include "init_reboot.h"
#include "init_param.h"
#include "param_stub.h"
#include "init_utils.h"
#include "trigger_manager.h"
#include "init_group_manager.h"
#include "init_cmdexecutor.h"

using namespace testing::ext;
using namespace std;

namespace init_ut {
class InitRebootUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

#ifndef OHOS_LITE
static int g_result = 0;
HWTEST_F(InitRebootUnitTest, TestAddRebootCmd, TestSize.Level1)
{
    auto rebootCallback = [](int id, const char *name, int argc, const char **argv) -> int {
        return 0;
    };
    int ret = AddRebootCmdExecutor("reboot_cmd1", rebootCallback);
    EXPECT_EQ(ret, 0);
    ret = AddRebootCmdExecutor("reboot_cmd2", rebootCallback);
    EXPECT_EQ(ret, 0);
    ret = AddRebootCmdExecutor("reboot_cmd3", rebootCallback);
    EXPECT_EQ(ret, 0);
    ret = AddRebootCmdExecutor("reboot_cmd4", [](int id, const char *name, int argc, const char **argv)-> int {
        g_result = 4; // 4 test index
        return 0;
    });
    EXPECT_EQ(ret, 0);
    ret = AddRebootCmdExecutor("reboot_cmd5", [](int id, const char *name, int argc, const char **argv)-> int {
        g_result = 5; // 5 test index
        return 0;
    });
    EXPECT_EQ(ret, 0);
    ret = AddRebootCmdExecutor("reboot_cmd6", [](int id, const char *name, int argc, const char **argv)-> int {
        g_result = 6; // 6 test index
        return 0;
    });
    EXPECT_EQ(ret, 0);
    ret = AddRebootCmdExecutor("reboot_cmd7", rebootCallback);
    EXPECT_EQ(ret, 0);
    ret = AddRebootCmdExecutor("reboot_cmd7", rebootCallback);
    EXPECT_NE(ret, 0);

    TestSetParamCheckResult("ohos.servicectrl.reboot", 0777, 0);
    // exec
    SystemWriteParam("ohos.startup.powerctrl", "reboot,reboot_cmd4");
    EXPECT_EQ(g_result, 4); // 4 test index
    SystemWriteParam("ohos.startup.powerctrl", "reboot,reboot_cmd5");
    EXPECT_EQ(g_result, 5); // 5 test index
    SystemWriteParam("ohos.startup.powerctrl", "reboot,reboot_cmd6");
    EXPECT_EQ(g_result, 6); // 6 test index

    // invalid test
    ret = AddRebootCmdExecutor(nullptr, [](int id, const char *name, int argc, const char **argv)-> int {
        printf("reboot_cmd7 %s", name);
        return 0;
    });
    EXPECT_NE(ret, 0);
    ret = AddRebootCmdExecutor(nullptr, nullptr);
    EXPECT_NE(ret, 0);
}
#endif

HWTEST_F(InitRebootUnitTest, TestInitReboot, TestSize.Level1)
{
    ExecReboot("reboot");
    ExecReboot("reboot,shutdown");
    ExecReboot("reboot,bootloader");
    ExecReboot("reboot,updater:123");
    ExecReboot("reboot,flash:123");
    ExecReboot("reboot,flashd:123");
    ExecReboot("reboot,suspend:123");
    const char *option = nullptr;
    int ret = DoReboot(option);
    EXPECT_EQ(ret, 0);
    option = "updater";
    ret = DoReboot(option);
    EXPECT_EQ(ret, 0);
    ret = DoReboot(DEVICE_CMD_SUSPEND);
    EXPECT_EQ(ret, 0);
    ret = DoReboot(DEVICE_CMD_FREEZE);
    EXPECT_EQ(ret, 0);
    clearMisc();
}
} // namespace init_ut
