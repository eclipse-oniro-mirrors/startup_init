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
#include <chrono>
#include <thread>
#include <string>
#include <iostream>
#include <gtest/gtest.h>

#include "accesstoken_kit.h"
#include "parameter.h"
#include "system_ability_definition.h"
#include "if_system_ability_manager.h"
#include "iservice_registry.h"
#include "parcel.h"
#include "string_ex.h"
#include "device_info_kits.h"
#include "device_info_load.h"
#include "device_info_proxy.h"
#include "idevice_info.h"
#include "device_info_stub.h"
#include "sysparam_errno.h"
#include "deviceinfoservice_ipc_interface_code.h"

using namespace testing::ext;
using namespace std;
using namespace OHOS;

static int g_tokenType = OHOS::Security::AccessToken::TOKEN_HAP;
static int g_tokenVerifyResult = 0;
namespace OHOS {
namespace Security {
namespace AccessToken {
ATokenTypeEnum AccessTokenKit::GetTokenTypeFlag(AccessTokenID tokenID)
{
    return static_cast<ATokenTypeEnum>(g_tokenType);
}
int AccessTokenKit::VerifyAccessToken(AccessTokenID tokenID, const std::string& permissionName)
{
    return g_tokenVerifyResult;
}
} // namespace AccessToken
} // namespace Security
} // namespace OHOS

const int UDID_LEN = 65;
namespace init_ut {
using DeviceInfoServicePtr = OHOS::device_info::DeviceInfoService *;
class DeviceInfoUnittest : public testing::Test {
public:
    DeviceInfoUnittest() {};
    virtual ~DeviceInfoUnittest() {};
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
    void TestBody(void) {};
    DeviceInfoServicePtr GetDeviceInfoService()
    {
        static DeviceInfoServicePtr deviceInfoServicePtr = nullptr;
        if (deviceInfoServicePtr == nullptr) {
            deviceInfoServicePtr = new OHOS::device_info::DeviceInfoService(0, true);
            if (deviceInfoServicePtr == nullptr) {
                return nullptr;
            }
            deviceInfoServicePtr->OnStart();
        }
        return deviceInfoServicePtr;
    }
};

HWTEST_F(DeviceInfoUnittest, DevInfoAgentTest, TestSize.Level1)
{
    OHOS::device_info::DeviceInfoKits &kits = OHOS::device_info::DeviceInfoKits::GetInstance();
    std::string serial = {};
    int ret = kits.GetSerialID(serial);
    EXPECT_EQ(ret, SYSPARAM_PERMISSION_DENIED);
    ret = kits.GetUdid(serial);
    EXPECT_EQ(ret, SYSPARAM_PERMISSION_DENIED);
}

HWTEST_F(DeviceInfoUnittest, DevInfoDiedTest, TestSize.Level1)
{
    sptr<ISystemAbilityManager> samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    DINFO_CHECK(samgr != nullptr, return, "Get samgr failed");
    sptr<IRemoteObject> object = samgr->GetSystemAbility(SYSPARAM_DEVICE_SERVICE_ID);
    DINFO_CHECK(object != nullptr, return, "Get deviceinfo manager object from samgr failed");
    OHOS::device_info::DeviceInfoKits &kits = OHOS::device_info::DeviceInfoKits::GetInstance();
    if (kits.GetDeathRecipient() != nullptr) {
        kits.GetDeathRecipient()->OnRemoteDied(object);
    }
    std::string serial = {};
    int ret = kits.GetSerialID(serial);
    EXPECT_EQ(ret, SYSPARAM_PERMISSION_DENIED);
}

HWTEST_F(DeviceInfoUnittest, DevInfoAgentFail, TestSize.Level1)
{
    sptr<OHOS::device_info::DeviceInfoLoad> deviceInfoLoad = new (std::nothrow) OHOS::device_info::DeviceInfoLoad();
    ASSERT_NE(deviceInfoLoad, nullptr);
    deviceInfoLoad->OnLoadSystemAbilityFail(SYSPARAM_DEVICE_SERVICE_ID);
    deviceInfoLoad->OnLoadSystemAbilityFail(SYSPARAM_DEVICE_SERVICE_ID + 1);

    OHOS::device_info::DeviceInfoKits &kits = OHOS::device_info::DeviceInfoKits::GetInstance();
    kits.FinishStartSAFailed();
}

HWTEST_F(DeviceInfoUnittest, DeviceInfoServiceInvalidTokenTest, TestSize.Level1)
{
    string result;
    DeviceInfoServicePtr deviceInfoService = GetDeviceInfoService();
    ASSERT_NE(deviceInfoService, nullptr);
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    g_tokenType = OHOS::Security::AccessToken::TOKEN_INVALID;
    data.WriteInterfaceToken(OHOS::device_info::DeviceInfoStub::GetDescriptor());
    deviceInfoService->OnRemoteRequest
        (static_cast<uint32_t> (OHOS::device_info::DeviceInfoInterfaceCode::COMMAND_GET_UDID), data, reply, option);
}

HWTEST_F(DeviceInfoUnittest, DeviceInfoServiceFailTest, TestSize.Level1)
{
    string result;
    DeviceInfoServicePtr deviceInfoService = GetDeviceInfoService();
    ASSERT_NE(deviceInfoService, nullptr);
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    g_tokenType = OHOS::Security::AccessToken::TOKEN_HAP;
    g_tokenVerifyResult = OHOS::Security::AccessToken::TypePermissionState::PERMISSION_DENIED;

    // udid
    data.WriteInterfaceToken(OHOS::device_info::DeviceInfoStub::GetDescriptor());
    deviceInfoService->OnRemoteRequest
        (static_cast<uint32_t> (OHOS::device_info::DeviceInfoInterfaceCode::COMMAND_GET_UDID),
        data, reply, option);
    // serial
    data.WriteInterfaceToken(OHOS::device_info::DeviceInfoStub::GetDescriptor());
    deviceInfoService->OnRemoteRequest
        (static_cast<uint32_t> (OHOS::device_info::DeviceInfoInterfaceCode::COMMAND_GET_SERIAL_ID),
        data, reply, option);
}

HWTEST_F(DeviceInfoUnittest, DeviceInfoServiceTest, TestSize.Level1)
{
    string result;
    DeviceInfoServicePtr deviceInfoService = GetDeviceInfoService();
    ASSERT_NE(deviceInfoService, nullptr);
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    g_tokenType = OHOS::Security::AccessToken::TOKEN_HAP;
    g_tokenVerifyResult = OHOS::Security::AccessToken::TypePermissionState::PERMISSION_GRANTED;
    data.WriteInterfaceToken(OHOS::device_info::DeviceInfoStub::GetDescriptor());
    deviceInfoService->OnRemoteRequest
        (static_cast<uint32_t> (OHOS::device_info::DeviceInfoInterfaceCode::COMMAND_GET_UDID),
        data, reply, option);
    data.WriteInterfaceToken(OHOS::device_info::DeviceInfoStub::GetDescriptor());
    deviceInfoService->OnRemoteRequest
        (static_cast<uint32_t> (OHOS::device_info::DeviceInfoInterfaceCode::COMMAND_GET_SERIAL_ID),
        data, reply, option);
    data.WriteInterfaceToken(OHOS::device_info::DeviceInfoStub::GetDescriptor());
    deviceInfoService->OnRemoteRequest
        (static_cast<uint32_t> (OHOS::device_info::DeviceInfoInterfaceCode::COMMAND_GET_SERIAL_ID) + 1,
        data, reply, option);

    deviceInfoService->OnRemoteRequest
        (static_cast<uint32_t> (OHOS::device_info::DeviceInfoInterfaceCode::COMMAND_GET_SERIAL_ID) + 1,
        data, reply, option);
    std::this_thread::sleep_for(std::chrono::seconds(3)); // wait sa unload 3s
    deviceInfoService->GetUdid(result);
    deviceInfoService->GetSerialID(result);
    deviceInfoService->OnStop();
    std::vector<std::u16string> args = {};
    deviceInfoService->Dump(STDOUT_FILENO, args);
    deviceInfoService->Dump(-1, args);
}

HWTEST_F(DeviceInfoUnittest, TestInterface, TestSize.Level1)
{
    char localDeviceId[UDID_LEN] = {0};
    int ret = AclGetDevUdid(nullptr, UDID_LEN);
    ASSERT_NE(ret, 0);
    ret = AclGetDevUdid(localDeviceId, 2); // 2 test
    ASSERT_NE(ret, 0);

    ret = AclGetDevUdid(localDeviceId, UDID_LEN);
    const char *serialNumber = AclGetSerial();
    EXPECT_NE(nullptr, serialNumber);
}

HWTEST_F(DeviceInfoUnittest, TestDeviceInfoProxy1, TestSize.Level1)
{
    auto remotePtr = device_info::DeviceInfoKits::GetInstance().GetService();
    ASSERT_NE(remotePtr, nullptr);
    auto remote = remotePtr->AsObject();
    sptr<device_info::DeviceInfoProxy> proxy = new(std::nothrow) device_info::DeviceInfoProxy(remote);
    ASSERT_NE(proxy, nullptr);

    device_info::DeviceInfoKits::GetInstance().FinishStartSASuccess(proxy->AsObject());
    std::string udid;
    std::string serialId;
    proxy->GetUdid(udid);
    proxy->GetSerialID(serialId);

    char localDeviceId[UDID_LEN] = {0};
    (void)AclGetDevUdid(localDeviceId, UDID_LEN);
    const char *serialNumber = AclGetSerial();
    EXPECT_NE(nullptr, serialNumber);
}

HWTEST_F(DeviceInfoUnittest, TestDeviceInfoProxy2, TestSize.Level1)
{
    sptr<device_info::DeviceInfoProxy> proxy = new(std::nothrow) device_info::DeviceInfoProxy(nullptr);
    ASSERT_NE(proxy, nullptr);

    std::string udid;
    std::string serialId;
    proxy->GetUdid(udid);
    proxy->GetSerialID(serialId);
}
}  // namespace init_ut
