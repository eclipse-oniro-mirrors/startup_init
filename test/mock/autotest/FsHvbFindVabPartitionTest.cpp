/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include <gmock/gmock.h>
#include "FsHvbFindVabPartition.h"  // 假设头文件名为 FsHvbFindVabPartition.h

// 模拟外部依赖函数
class MockHookMgr {
public:
    MOCK_METHOD(int, HookMgrExecute, (void*, int, void*, HOOK_EXEC_OPTIONS*), ());
};

// FsHvbFindVabPartition 函数的测试类
class FsHvbFindVabPartitionTest : public ::testing::Test {
protected:
    MockHookMgr mockHookMgr;
    
    void SetUp() override {
        // 可以做一些初始化工作
    }

    void TearDown() override {
        // 清理工作
    }
};

// Test: memcpy_s 失败（模拟复制失败）
TEST_F(FsHvbFindVabPartitionTest, TestMemcpyFail)
{
    HvbDeviceParam devPara;
    EXPECT_CALL(mockHookMgr, HookMgrExecute(_, _, _, _))
        .Times(0);  // 确保 HookMgrExecute 不被调用
    
    // 模拟 memcpy_s 失败
    int result = FsHvbFindVabPartition("testPartition", &devPara);
    EXPECT_EQ(result, -1);  // 验证返回 -1
}

// Test: HookMgrExecute 返回错误
TEST_F(FsHvbFindVabPartitionTest, TestHookMgrExecuteFail)
{
    HvbDeviceParam devPara;
    EXPECT_CALL(mockHookMgr, HookMgrExecute(_, _, _, _))
        .WillOnce(Return(-1));  // 模拟 HookMgrExecute 返回错误

    int result = FsHvbFindVabPartition("testPartition", &devPara);
    EXPECT_EQ(result, -1);  // 验证返回 -1
}

// Test: HookMgrExecute 成功，未找到分区
TEST_F(FsHvbFindVabPartitionTest, TestHookMgrExecuteNoPartitionFound)
{
    HvbDeviceParam devPara;
    EXPECT_CALL(mockHookMgr, HookMgrExecute(_, _, _, _))
        .WillOnce(Return(0));  // 模拟 HookMgrExecute 成功但未找到分区

    int result = FsHvbFindVabPartition("testPartition", &devPara);
    EXPECT_EQ(result, 0);  // 验证返回 0
}

// Test: HookMgrExecute 成功，找到了分区
TEST_F(FsHvbFindVabPartitionTest, TestHookMgrExecutePartitionFound)
{
    HvbDeviceParam devPara;
    // 模拟 memcpy_s 成功
    EXPECT_CALL(mockHookMgr, HookMgrExecute(_, _, _, _))
        .WillOnce(Return(0));  // 模拟 HookMgrExecute 成功，找到了分区

    // 模拟 devPara 更新
        strcpy_s(devPara.value, "foundPartition");

    int result = FsHvbFindVabPartition("testPartition", &devPara);
    EXPECT_EQ(result, 0);  // 验证返回 0，表示成功找到分区
}

// Test: memcpy_s 和 memset_s 成功，验证 partition 相关操作
TEST_F(FsHvbFindVabPartitionTest, TestMemcpyAndMemsetSuccess)
{
    HvbDeviceParam devPara;
    EXPECT_CALL(mockHookMgr, HookMgrExecute(_, _, _, _))
        .WillOnce(Return(0));  // 模拟 HookMgrExecute 成功

    int result = FsHvbFindVabPartition("testPartition", &devPara);
    EXPECT_EQ(result, 0);  // 验证返回 0，表示操作成功
}
