/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <thread>
#include <gtest/gtest.h>
#include <cstdint>

#include "beget_ext.h"
#include "if_system_ability_manager.h"
#include "iservice_registry.h"
#include "idevice_info.h"
#include "parameter.h"
#include "system_ability_definition.h"
#include "sysparam_errno.h"
#include "deviceinfo.h"

using namespace std;
using namespace testing::ext;
using namespace OHOS;

namespace initModuleTest {
class DeviceInfoModuleTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(DeviceInfoModuleTest, DeviceInfoGetUdid_001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "DeviceInfoGetUdid_001 start";
    char udid[65] = {}; // 65 udid len
    int ret = AclGetDevUdid(udid, sizeof(udid));
    EXPECT_EQ(ret, SYSPARAM_PERMISSION_DENIED);

    sptr<ISystemAbilityManager> samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    BEGET_ERROR_CHECK(samgr != nullptr, return, "Get samgr failed");
    sptr<IRemoteObject> object = samgr->GetSystemAbility(SYSPARAM_DEVICE_SERVICE_ID);
    BEGET_ERROR_CHECK(object != nullptr, return, "Get deviceinfo manager object from samgr failed");

    std::this_thread::sleep_for(std::chrono::seconds(20)); // wait sa died 20s

    object = samgr->GetSystemAbility(SYSPARAM_DEVICE_SERVICE_ID);
    BEGET_ERROR_CHECK(object == nullptr, return, "Get deviceinfo manager object from samgr failed");

    ret = AclGetDevUdid(udid, sizeof(udid));
    EXPECT_EQ(ret, SYSPARAM_PERMISSION_DENIED);

    object = samgr->GetSystemAbility(SYSPARAM_DEVICE_SERVICE_ID);
    BEGET_ERROR_CHECK(object != nullptr, return, "Get deviceinfo manager object from samgr failed");

    GTEST_LOG_(INFO) << "DeviceInfoGetUdid_001 end";
}

HWTEST_F(DeviceInfoModuleTest, DeviceInfoGetSerial_001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "DeviceInfoGetSerial_001 start";
    const char *serial = AclGetSerial();
    EXPECT_EQ(serial != nullptr, 1);

    sptr<ISystemAbilityManager> samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    BEGET_ERROR_CHECK(samgr != nullptr, return, "Get samgr failed");
    sptr<IRemoteObject> object = samgr->GetSystemAbility(SYSPARAM_DEVICE_SERVICE_ID);
    BEGET_ERROR_CHECK(object != nullptr, return, "Get deviceinfo manager object from samgr failed");

    std::this_thread::sleep_for(std::chrono::seconds(20)); // wait sa died 20s

    object = samgr->GetSystemAbility(SYSPARAM_DEVICE_SERVICE_ID);
    BEGET_ERROR_CHECK(object == nullptr, return, "Get deviceinfo manager object from samgr failed");

    serial = AclGetSerial();
    EXPECT_EQ(serial != nullptr, 1);

    object = samgr->GetSystemAbility(SYSPARAM_DEVICE_SERVICE_ID);
    BEGET_ERROR_CHECK(object != nullptr, return, "Get deviceinfo manager object from samgr failed");

    GTEST_LOG_(INFO) << "DeviceInfoGetSerial_001 end";
}

#ifdef PARAM_FEATURE_GET_DEVICE_SN
HWTEST_F(DeviceInfoModuleTest, DeviceInfoGetDiskSN_001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "DeviceInfoGetDiskSN_001 start";
    char diskSN[20] = {}; // 20 diskSN len
    int ret = AclGetDiskSN(diskSN, sizeof(diskSN));
    EXPECT_EQ(ret, SYSPARAM_PERMISSION_DENIED);

    sptr<ISystemAbilityManager> samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    BEGET_ERROR_CHECK(samgr != nullptr, return, "Get samgr failed");
    sptr<IRemoteObject> object = samgr->GetSystemAbility(SYSPARAM_DEVICE_SERVICE_ID);
    BEGET_ERROR_CHECK(object != nullptr, return, "Get deviceinfo manager object from samgr failed");

    std::this_thread::sleep_for(std::chrono::seconds(20)); // wait sa died 20s

    object = samgr->GetSystemAbility(SYSPARAM_DEVICE_SERVICE_ID);
    BEGET_ERROR_CHECK(object == nullptr, return, "Get deviceinfo manager object from samgr failed");

    ret = AclGetDiskSN(diskSN, sizeof(diskSN));
    EXPECT_EQ(ret, SYSPARAM_PERMISSION_DENIED);

    object = samgr->GetSystemAbility(SYSPARAM_DEVICE_SERVICE_ID);
    BEGET_ERROR_CHECK(object != nullptr, return, "Get deviceinfo manager object from samgr failed");

    GTEST_LOG_(INFO) << "DeviceInfoGetDiskSN_001 end";
}
#endif

HWTEST_F(DeviceInfoModuleTest, DeviceInfoOHGetDiskSN_001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "DeviceInfoOHGetDiskSN_001 start";
    char diskSN[65] = {0};
    int32_t ret = OH_GetDiskSN(diskSN);
    EXPECT_TRUE(ret == 0 || ret == BOARD_INFO_PERMISSION_DENIED_CODE);
    GTEST_LOG_(INFO) << "DeviceInfoOHGetDiskSN_001 end";
}

HWTEST_F(DeviceInfoModuleTest, DeviceInfoOHGetDiskSN_002, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "DeviceInfoOHGetDiskSN_002 start";
    int32_t ret = OH_GetDiskSN(nullptr);
    EXPECT_EQ(ret, 0);
    GTEST_LOG_(INFO) << "DeviceInfoOHGetDiskSN_002 end";
}

HWTEST_F(DeviceInfoModuleTest, DeviceInfoOHGetDiskSN_003, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "DeviceInfoOHGetDiskSN_003 start";
    char diskSN[65] = {0};
    int32_t ret1 = OH_GetDiskSN(diskSN);
    int32_t ret2 = OH_GetDiskSN(diskSN);
    EXPECT_TRUE(ret1 == 0 || ret1 == BOARD_INFO_PERMISSION_DENIED_CODE);
    EXPECT_TRUE(ret2 == 0 || ret2 == BOARD_INFO_PERMISSION_DENIED_CODE);
    EXPECT_EQ(ret1, ret2);
    GTEST_LOG_(INFO) << "DeviceInfoOHGetDiskSN_003 end";
}

HWTEST_F(DeviceInfoModuleTest, DeviceInfoOHGetOdid_001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "DeviceInfoOHGetOdid_001 start";
    const char *odid = OH_GetOdid();
    EXPECT_NE(odid, nullptr);
    GTEST_LOG_(INFO) << "DeviceInfoOHGetOdid_001 end";
}

HWTEST_F(DeviceInfoModuleTest, DeviceInfoOHGetOdid_002, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "DeviceInfoOHGetOdid_002 start";
    const char *odid1 = OH_GetOdid();
    const char *odid2 = OH_GetOdid();
    EXPECT_EQ(odid1, odid2);
    GTEST_LOG_(INFO) << "DeviceInfoOHGetOdid_002 end";
}
}