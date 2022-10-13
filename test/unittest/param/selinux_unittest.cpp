/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include <gtest/gtest.h>

#include "param_manager.h"
#include "param_security.h"
#include "param_stub.h"
#include "param_utils.h"
#include "securec.h"

using namespace testing::ext;
using namespace std;

namespace init_ut {
class SelinuxUnitTest : public ::testing::Test {
public:
    SelinuxUnitTest() {}
    virtual ~SelinuxUnitTest() {}

    void SetUp()
    {
        SetTestPermissionResult(0);
    }
    void TearDown() {}
    void TestBody() {}

    int TestSelinuxInitLocalLabel()
    {
        int ret = RegisterSecuritySelinuxOps(&initParamSercurityOps, LABEL_INIT_FOR_INIT);
        EXPECT_EQ(ret, 0);

        if (initParamSercurityOps.securityInitLabel == nullptr || initParamSercurityOps.securityFreeLabel == nullptr) {
            return -1;
        }
        ParamSecurityLabel label = {};
        ret = initParamSercurityOps.securityInitLabel(&label, LABEL_INIT_FOR_INIT);
        EXPECT_EQ(ret, 0);
        ret = initParamSercurityOps.securityFreeLabel(&label);
        EXPECT_EQ(ret, 0);
        return 0;
    }

    int TestSelinuxCheckFilePermission(const char *fileName)
    {
        int ret = RegisterSecuritySelinuxOps(&initParamSercurityOps, LABEL_INIT_FOR_INIT);
        EXPECT_EQ(ret, 0);
        if (initParamSercurityOps.securityCheckFilePermission == nullptr) {
            return -1;
        }
        ParamSecurityLabel label = {};
        ret = initParamSercurityOps.securityInitLabel(&label, LABEL_INIT_FOR_INIT);
        EXPECT_EQ(ret, 0);
        ret = initParamSercurityOps.securityCheckFilePermission(&label, fileName, DAC_WRITE);
        EXPECT_EQ(ret, 0);
        ret = initParamSercurityOps.securityFreeLabel(&label);
        EXPECT_EQ(ret, 0);
        return 0;
    }

    int TestSelinuxCheckParaPermission(const char *name, const char *label)
    {
        int ret = RegisterSecuritySelinuxOps(&initParamSercurityOps, LABEL_INIT_FOR_INIT);
        EXPECT_EQ(ret, 0);
        if (initParamSercurityOps.securityCheckFilePermission == nullptr) {
            return -1;
        }
        ParamSecurityLabel srclabel = {};
        ret = initParamSercurityOps.securityInitLabel(&srclabel, LABEL_INIT_FOR_INIT);
        EXPECT_EQ(ret, 0);

        ret = initParamSercurityOps.securityCheckParamPermission(&srclabel, name, DAC_WRITE);
        EXPECT_EQ(ret, 0);
        ret = initParamSercurityOps.securityFreeLabel(&srclabel);
        EXPECT_EQ(ret, 0);
        return 0;
    }

    int TestClientSelinuxCheckFilePermission(const char *fileName)
    {
        int ret = RegisterSecuritySelinuxOps(&clientParamSercurityOps, 0);
        EXPECT_EQ(ret, 0);
        if (clientParamSercurityOps.securityCheckFilePermission == nullptr) {
            EXPECT_EQ(1, 0);
            return -1;
        }
        ParamSecurityLabel label = {};
        ret = clientParamSercurityOps.securityInitLabel(&label, 0);
        EXPECT_EQ(ret, 0);
        ret = clientParamSercurityOps.securityCheckFilePermission(&label, fileName, DAC_READ);
        EXPECT_EQ(ret, 0);
        ret = clientParamSercurityOps.securityFreeLabel(&label);
        EXPECT_EQ(ret, 0);
        return 0;
    }

    int TestClientSelinuxCheckParaPermissionWrite(const char *name, const char *label)
    {
        int ret = RegisterSecuritySelinuxOps(&clientParamSercurityOps, 0);
        EXPECT_EQ(ret, 0);

        if (clientParamSercurityOps.securityCheckFilePermission == nullptr) {
            return -1;
        }
        ParamSecurityLabel srclabel = {};
        ret = clientParamSercurityOps.securityInitLabel(&srclabel, 0);
        EXPECT_EQ(ret, 0);
        ret = clientParamSercurityOps.securityCheckParamPermission(&srclabel, name, DAC_WRITE);
        EXPECT_EQ(ret, 0);
        ret = clientParamSercurityOps.securityFreeLabel(&srclabel);
        EXPECT_EQ(ret, 0);
        return 0;
    }

    int TestClientSelinuxCheckParaPermissionRead(const char *name, const char *label)
    {
        int ret = RegisterSecuritySelinuxOps(&clientParamSercurityOps, 0);
        EXPECT_EQ(ret, 0);
        if (clientParamSercurityOps.securityCheckFilePermission == nullptr) {
            return -1;
        }
        ParamSecurityLabel srclabel = {};
        ret = clientParamSercurityOps.securityInitLabel(&srclabel, 0);
        EXPECT_EQ(ret, 0);
        ret = clientParamSercurityOps.securityCheckParamPermission(&srclabel, name, DAC_READ);
        EXPECT_EQ(ret, 0);
        ret = clientParamSercurityOps.securityFreeLabel(&srclabel);
        EXPECT_EQ(ret, 0);
        uint8_t updataMode = GetParamWorkSpace()->ops.updaterMode;
        GetParamWorkSpace()->ops.updaterMode = 1; // 1 test updater mode
        RegisterSecuritySelinuxOps(&clientParamSercurityOps, 0);
        if (clientParamSercurityOps.securityCheckParamPermission != nullptr) {
            clientParamSercurityOps.securityCheckParamPermission(nullptr, nullptr, 0);
        }
        GetParamWorkSpace()->ops.updaterMode = updataMode;
        return 0;
    }

private:
    ParamSecurityOps initParamSercurityOps {};
    ParamSecurityOps clientParamSercurityOps {};
};

HWTEST_F(SelinuxUnitTest, TestSelinuxInitLocalLabel, TestSize.Level0)
{
    SelinuxUnitTest test;
    test.TestSelinuxInitLocalLabel();
}

HWTEST_F(SelinuxUnitTest, TestSelinuxCheckFilePermission, TestSize.Level0)
{
    SelinuxUnitTest test;
    test.TestSelinuxCheckFilePermission(STARTUP_INIT_UT_PATH "/trigger_test.cfg");
}

HWTEST_F(SelinuxUnitTest, TestSelinuxCheckParaPermission, TestSize.Level0)
{
    SelinuxUnitTest test;
    test.TestSelinuxCheckParaPermission("aaa.bbb.bbb.ccc", "user:group1:r");
}

HWTEST_F(SelinuxUnitTest, TestClientDacCheckFilePermission, TestSize.Level0)
{
    SelinuxUnitTest test;
    test.TestClientSelinuxCheckFilePermission(STARTUP_INIT_UT_PATH "/trigger_test.cfg");
}

HWTEST_F(SelinuxUnitTest, TestClientDacCheckParaPermission, TestSize.Level0)
{
    SelinuxUnitTest test;
    test.TestClientSelinuxCheckParaPermissionWrite("aaa.bbb.bbb.ccc", "user:group1:r");
    test.TestClientSelinuxCheckParaPermissionRead("aaa.bbb.bbb.ccc", "user:group1:r");
}
}