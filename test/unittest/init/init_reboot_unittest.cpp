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
#include "init_unittest.h"
#include "init_utils.h"
#include "param_libuvadp.h"
#include "trigger_manager.h"

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

HWTEST_F(InitRebootUnitTest, TestInitReboot, TestSize.Level1)
{
    ExecReboot("reboot");
    ExecReboot("reboot,shutdown");
    ExecReboot("reboot,bootloader");
    ExecReboot("reboot,updater:123");
    ExecReboot("reboot,flash:123");
    const char *option = nullptr;
    int ret = DoReboot(option);
    EXPECT_EQ(ret, 0);
    option = "updater";
    ret = DoReboot(option);
    EXPECT_EQ(ret, 0);
}
} // namespace init_ut
