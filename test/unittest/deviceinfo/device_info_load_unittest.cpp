/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under * Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with * License.
 * You may obtain a copy of * License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under * License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <string>

#include "device_info_load.h"
#include "idevice_info.h"
#include "system_ability_definition.h"

using namespace testing::ext;
using namespace OHOS;

namespace init_ut {
class DeviceInfoLoadUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

HWTEST_F(DeviceInfoLoadUnitTest, DeviceInfoLoad_Constructor_001, TestSize.Level1)
{
    device_info::DeviceInfoLoad load;
}

HWTEST_F(DeviceInfoLoadUnitTest, DeviceInfoLoad_OnLoadSystemAbilitySuccess_001, TestSize.Level1)
{
    device_info::DeviceInfoLoad load;
    load.OnLoadSystemAbilitySuccess(SYSPARAM_DEVICE_SERVICE_ID, nullptr);
}

HWTEST_F(DeviceInfoLoadUnitTest, DeviceInfoLoad_OnLoadSystemAbilitySuccess_002, TestSize.Level1)
{
    device_info::DeviceInfoLoad load;
    sptr<IRemoteObject> mockObject = nullptr;
    load.OnLoadSystemAbilitySuccess(SYSPARAM_DEVICE_SERVICE_ID, mockObject);
}

HWTEST_F(DeviceInfoLoadUnitTest, DeviceInfoLoad_OnLoadSystemAbilityFail_001, TestSize.Level1)
{
    device_info::DeviceInfoLoad load;
    load.OnLoadSystemAbilityFail(SYSPARAM_DEVICE_SERVICE_ID);
}

} // namespace init_ut