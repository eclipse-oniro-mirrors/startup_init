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

#include "erofs_overlay_common.h"
#include "securec.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
class ErofsOverlayUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(ErofsOverlayUnitTest, Init_IsOverlayEnable_001, TestSize.Level0)
{
    bool ret = IsOverlayEnable();
    EXPECT_EQ(ret, true);
}

HWTEST_F(ErofsOverlayUnitTest, Init_CheckIsExt4_001, TestSize.Level0)
{
    const char *dev;
    uint64_t offset;
    bool ret = CheckIsExt4(dev, offset);
    EXPECT_EQ(ret, true);
}

HWTEST_F(ErofsOverlayUnitTest, Init_CheckIsErofs_001, TestSize.Level0)
{
    const char *dev;
    bool ret = CheckIsErofs(dev);
    EXPECT_EQ(ret, true);
}
}