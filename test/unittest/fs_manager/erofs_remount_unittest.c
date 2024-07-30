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
class ErofsRemountUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(ErofsRemountUnitTest, Init_GetRemountResult_001, TestSize.Level0)
{
    int ret = GetRemountResult();
    EXPECT_EQ(ret, true);
}

HWTEST_F(ErofsRemountUnitTest, Init_CheckIsExt4_001, TestSize.Level0)
{
    bool result = true;
    SetRemountResultFlag(result);
}

HWTEST_F(ErofsRemountUnitTest, Init_RemountOverlay_001, TestSize.Level0)
{
    int ret = RemountOverlay();
    EXPECT_EQ(ret, true);
}

HWTEST_F(ErofsRemountUnitTest, Init_MountOverlayOne_001, TestSize.Level0)
{
    const char *mnt;
    int ret = MountOverlayOne(mnt);
    EXPECT_EQ(ret, true);
}

HWTEST_F(ErofsRemountUnitTest, Init_OverlayRemountVendorPre_001, TestSize.Level0)
{
    OverlayRemountVendorPre();
}

HWTEST_F(ErofsRemountUnitTest, Init_OverlayRemountVendorPost_001, TestSize.Level0)
{
    OverlayRemountVendorPost();
}
}