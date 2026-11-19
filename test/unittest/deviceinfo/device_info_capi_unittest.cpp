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

// Unit tests for the env-config C API in device_info.cpp
// (SetEnvConfig/GetEnvConfig/RemoveEnvConfig, defined under #ifdef CUSTOM_SANDBOX
// + #ifdef PARAM_FEATURE_DEVICEINFO).
//
// Why this file exists: these three C API functions had 0% coverage because no
// ut called them — device_info_appenv_unittest.cpp exercises the SA member
// DeviceInfoService::SetEnvConfig directly (bypassing the C API), and
// appenv_cli_unittest.cpp's calls are intercepted by --wrap (in its own target).
//
// This target has NO --wrap, so calling SetEnvConfig/GetEnvConfig/RemoveEnvConfig
// here reaches the real implementations in device_info.cpp directly. Coverage
// is attributed to device_info.cpp.
//
// Coverage scope:
//   - nullptr/size guards (SetEnvConfig null key/value; GetEnvConfig null
//     key/value/size<=0; RemoveEnvConfig null key → SYSPARAM_INVALID_INPUT)
//   - kits-call branch (valid args → instance.SetEnvConfig → kits → samgr;
//     under no device/SA returns non-zero, which the C API forwards)

#include <cstring>
#include <gtest/gtest.h>
#include <string>

#include "sysparam_errno.h"

using namespace testing::ext;

// C API declarations (device_info.cpp defines them extern "C" under
// #ifdef CUSTOM_SANDBOX + #ifdef PARAM_FEATURE_DEVICEINFO; this target defines
// both). No __real_ prefix needed — this target has no --wrap.
extern "C" {
int SetEnvConfig(const char *key, const char *value, int userId);
int GetEnvConfig(const char *key, char *value, int size, int userId);
int RemoveEnvConfig(const char *key, int userId);
}

namespace {
class DeviceInfoCapiUnitTest : public testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ---------- nullptr / size guards ----------

// SetEnvConfig: key == nullptr → SYSPARAM_INVALID_INPUT
HWTEST_F(DeviceInfoCapiUnitTest, SetEnvConfig_NullKey_InvalidInput, TestSize.Level1)
{
    EXPECT_EQ(SetEnvConfig(nullptr, "v", 100), SYSPARAM_INVALID_INPUT);
}

// SetEnvConfig: value == nullptr → SYSPARAM_INVALID_INPUT (proves the || does
// not short-circuit incorrectly)
HWTEST_F(DeviceInfoCapiUnitTest, SetEnvConfig_NullValue_InvalidInput, TestSize.Level1)
{
    EXPECT_EQ(SetEnvConfig("K", nullptr, 100), SYSPARAM_INVALID_INPUT);
}

// GetEnvConfig: key == nullptr → SYSPARAM_INVALID_INPUT
HWTEST_F(DeviceInfoCapiUnitTest, GetEnvConfig_NullKey_InvalidInput, TestSize.Level1)
{
    char buf[16] = {0};
    EXPECT_EQ(GetEnvConfig(nullptr, buf, sizeof(buf), 100), SYSPARAM_INVALID_INPUT);
}

// GetEnvConfig: value == nullptr → SYSPARAM_INVALID_INPUT
HWTEST_F(DeviceInfoCapiUnitTest, GetEnvConfig_NullValue_InvalidInput, TestSize.Level1)
{
    EXPECT_EQ(GetEnvConfig("K", nullptr, 16, 100), SYSPARAM_INVALID_INPUT);
}

// GetEnvConfig: size <= 0 → SYSPARAM_INVALID_INPUT (covers the size<=0 operand)
HWTEST_F(DeviceInfoCapiUnitTest, GetEnvConfig_NonPositiveSize_InvalidInput, TestSize.Level1)
{
    char buf[16] = {0};
    EXPECT_EQ(GetEnvConfig("K", buf, 0, 100), SYSPARAM_INVALID_INPUT);
    EXPECT_EQ(GetEnvConfig("K", buf, -1, 100), SYSPARAM_INVALID_INPUT);
}

// RemoveEnvConfig: key == nullptr → SYSPARAM_INVALID_INPUT
HWTEST_F(DeviceInfoCapiUnitTest, RemoveEnvConfig_NullKey_InvalidInput, TestSize.Level1)
{
    EXPECT_EQ(RemoveEnvConfig(nullptr, 100), SYSPARAM_INVALID_INPUT);
}

// ---------- kits-call branch (real path) ----------
// Valid args → instance.SetEnvConfig → kits → samgr. Under no device/SA the
// kits path returns non-zero, which the C API forwards. Covers the
// `instance.SetEnvConfig(...)` branch in device_info.cpp.
//
// Not asserting a specific code: kits returns -1 (GetService nullptr) or
// PERMISSION_DENIED depending on samgr availability in the ut env; both prove
// the kits-call branch was taken. EXPECT_NE(ret, SYSPARAM_INVALID_INPUT) proves
// the guard passed and execution went to kits.

HWTEST_F(DeviceInfoCapiUnitTest, SetEnvConfig_KitsPath_NonZero, TestSize.Level1)
{
    int ret = SetEnvConfig("VALID_KEY", "v", 100);
    EXPECT_NE(ret, 0);
    EXPECT_NE(ret, SYSPARAM_INVALID_INPUT);
}

HWTEST_F(DeviceInfoCapiUnitTest, GetEnvConfig_KitsPath_NonZero, TestSize.Level1)
{
    char buf[64] = {0};
    int ret = GetEnvConfig("VALID_KEY", buf, sizeof(buf), 100);
    EXPECT_NE(ret, 0);
    EXPECT_NE(ret, SYSPARAM_INVALID_INPUT);
}

HWTEST_F(DeviceInfoCapiUnitTest, RemoveEnvConfig_KitsPath_NonZero, TestSize.Level1)
{
    int ret = RemoveEnvConfig("VALID_KEY", 100);
    EXPECT_NE(ret, 0);
    EXPECT_NE(ret, SYSPARAM_INVALID_INPUT);
}
} // namespace
