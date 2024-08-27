/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>
#include "dm_verity.h"

using namespace testing::ext;
using namespace std;

namespace init_ut {
class DmVerifyUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(DmVerifyUnitTest, HvbDmVerityinit_001, TestSize.Level0)
{
    int ret;
    FstabItem fstabitem = {(char *)"deviceName", (char *)"mountPoint",
        (char *)"fsType", (char *)"mountOptions", 1, nullptr};
    Fstab fstab = {&fstabitem};

    ret = HvbDmVerityinit(&fstab);
    EXPECT_EQ(ret, 0);

    setenv("SWTYPE_VALUE", "Test", 1);
    ret = HvbDmVerityinit(&fstab);
    EXPECT_EQ(ret, 0);

    setenv("ENABLE_VALUE", "false", 1);
    ret = HvbDmVerityinit(&fstab);
    EXPECT_EQ(ret, 0);
    setenv("ENABLE_VALUE", "FALSE", 1);
    ret = HvbDmVerityinit(&fstab);
    EXPECT_EQ(ret, 0);

    setenv("ENABLE_VALUE", "orange", 1);
    ret = HvbDmVerityinit(&fstab);
    EXPECT_EQ(ret, 0);

    setenv("ENABLE_VALUE", "ORANGE", 1);
    ret = HvbDmVerityinit(&fstab);
    EXPECT_EQ(ret, 0);

    unsetenv("SWTYPE_VALUE");
    unsetenv("ENABLE_VALUE");
}

HWTEST_F(DmVerifyUnitTest, HvbDmVerityinit_002, TestSize.Level0)
{
    int ret;
    FstabItem fstabitem = {(char *)"deviceName", (char *)"mountPoint",
        (char *)"fsType", (char *)"mountOptions", 1, nullptr};
    Fstab fstab = {&fstabitem};

    ret = HvbDmVerityinit(&fstab);
    EXPECT_EQ(ret, 0);

    setenv("SWTYPE_VALUE", "factory", 1);
    ret = HvbDmVerityinit(&fstab);
    EXPECT_EQ(ret, 0);

    fstabitem.fsManagerFlags = 0x00000010;
    ret = HvbDmVerityinit(&fstab);
    EXPECT_EQ(ret, -1);

    setenv("INIT_VALUE", "on", 1);
    ret = HvbDmVerityinit(&fstab);
    EXPECT_EQ(ret, 0);

    unsetenv("SWTYPE_VALUE");
    unsetenv("INIT_VALUE");
}

HWTEST_F(DmVerifyUnitTest, HvbDmVeritySetUp_001, TestSize.Level0)
{
    int ret;
    FstabItem fstabitem = {(char *)"deviceName", (char *)"mountPoint",
        (char *)"fsType", (char *)"mountOptions", 1, nullptr};

    ret = HvbDmVeritySetUp(&fstabitem);
    EXPECT_EQ(ret, 0);

    setenv("SWTYPE_VALUE", "factory", 1);
    ret = HvbDmVeritySetUp(nullptr);
    EXPECT_EQ(ret, -1);
    fstabitem.fsManagerFlags = 0x00000009;
    ret = HvbDmVeritySetUp(&fstabitem);
    EXPECT_EQ(ret, 0);

    fstabitem.fsManagerFlags = 0x00000010;
    ret = HvbDmVeritySetUp(&fstabitem);
    EXPECT_EQ(ret, -1);
    fstabitem.fsManagerFlags = 0x00000018;
    ret = HvbDmVeritySetUp(&fstabitem);
    EXPECT_EQ(ret, -1);

    setenv("HASH_VALUE", "on", 1);
    ret = HvbDmVeritySetUp(&fstabitem);
    EXPECT_EQ(ret, 0);

    unsetenv("SWTYPE_VALUE");
    unsetenv("HASH_VALUE");
}

HWTEST_F(DmVerifyUnitTest, HvbDmVerityFinal_001, TestSize.Level0)
{
    HvbDmVerityFinal();

    setenv("SWTYPE_VALUE", "factory", 1);
    HvbDmVerityFinal();

    setenv("FINAL_VALUE", "on", 1);
    HvbDmVerityFinal();

    unsetenv("SWTYPE_VALUE");
    unsetenv("FINAL_VALUE");
}
}  // namespace init_ut