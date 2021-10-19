/*
 * Copyright (c) 2020 Huawei Device Co., Ltd.
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

#include "init_cmds.h"
#include "init_param.h"
#include "init_unittest.h"

using namespace testing::ext;
using namespace std;

namespace init_ut {
class CmdsUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void)
    {
        InitParamService();
    };
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(CmdsUnitTest, TestGetValueForCmd, TestSize.Level1)
{
    // 1, test one param
    SystemWriteParam("ro.test.1111.2222", "2.3.4.5.6.7.8");
    char *tmpParamValue = (char *)malloc(PARAM_VALUE_LEN_MAX + 1);
    if (tmpParamValue == nullptr) {
        EXPECT_EQ(1, 0);
        return;
    }
    tmpParamValue[0] = '\0';
    const char *cmd1 = "init.${ro.test.1111.2222}.usb.cfg";
    GetParamValue(cmd1, strlen(cmd1), tmpParamValue, PARAM_VALUE_LEN_MAX);
    printf("Tmp value %s \n", tmpParamValue);
    EXPECT_EQ(strcmp("init.2.3.4.5.6.7.8.usb.cfg", tmpParamValue), 0);

    // 2, test two param
    const char *cmd2 = "init.${ro.test.1111.2222}.usb.${ro.test.1111.2222}";
    GetParamValue(cmd2, strlen(cmd2), tmpParamValue, PARAM_VALUE_LEN_MAX);
    printf("Tmp value %s \n", tmpParamValue);
    EXPECT_EQ(strcmp("init.2.3.4.5.6.7.8.usb.2.3.4.5.6.7.8", tmpParamValue), 0);

    // 3, test two param
    const char *cmd3 = "${ro.test.1111.2222}.init.${ro.test.1111.2222}.usb.${ro.test.1111.2222}";
    GetParamValue(cmd3, strlen(cmd3), tmpParamValue, PARAM_VALUE_LEN_MAX);
    printf("Tmp value %s \n", tmpParamValue);
    EXPECT_EQ(strcmp("2.3.4.5.6.7.8.init.2.3.4.5.6.7.8.usb.2.3.4.5.6.7.8", tmpParamValue), 0);
    free(tmpParamValue);
}
HWTEST_F(CmdsUnitTest, TestDoCopy, TestSize.Level1)
{
    DoCmdByName("copy ", "libdemo.z.so aaaa.so");
}
} // namespace init_ut
