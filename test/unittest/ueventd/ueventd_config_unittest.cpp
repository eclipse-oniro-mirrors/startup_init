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
#include <cerrno>
#include <dirent.h>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "param_stub.h"
#include "init_utils.h"
#include "ueventd_read_cfg.h"
#include "ueventd_parameter.h"

extern "C" {
bool IsMatch(const char *target, const char *pattern);
}

using namespace std;
using namespace testing::ext;

namespace UeventdUt {
class UeventdConfigUnitTest : public testing::Test {
public:
static void SetUpTestCase(void) {};
static void TearDownTestCase(void) {};
void SetUp() {};
void TearDown() {};
};

HWTEST_F(UeventdConfigUnitTest, TestSectionConfigParse, TestSize.Level0)
{
    ParseUeventdConfigFile(STARTUP_INIT_UT_PATH"/ueventd_ut/valid.config");
    uid_t uid = 0;
    gid_t gid = 0;
    mode_t mode = 0;
    GetDeviceNodePermissions("/dev/test", &uid, &gid, &mode);
    EXPECT_EQ(uid, 1000);
    EXPECT_EQ(gid, 1000);
    EXPECT_EQ(mode, 0666);
    uid = 999;
    gid = 999;
    mode = 0777;
    // /dev/test1 is not a valid item, miss gid
    // UGO will be default value.
    GetDeviceNodePermissions("/dev/test1", &uid, &gid, &mode);
    EXPECT_EQ(uid, 999);
    EXPECT_EQ(gid, 999);
    EXPECT_EQ(mode, 0777);
    // /dev/test2 is not a valid item, too much items
    // UGO will be default value.
    GetDeviceNodePermissions("/dev/test2", &uid, &gid, &mode);
    EXPECT_EQ(uid, 999);
    EXPECT_EQ(gid, 999);
    EXPECT_EQ(mode, 0777);
    ChangeSysAttributePermissions("/dev/test2");
    ChangeSysAttributePermissions("/dir/to/nothing");
}

HWTEST_F(UeventdConfigUnitTest, TestConfigEntry, TestSize.Level0)
{
    string file = "[device";
    int rc = ParseUeventConfig(const_cast<char*>(file.c_str())); // Invalid section
    EXPECT_EQ(rc, -1);
    file = "[unknown]";
    rc = ParseUeventConfig(const_cast<char*>(file.c_str()));  // Unknown section
    EXPECT_EQ(rc, -1);
    file = "[device]";
    rc = ParseUeventConfig(const_cast<char*>(file.c_str())); // valid section
    EXPECT_EQ(rc, 0);
}

HWTEST_F(UeventdConfigUnitTest, TestParameter, TestSize.Level0)
{
    ParseUeventdConfigFile(STARTUP_INIT_UT_PATH"/ueventd_ut/valid.config");
    SetUeventDeviceParameter("/dev/testbinder1", 0);
    SetUeventDeviceParameter("/dev/testbinder2", 0);
    SetUeventDeviceParameter("/dev/testbinder3", 0);
    SetUeventDeviceParameter("/notpath", 0);
    SetUeventDeviceParameter(nullptr, 1);
    GetDeviceUdevConfByDevNode(nullptr);
    int ret = IsMatch("testtarget", "te?*a");
    IsMatch("testtarget", "a");
    ret = IsMatch("test", "t****");
    EXPECT_EQ(ret, true);
}
} // namespace UeventdUt
