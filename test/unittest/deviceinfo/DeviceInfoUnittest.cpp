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
#include <string>
#include <iostream>
#include <gtest/gtest.h>
#include "parameter.h"
#include "system_ability_definition.h"
#include "if_system_ability_manager.h"
#include "iservice_registry.h"
#include "parcel.h"
#include "string_ex.h"
#include "device_info_kits.h"
#include "idevice_info.h"
#include "device_info_stub.h"
using namespace testing::ext;
using namespace std;
using namespace OHOS;
using namespace OHOS::device_info;

const int UDID_LEN = 65;
namespace init_ut {
using DeviceInfoServicePtr = DeviceInfoService *;
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
            deviceInfoServicePtr = new DeviceInfoService(0, true);
            if (deviceInfoServicePtr == nullptr) {
                return nullptr;
            }
            deviceInfoServicePtr->OnStart();
        }
        return deviceInfoServicePtr;
    }
};

HWTEST_F(DeviceInfoUnittest, GetDevUdidTest, TestSize.Level1)
{
    char localDeviceId[UDID_LEN] = {0};
    AclGetDevUdid(localDeviceId, UDID_LEN);
    const char *serialNumber = AclGetSerial();
    EXPECT_NE(nullptr, serialNumber);

    sptr<ISystemAbilityManager> samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    ASSERT_NE(nullptr, samgr);
    sptr<IRemoteObject> object = samgr->GetSystemAbility(SYSPARAM_DEVICE_SERVICE_ID);
    ASSERT_NE(nullptr, samgr);
}
HWTEST_F(DeviceInfoUnittest, StubTest, TestSize.Level1)
{
    string result;
    DeviceInfoServicePtr deviceInfoService = GetDeviceInfoService();
    ASSERT_NE(deviceInfoService, nullptr);
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    data.WriteInterfaceToken(DeviceInfoStub::GetDescriptor());
    deviceInfoService->OnRemoteRequest(IDeviceInfo::COMMAND_GET_UDID, data, reply, option);
    data.WriteInterfaceToken(DeviceInfoStub::GetDescriptor());
    deviceInfoService->OnRemoteRequest(IDeviceInfo::COMMAND_GET_SERIAL_ID, data, reply, option);
    deviceInfoService->GetUdid(result);
    deviceInfoService->GetSerialID(result);
    deviceInfoService->OnStop();
}
}  // namespace init_ut
