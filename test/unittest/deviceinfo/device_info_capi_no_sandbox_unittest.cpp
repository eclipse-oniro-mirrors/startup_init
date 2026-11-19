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

// Unit tests for the non-CUSTOM_SANDBOX stubs of the env-config C API in
// device_info.cpp (L312-327 `#else // !CUSTOM_SANDBOX`).
//
// Why this file exists: on non-PC products (CUSTOM_SANDBOX not defined), the
// three C API functions are stubs that return SYSPARAM_PERMISSION_DENIED for
// ABI stability (versionscript always exports the symbols). The PC
// (CUSTOM_SANDBOX) implementations are covered by device_info_capi_ut; without
// this target the non-PC stubs had 0% coverage.
//
// This target does NOT define CUSTOM_SANDBOX, so device_info.cpp compiles the
// `#else` branch (L312-327). PARAM_FEATURE_DEVICEINFO is also left undefined so
// no kits path is pulled in (the stubs are pure returns). STARTUP_INIT_TEST is
// defined to keep INIT_STATIC consistent with sibling targets.

#include <cstring>
#include <gtest/gtest.h>

#include "sysparam_errno.h"

using namespace testing::ext;

// C API declarations (device_info.cpp defines them extern "C" under the
// !CUSTOM_SANDBOX branch; this target does not define CUSTOM_SANDBOX).
extern "C" {
int SetEnvConfig(const char *key, const char *value, int userId);
int GetEnvConfig(const char *key, char *value, int size, int userId);
int RemoveEnvConfig(const char *key, int userId);
}

namespace {
class DeviceInfoCapiNoSandboxUnitTest : public testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// All three stubs return SYSPARAM_PERMISSION_DENIED unconditionally. Even with
// valid args (no guard) the stub returns PERMISSION_DENIED — this is the
// contract for non-PC products (appenv CLI is not built there; the symbols
// exist only for ABI stability).

HWTEST_F(DeviceInfoCapiNoSandboxUnitTest, SetEnvConfig_Stub_ReturnsPermissionDenied, TestSize.Level1)
{
    EXPECT_EQ(SetEnvConfig("K", "v", 100), SYSPARAM_PERMISSION_DENIED);
}

HWTEST_F(DeviceInfoCapiNoSandboxUnitTest, GetEnvConfig_Stub_ReturnsPermissionDenied, TestSize.Level1)
{
    char buf[16] = {0};
    EXPECT_EQ(GetEnvConfig("K", buf, sizeof(buf), 100), SYSPARAM_PERMISSION_DENIED);
}

HWTEST_F(DeviceInfoCapiNoSandboxUnitTest, RemoveEnvConfig_Stub_ReturnsPermissionDenied, TestSize.Level1)
{
    EXPECT_EQ(RemoveEnvConfig("K", 100), SYSPARAM_PERMISSION_DENIED);
}
} // namespace
