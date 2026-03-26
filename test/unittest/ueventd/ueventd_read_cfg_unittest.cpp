/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
class UeventdReadCfgUnitTest : public testing::Test {
public:
static void SetUpTestCase(void) {};
static void TearDownTestCase(void) {};
void SetUp() {};
void TearDown() {};
};

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_ParseDeviceConfig001, TestSize.Level0)
{
    char config[] = "/dev/test 0666 1000 1000";
    int ret = ParseDeviceConfig(config);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_ParseDeviceConfig002, TestSize.Level0)
{
    char config[] = "/dev/test 0666 1000 1000 test_param";
    int ret = ParseDeviceConfig(config);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_ParseDeviceConfig003, TestSize.Level0)
{
    char config[] = "/dev/test 0666 1000";
    int ret = ParseDeviceConfig(config);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_ParseSysfsConfig001, TestSize.Level0)
{
    char config[] = "/sys/test attr 0666 1000 1000";
    int ret = ParseSysfsConfig(config);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_ParseFirmwareConfig001, TestSize.Level0)
{
    mkdir("/data/firmware_test", 0755);
    char config[] = "/data/firmware_test";
    int ret = ParseFirmwareConfig(config);
    EXPECT_EQ(ret, 0);
    rmdir("/data/firmware_test");
}

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_GetSection001, TestSize.Level0)
{
    SECTION ret = GetSection("device");
    EXPECT_EQ(ret, SECTION_DEVICE);
}

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_GetSection002, TestSize.Level0)
{
    SECTION ret = GetSection("sysfs");
    EXPECT_EQ(ret, SECTION_SYSFS);
}

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_GetSection003, TestSize.Level0)
{
    SECTION ret = GetSection("firmware");
    EXPECT_EQ(ret, SECTION_FIRMWARE);
}

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_GetSection004, TestSize.Level0)
{
    SECTION ret = GetSection("unknown");
    EXPECT_EQ(ret, SECTION_INVALID);
}

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_GetSection005, TestSize.Level0)
{
    SECTION ret = GetSection(nullptr);
    EXPECT_EQ(ret, SECTION_INVALID);
}

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_DoUeventConfigParse001, TestSize.Level0)
{
    char buffer[] = "[device]\n/dev/test 0666 1000 1000\n";
    DoUeventConfigParse(buffer, strlen(buffer));
    uid_t uid = 0;
    gid_t gid = 0;
    mode_t mode = 0;
    GetDeviceNodePermissions("/dev/test", &uid, &gid, &mode);
    EXPECT_EQ(uid, 1000);
    EXPECT_EQ(gid, 1000);
    EXPECT_EQ(mode, 0666);
}

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_DoUeventConfigParse002, TestSize.Level0)
{
    char buffer[][] = "# This is a comment\n[device]\n/dev/test 0666 1000 1000\n";
    DoUeventConfigParse(buffer, strlen(buffer));
}

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_CloseUeventConfig001, TestSize.Level0)
{
    CloseUeventConfig();
}

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_IsMatch001, TestSize.Level0)
{
    bool ret = IsMatch("test", "test");
    EXPECT_TRUE(ret);
}

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_IsMatch002, TestSize.Level0)
{
    bool ret = IsMatch("test", "t*");
    EXPECT_TRUE(ret);
}

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_IsMatch003, TestSize.Level0)
{
    bool ret = IsMatch("test", "t??t");
    EXPECT_TRUE(ret);
}

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_IsMatch004, TestSize.Level0)
{
    bool ret = IsMatch("test", "te?t");
    EXPECT_FALSE(ret);
}

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_IsMatch005, TestSize.Level0)
{
    bool ret = IsMatch(nullptr, "test");
    EXPECT_FALSE(ret);
}

HWTEST_F(UeventdReadCfgUnitTest, Init_UeventdReadCfgTest_IsMatch006, TestSize.Level0)
{
    bool ret = IsMatch("test", nullptr);
    EXPECT_TRUE(ret);
}

} // namespace UeventdUt