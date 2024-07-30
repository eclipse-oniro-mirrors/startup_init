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

#include "fs_dm.h"
#include "securec.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
class FsDmUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(FsDmUnitTest, Init_FsDmInitDmDev_001, TestSize.Level0)
{
    char *devPath = "test";
    bool useSocket = true;
    int ret = FsDmInitDmDev(devPath, useSocket);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(FsDmUnitTest, Init_FsDmCreateDevice_001, TestSize.Level0)
{
    char **dmDevPath = "test";
    const char *devName = "test";
    DmVerityTarget *target;
    int ret = FsDmCreateDevice(dmDevPath, devName, target);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(FsDmUnitTest, Init_FsDmRemoveDevice_001, TestSize.Level0)
{
    const char *devName = "test";
    int ret = FsDmRemoveDevice(devName);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(FsDmUnitTest, Init_FsDmCreateLinearDevice_001, TestSize.Level0)
{
    const char *devName = "test";
    char *dmBlkName = "test";
    uint64_t dmBlkNameLen = 0;
    DmVerityTarget *target;
    int ret = FsDmCreateLinearDevice(devName, dmBlkName, dmBlkNameLen, target);
    EXPECT_EQ(ret, 0);
}
}