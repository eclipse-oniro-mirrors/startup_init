/*
* Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "erofs_mount_overlay.h"
#include "securec.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
class ErofsMountUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(ErofsMountUnitTest, Init_DoMountOverlayDevice_001, TestSize.Level0)
{
    FstabItem *item;
    int ret = DoMountOverlayDevice(item);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(ErofsMountUnitTest, Init_MountExt4Device_001, TestSize.Level0)
{
    const char *dev;
    const char *mnt;
    bool isFirstMount = true;
    int ret = MountExt4Device(dev, mnt, isFirstMount);
    EXPECT_EQ(ret, 0);
}
}