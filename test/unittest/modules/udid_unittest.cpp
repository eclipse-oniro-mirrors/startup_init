/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include <iostream>

#include "udid.h"
#include "param_stub.h"
#include "sysparam_errno.h"
#include "securec.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
extern "C" {
int GetSha256Value(const char *input, char *udid, uint32_t udidSize);
void SetDevUdid();
int CalcDevUdid(char *udid, uint32_t size);
}

class UdidUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(UdidUnitTest, TestUDID_001, TestSize.Level1)
{
    SetDevUdid();
}

HWTEST_F(UdidUnitTest, TestUDID_002, TestSize.Level1)
{
    int ret = GetSha256Value(nullptr, nullptr, 0);
    EXPECT_EQ(ret, EC_FAILURE);
}

HWTEST_F(UdidUnitTest, TestUDID_003, TestSize.Level1)
{
    char udid[66] = {0};
    int ret = CalcDevUdid(udid, 6);
    EXPECT_EQ(ret, EC_FAILURE);
    ret = CalcDevUdid(udid, 66);
    EXPECT_EQ(ret, EC_SUCCESS);
}
} // namespace init_ut
