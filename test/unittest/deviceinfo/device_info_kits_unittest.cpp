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
#include <mutex>

#include "device_info_kits.h"
#include "idevice_info.h"
#include "sysparam_errno.h"

using namespace testing::ext;
using namespace OHOS;

namespace init_ut {
class DeviceInfoKitsUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

HWTEST_F(DeviceInfoKitsUnitTest, DeviceInfoKits_GetInstance_001, TestSize.Level1)
{
    (void)device_info::DeviceInfoKits::GetInstance();
}

HWTEST_F(DeviceInfoKitsUnitTest, DeviceInfoKits_GetUGetdid_001, TestSize.Level1)
{
    device_info::DeviceInfoKits &instance = device_info::DeviceInfoKits::GetInstance();
    std::string result;
    int32_t ret = instance.GetUdid(result);
    EXPECT_EQ(ret, SYSPARAM_PERMISSION_DENIED);
}

HWTEST_F(DeviceInfoKitsUnitTest, DeviceInfoKitsKits_GetSerialID_001, TestSize.Level1)
{
    device_info::DeviceInfoKits &instance = device_info::DeviceInfoKits::GetInstance();
    std::string result;
    int32_t ret = instance.GetSerialID(result);
    EXPECT_EQ(ret, SYSPARAM_PERMISSION_DENIED);
}

HWTEST_F(DeviceInfoKitsUnitTest, DeviceInfoKits_GetDiskSN_001, TestSize.Level1)
{
    device_info::DeviceInfoKits &instance = device_info::DeviceInfoKits::GetInstance();
    std::string result;
    int32_t ret = instance.GetDiskSN(result);
    EXPECT_EQ(ret, SYSPARAM_PERMISSION_DENIED);
}

} // namespace init_ut