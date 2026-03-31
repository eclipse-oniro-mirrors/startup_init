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

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "accesstoken_kit.h"
#include "device_info_service.h"
#include "idevice_info.h"
#include "sysparam_errno.h"
#include "system_ability_definition.h"

using namespace testing::ext;
using namespace OHOS;

extern int g_tokenVerifyResult;

namespace init_ut {
class DeviceInfoServiceUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

HWTEST_F(DeviceInfoServiceUnitTest, DeviceInfoService_GetUdid_001, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    std::string result;
    int32_t ret = service.GetUdid(result);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(DeviceInfoServiceUnitTest, DeviceInfoService_GetSerialID_001, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    std::string result;
    int32_t ret = service.GetSerialID(result);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(DeviceInfoServiceUnitTest, DeviceInfoService_GetDiskSN_001, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    std::string result;
    int32_t ret = service.GetDiskSN(result);
    EXPECT_EQ(ret, device_info::DeviceInfoService::ERR_FAIL);
}

HWTEST_F(DeviceInfoServiceUnitTest, DeviceInfoService_CallbackEnter_001, TestSize.Level1)
{
    g_tokenVerifyResult = OHOS::Security::AccessToken::TypePermissionState::PERMISSION_DENIED;
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    int32_t ret = service.CallbackEnter(static_cast<uint32_t>(device_info::IDeviceInfoIpcCode::COMMAND_GET_UDID));
    EXPECT_EQ(ret, SYSPARAM_PERMISSION_DENIED);
}

HWTEST_F(DeviceInfoServiceUnitTest, DeviceInfoService_CallbackEnter_002, TestSize.Level1)
{
    g_tokenVerifyResult = OHOS::Security::AccessToken::TypePermissionState::PERMISSION_DENIED;
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    std::string result;
    int32_t ret = service.CallbackEnter(static_cast<uint32_t>(device_info::IDeviceInfoIpcCode::COMMAND_GET_SERIAL_I_D));
    EXPECT_EQ(ret, SYSPARAM_PERMISSION_DENIED);
}

HWTEST_F(DeviceInfoServiceUnitTest, DeviceInfoService_CallbackEnter_003, TestSize.Level1)
{
    g_tokenVerifyResult = OHOS::Security::AccessToken::TypePermissionState::PERMISSION_DENIED;
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    int32_t ret = service.CallbackEnter(static_cast<uint32_t>(device_info::IDeviceInfoIpcCode::COMMAND_GET_DISK_S_N));
    EXPECT_EQ(ret, SYSPARAM_PERMISSION_DENIED);
}

HWTEST_F(DeviceInfoServiceUnitTest, DeviceInfoService_CallbackEnter_004, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    int32_t ret = service.CallbackEnter(999);
    EXPECT_EQ(ret, SYSPARAM_PERMISSION_DENIED);
}

HWTEST_F(DeviceInfoServiceUnitTest, DeviceInfoService_CallbackExit_001, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    int32_t ret = service.CallbackExit(0, 0);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(DeviceInfoServiceUnitTest, DeviceInfoService_CheckPermission_001, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    bool ret = service.CheckPermission("ohos.permission.sec.ACCESS_UDID");
    EXPECT_FALSE(ret);
}
} // namespace init_ut