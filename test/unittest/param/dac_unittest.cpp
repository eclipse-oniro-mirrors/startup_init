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
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>

#include "param_security.h"
#include "param_utils.h"
#include "securec.h"
#include "init_unittest.h"

extern "C" {
extern int RegisterSecurityDacOps(ParamSecurityOps *ops, int isInit);
}

using namespace testing::ext;
using namespace std;

static int SecurityLabelGet(const ParamAuditData *auditData, void *context)
{
    return 0;
}

class DacUnitTest : public ::testing::Test {
public:
    DacUnitTest() {}
    virtual ~DacUnitTest() {}

    void SetUp() {}
    void TearDown() {}
    void TestBody() {}

    int TestDacInitLocalLabel()
    {
        int ret = RegisterSecurityDacOps(&initParamSercurityOps, LABEL_INIT_FOR_INIT);
        EXPECT_EQ(ret, 0);

        if (initParamSercurityOps.securityInitLabel == nullptr || initParamSercurityOps.securityFreeLabel == nullptr) {
            return -1;
        }
        ParamSecurityLabel *label = nullptr;
        ret = initParamSercurityOps.securityInitLabel(&label, LABEL_INIT_FOR_INIT);
        EXPECT_EQ(ret, 0);
        ret = initParamSercurityOps.securityFreeLabel(label);
        EXPECT_EQ(ret, 0);
        return 0;
    }

    int TestDacCheckFilePermission(const char *fileName)
    {
        int ret = RegisterSecurityDacOps(&initParamSercurityOps, LABEL_INIT_FOR_INIT);
        EXPECT_EQ(ret, 0);
        if (initParamSercurityOps.securityCheckFilePermission == nullptr) {
            return -1;
        }
        ParamSecurityLabel *label = nullptr;
        ret = initParamSercurityOps.securityInitLabel(&label, LABEL_INIT_FOR_INIT);
        EXPECT_EQ(ret, 0);
        ret = initParamSercurityOps.securityCheckFilePermission(label, fileName, DAC_WRITE);
        EXPECT_EQ(ret, 0);
        ret = initParamSercurityOps.securityFreeLabel(label);
        EXPECT_EQ(ret, 0);
        return 0;
    }

    int TestDacCheckParaPermission(const char *name, ParamDacData *dacData, int mode)
    {
        int ret = RegisterSecurityDacOps(&initParamSercurityOps, LABEL_INIT_FOR_INIT);
        EXPECT_EQ(ret, 0);
        if (initParamSercurityOps.securityCheckFilePermission == nullptr) {
            return -1;
        }
        ParamSecurityLabel *srclabel = nullptr;
        ret = initParamSercurityOps.securityInitLabel(&srclabel, LABEL_INIT_FOR_INIT);
        EXPECT_EQ(ret, 0);

        ParamAuditData auditData = {};
        auditData.name = name;
        auditData.label = nullptr;
        memcpy_s(&auditData.dacData, sizeof(ParamDacData), dacData, sizeof(ParamDacData));
        ret = initParamSercurityOps.securityCheckParamPermission(srclabel, &auditData, mode);
        initParamSercurityOps.securityFreeLabel(srclabel);
        return ret;
    }

    int TestClientDacCheckFilePermission(const char *fileName)
    {
        int ret = RegisterSecurityDacOps(&clientParamSercurityOps, 0);
        EXPECT_EQ(ret, 0);
        if (clientParamSercurityOps.securityDecodeLabel != nullptr) {
            EXPECT_EQ(1, 0);
        }
        if (clientParamSercurityOps.securityGetLabel != nullptr) {
            EXPECT_EQ(1, 0);
        }
        if (clientParamSercurityOps.securityCheckFilePermission == nullptr) {
            EXPECT_EQ(1, 0);
            return -1;
        }
        ParamSecurityLabel *label = nullptr;
        ret = clientParamSercurityOps.securityInitLabel(&label, 0);
        EXPECT_EQ(ret, 0);
        ret = clientParamSercurityOps.securityCheckFilePermission(label, fileName, DAC_READ);
        EXPECT_EQ(ret, 0);
        ret = clientParamSercurityOps.securityFreeLabel(label);
        EXPECT_EQ(ret, 0);
        return 0;
    }

    int TestEncode(const ParamSecurityLabel *label, std::vector<char> &buffer)
    {
        int ret = RegisterSecurityDacOps(&clientParamSercurityOps, 0);
        EXPECT_EQ(ret, 0);
        if (clientParamSercurityOps.securityDecodeLabel != nullptr) {
            EXPECT_EQ(1, 0);
        }
        if (clientParamSercurityOps.securityGetLabel != nullptr) {
            EXPECT_EQ(1, 0);
        }
        if (clientParamSercurityOps.securityEncodeLabel == nullptr) {
            EXPECT_EQ(1, 0);
            return -1;
        }
        uint32_t bufferSize = 0;
        ret = clientParamSercurityOps.securityEncodeLabel(label, nullptr, &bufferSize);
        EXPECT_EQ(ret, 0);
        buffer.resize(bufferSize + 1);
        ret = clientParamSercurityOps.securityEncodeLabel(label, buffer.data(), &bufferSize);
        EXPECT_EQ(ret, 0);
        return 0;
    }

    int TestDecode(const ParamSecurityLabel *label, std::vector<char> &buffer)
    {
        int ret = RegisterSecurityDacOps(&clientParamSercurityOps, 1);
        EXPECT_EQ(ret, 0);
        if (clientParamSercurityOps.securityDecodeLabel == nullptr) {
            EXPECT_EQ(1, 0);
        }
        if (clientParamSercurityOps.securityEncodeLabel != nullptr) {
            EXPECT_EQ(1, 0);
            return -1;
        }
        ParamSecurityLabel *tmp = nullptr;
        ret = clientParamSercurityOps.securityDecodeLabel(&tmp, buffer.data(), buffer.size());
        if (label == nullptr || tmp == nullptr) {
            return -1;
        }
        EXPECT_EQ(ret, 0);
        EXPECT_EQ(label->cred.gid, tmp->cred.gid);
        EXPECT_EQ(label->cred.uid, tmp->cred.uid);
        return 0;
    }

private:
    ParamSecurityOps initParamSercurityOps {};
    ParamSecurityOps clientParamSercurityOps {};
};

HWTEST_F(DacUnitTest, TestDacInitLocalLabel, TestSize.Level0)
{
    DacUnitTest test;
    test.TestDacInitLocalLabel();
}

HWTEST_F(DacUnitTest, TestDacLabelEncode, TestSize.Level0)
{
    DacUnitTest test;
    std::vector<char> buffer;
    ParamSecurityLabel label = {0, { 4444, 5555}};
    test.TestEncode(&label, buffer);
    test.TestDecode(&label, buffer);
}

HWTEST_F(DacUnitTest, TestDacCheckFilePermission, TestSize.Level0)
{
    DacUnitTest test;
    test.TestDacCheckFilePermission(PARAM_DEFAULT_PATH"/trigger_test.cfg");
}

HWTEST_F(DacUnitTest, TestDacCheckUserParaPermission, TestSize.Level0)
{
    // 相同用户
    DacUnitTest test;
    ParamDacData dacData;
    dacData.gid = getegid();
    dacData.uid = geteuid();
    // read
    dacData.mode = 0400;
    int ret = test.TestDacCheckParaPermission("test.permission.read.aaa", &dacData, DAC_READ);
    EXPECT_EQ(ret, 0);
    dacData.mode = 0400;
    ret = test.TestDacCheckParaPermission("test.permission.read.aaa", &dacData, DAC_WRITE);
    EXPECT_EQ(ret, 0);
    dacData.mode = 0400;
    ret = test.TestDacCheckParaPermission("test.permission.read.aaa", &dacData, DAC_WATCH);
    EXPECT_EQ(ret, 0);

    // write
    dacData.mode = 0200;
    ret = test.TestDacCheckParaPermission("test.permission.write.aaa", &dacData, DAC_READ);
    EXPECT_EQ(ret, 0);
    dacData.mode = 0200;
    ret = test.TestDacCheckParaPermission("test.permission.write.aaa", &dacData, DAC_WRITE);
    EXPECT_EQ(ret, 0);
    dacData.mode = 0200;
    ret = test.TestDacCheckParaPermission("test.permission.write.aaa", &dacData, DAC_WATCH);
    EXPECT_EQ(ret, 0);

    // watch
    dacData.mode = 0100;
    ret = test.TestDacCheckParaPermission("test.permission.watch.aaa", &dacData, DAC_READ);
    EXPECT_EQ(ret, 0);
    dacData.mode = 0100;
    ret = test.TestDacCheckParaPermission("test.permission.watch.aaa", &dacData, DAC_WRITE);
    EXPECT_EQ(ret, 0);
    dacData.mode = 0100;
    ret = test.TestDacCheckParaPermission("test.permission.watch.aaa", &dacData, DAC_WATCH);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(DacUnitTest, TestDacCheckGroupParaPermission, TestSize.Level0)
{
    // 相同组
    DacUnitTest test;
    ParamDacData dacData;
    dacData.gid = getegid();
    dacData.uid = 13333;
    // read
    dacData.mode = 0040;
    int ret = test.TestDacCheckParaPermission("test.permission.read.aaa", &dacData, DAC_READ);
    EXPECT_EQ(ret, 0);
    dacData.mode = 0040;
    ret = test.TestDacCheckParaPermission("test.permission.read.aaa", &dacData, DAC_WRITE);
    EXPECT_EQ(ret, 0);
    dacData.mode = 0040;
    ret = test.TestDacCheckParaPermission("test.permission.read.aaa", &dacData, DAC_WATCH);
    EXPECT_EQ(ret, 0);

    // write
    dacData.mode = 0020;
    ret = test.TestDacCheckParaPermission("test.permission.write.aaa", &dacData, DAC_READ);
    EXPECT_EQ(ret, 0);
    dacData.mode = 0020;
    ret = test.TestDacCheckParaPermission("test.permission.write.aaa", &dacData, DAC_WRITE);
    EXPECT_EQ(ret, 0);
    dacData.mode = 0020;
    ret = test.TestDacCheckParaPermission("test.permission.write.aaa", &dacData, DAC_WATCH);
    EXPECT_EQ(ret, 0);

    // watch
    dacData.mode = 0010;
    ret = test.TestDacCheckParaPermission("test.permission.watch.aaa", &dacData, DAC_READ);
    EXPECT_EQ(ret, 0);
    dacData.mode = 0010;
    ret = test.TestDacCheckParaPermission("test.permission.watch.aaa", &dacData, DAC_WRITE);
    EXPECT_EQ(ret, 0);
    dacData.mode = 0010;
    ret = test.TestDacCheckParaPermission("test.permission.watch.aaa", &dacData, DAC_WATCH);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(DacUnitTest, TestDacCheckOtherParaPermission, TestSize.Level0)
{
    // 其他用户
    DacUnitTest test;
    ParamDacData dacData;
    dacData.gid = 13333;
    dacData.uid = 13333;
    // read
    dacData.mode = 0004;
    int ret = test.TestDacCheckParaPermission("test.permission.read.aaa", &dacData, DAC_READ);
    EXPECT_EQ(ret, 0);
    dacData.mode = 0004;
    ret = test.TestDacCheckParaPermission("test.permission.read.aaa", &dacData, DAC_WRITE);
    EXPECT_EQ(ret, 0);
    dacData.mode = 0004;
    ret = test.TestDacCheckParaPermission("test.permission.read.aaa", &dacData, DAC_WATCH);
    EXPECT_EQ(ret, 0);

    // write
    dacData.mode = 0002;
    ret = test.TestDacCheckParaPermission("test.permission.write.aaa", &dacData, DAC_READ);
    EXPECT_EQ(ret, 0);
    dacData.mode = 0002;
    ret = test.TestDacCheckParaPermission("test.permission.write.aaa", &dacData, DAC_WRITE);
    EXPECT_EQ(ret, 0);
    dacData.mode = 0002;
    ret = test.TestDacCheckParaPermission("test.permission.write.aaa", &dacData, DAC_WATCH);
    EXPECT_EQ(ret, 0);

    // watch
    dacData.mode = 0001;
    ret = test.TestDacCheckParaPermission("test.permission.watch.aaa", &dacData, DAC_READ);
    EXPECT_EQ(ret, 0);
    dacData.mode = 0001;
    ret = test.TestDacCheckParaPermission("test.permission.watch.aaa", &dacData, DAC_WRITE);
    EXPECT_EQ(ret, 0);
    dacData.mode = 0001;
    ret = test.TestDacCheckParaPermission("test.permission.watch.aaa", &dacData, DAC_WATCH);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(DacUnitTest, TestClientDacCheckFilePermission, TestSize.Level0)
{
    DacUnitTest test;
    test.TestClientDacCheckFilePermission(PARAM_DEFAULT_PATH"/trigger_test.cfg");
}
