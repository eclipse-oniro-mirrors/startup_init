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

#include "fs_hvb.h"
#include "securec.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
class FsHvbUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(FsHvbUnitTest, Init_FsHvbInit_001, TestSize.Level0)
{
    int ret = FsHvbInit();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbSetupHashtree_001, TestSize.Level0)
{
    FstabItem *fsItem;
    int ret = FsHvbSetupHashtree(fsItem);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbFinal_001, TestSize.Level0)
{
    int ret = FsHvbFinal();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbGetOps_001, TestSize.Level0)
{
    FsHvbGetOps();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbGetValueFromCmdLine_001, TestSize.Level0)
{
    char *val = "test";
    size_t size = 0;
    const char *key = "test";
    int ret = FsHvbGetValueFromCmdLine(val, size, key);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbConstructVerityTarget_001, TestSize.Level0)
{
    DmVerityTarget *target;
    const char *devName;
    struct hvb_cert *cert;
    int ret = FsHvbConstructVerityTarget(target, devName, cert);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(FsHvbUnitTest, Init_FsHvbDestoryVerityTarget_001, TestSize.Level0)
{
    DmVerityTarget *target;
    FsHvbDestoryVerityTarget(target);
}
}