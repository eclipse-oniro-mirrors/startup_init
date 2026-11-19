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

// Unit tests for the env-config path in device_info_kits.cpp
// (DeviceInfoKits::SetEnvConfig/GetEnvConfig/RemoveEnvConfig, defined under
// #ifdef CUSTOM_SANDBOX).
//
// This target has NO --wrap: kits env methods are C++ member functions (mangled
// symbols), not intercepted by --wrap anyway. Calling kits.SetEnvConfig etc.
// reaches the real implementations in device_info_kits.cpp. Coverage attributes
// to device_info_kits.cpp.
//
// Coverage scope:
//   1. GetService failure branch (no mock injected, samgr unreachable):
//      DINFO_CHECK `return -1` (L204/L219/L234). EXPECT_NE(ret, 0).
//   2. NOT_FOUND_SERVICE_ERROR_NUMBER retry branch (mock injected):
//      `if (ret == NOT_FOUND_SERVICE_ERROR_NUMBER)` (L206/L221/L236) →
//      RetryGetService → retry call → return. Covers L207-L212/L222-L227/
//      L237-L242 fully.
//
// Mock injection: DeviceInfoKits::deviceInfoService_ is a private member made
// accessible by the target's -Dprivate=public cflag. We set it to a
// MockIDeviceInfo whose env methods return 29189 (NOT_FOUND_SERVICE_ERROR_NUMBER)
// so the retry branch fires. AsObject() returns nullptr so RetryGetService's
// AsObject() call is a no-op for the deathRecipient_ block (deathRecipient_ is
// nullptr by default), and GetService returns the mock again (LoadDeviceInfoSa
// fast path: deviceInfoService_ != nullptr → return).
//
// Out of scope: the success path (deviceService->SetEnvConfig returning 0).
// The mock could return 0, but that only skips the retry branch — the success
// path is a single `return ret` line already covered by the retry cases above.
// The real business logic on the success path is covered on the SA side by
// device_info_appenv_unittest.cpp.

#include <gtest/gtest.h>
#include <string>

#include "device_info_kits.h"
#include "mock_idevice_info.h"
#include "sysparam_errno.h"

using namespace testing::ext;
using namespace OHOS;

namespace {
class DeviceInfoKitsEnvUnitTest : public testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        // Restore nullptr so subsequent cases without a mock injection see the
        // real (samgr-unreachable) GetService path. The kits singleton is
        // process-wide; leaking the mock sptr across cases would let other
        // cases' GetService return the mock instead of hitting samgr.
        auto &kits = OHOS::device_info::DeviceInfoKits::GetInstance();
        kits.deviceInfoService_ = nullptr;
    }
};

// ---------- GetService failure branch (no mock, samgr unreachable) ----------
// ut env samgr unreachable → GetSystemAbilityManager returns nullptr →
// LoadDeviceInfoSa returns early → deviceInfoService_ stays nullptr →
// DINFO_CHECK `return -1` (L204/L219/L234).

HWTEST_F(DeviceInfoKitsEnvUnitTest, KitsSetEnvConfig_NoService_Fail, TestSize.Level1)
{
    auto &kits = OHOS::device_info::DeviceInfoKits::GetInstance();
    kits.deviceInfoService_ = nullptr;
    int32_t ret = kits.SetEnvConfig("VALID_KEY", "v", 100);
    EXPECT_NE(ret, 0);
}

HWTEST_F(DeviceInfoKitsEnvUnitTest, KitsGetEnvConfig_NoService_Fail, TestSize.Level1)
{
    auto &kits = OHOS::device_info::DeviceInfoKits::GetInstance();
    kits.deviceInfoService_ = nullptr;
    std::string val;
    int32_t ret = kits.GetEnvConfig("VALID_KEY", val, 100);
    EXPECT_NE(ret, 0);
}

HWTEST_F(DeviceInfoKitsEnvUnitTest, KitsRemoveEnvConfig_NoService_Fail, TestSize.Level1)
{
    auto &kits = OHOS::device_info::DeviceInfoKits::GetInstance();
    kits.deviceInfoService_ = nullptr;
    int32_t ret = kits.RemoveEnvConfig("VALID_KEY", 100);
    EXPECT_NE(ret, 0);
}

// ---------- NOT_FOUND_SERVICE_ERROR_NUMBER retry branch (mock injected) ----------
// mock.envRet = 29189 → deviceService->SetEnvConfig returns 29189 →
// `if (ret == NOT_FOUND_SERVICE_ERROR_NUMBER)` fires → RetryGetService →
// retry call returns 29189 → return 29189. Covers L206-L212/L221-L227/L236-L242.

HWTEST_F(DeviceInfoKitsEnvUnitTest, KitsSetEnvConfig_NotFoundServiceRetry_29189, TestSize.Level1)
{
    auto &kits = OHOS::device_info::DeviceInfoKits::GetInstance();
    auto mock = sptr<init_ut::MockIDeviceInfo>(new init_ut::MockIDeviceInfo());
    mock->envRet = 29189;  // NOT_FOUND_SERVICE_ERROR_NUMBER
    kits.deviceInfoService_ = mock;
    int32_t ret = kits.SetEnvConfig("VALID_KEY", "v", 100);
    EXPECT_EQ(ret, 29189);
}

HWTEST_F(DeviceInfoKitsEnvUnitTest, KitsGetEnvConfig_NotFoundServiceRetry_29189, TestSize.Level1)
{
    auto &kits = OHOS::device_info::DeviceInfoKits::GetInstance();
    auto mock = sptr<init_ut::MockIDeviceInfo>(new init_ut::MockIDeviceInfo());
    mock->envRet = 29189;
    kits.deviceInfoService_ = mock;
    std::string val;
    int32_t ret = kits.GetEnvConfig("VALID_KEY", val, 100);
    EXPECT_EQ(ret, 29189);
}

HWTEST_F(DeviceInfoKitsEnvUnitTest, KitsRemoveEnvConfig_NotFoundServiceRetry_29189, TestSize.Level1)
{
    auto &kits = OHOS::device_info::DeviceInfoKits::GetInstance();
    auto mock = sptr<init_ut::MockIDeviceInfo>(new init_ut::MockIDeviceInfo());
    mock->envRet = 29189;
    kits.deviceInfoService_ = mock;
    int32_t ret = kits.RemoveEnvConfig("VALID_KEY", 100);
    EXPECT_EQ(ret, 29189);
}

// ---------- Success path (mock returns 0, no retry) ----------
// Covers the non-retry branch: `if (ret == NOT_FOUND...)` is false → skip retry
// → return ret (=0). Together with the retry cases above, both arms of the
// `if` are covered for each of the three methods.

HWTEST_F(DeviceInfoKitsEnvUnitTest, KitsSetEnvConfig_Success_0, TestSize.Level1)
{
    auto &kits = OHOS::device_info::DeviceInfoKits::GetInstance();
    auto mock = sptr<init_ut::MockIDeviceInfo>(new init_ut::MockIDeviceInfo());
    mock->envRet = 0;
    kits.deviceInfoService_ = mock;
    int32_t ret = kits.SetEnvConfig("VALID_KEY", "v", 100);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(DeviceInfoKitsEnvUnitTest, KitsGetEnvConfig_Success_0, TestSize.Level1)
{
    auto &kits = OHOS::device_info::DeviceInfoKits::GetInstance();
    auto mock = sptr<init_ut::MockIDeviceInfo>(new init_ut::MockIDeviceInfo());
    mock->envRet = 0;
    kits.deviceInfoService_ = mock;
    std::string val;
    int32_t ret = kits.GetEnvConfig("VALID_KEY", val, 100);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(DeviceInfoKitsEnvUnitTest, KitsRemoveEnvConfig_Success_0, TestSize.Level1)
{
    auto &kits = OHOS::device_info::DeviceInfoKits::GetInstance();
    auto mock = sptr<init_ut::MockIDeviceInfo>(new init_ut::MockIDeviceInfo());
    mock->envRet = 0;
    kits.deviceInfoService_ = mock;
    int32_t ret = kits.RemoveEnvConfig("VALID_KEY", 100);
    EXPECT_EQ(ret, 0);
}
} // namespace
