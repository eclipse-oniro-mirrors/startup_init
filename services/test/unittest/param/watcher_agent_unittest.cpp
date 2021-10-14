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

#include "iwatcher.h"
#include "iwatcher_manager.h"
#include "param_utils.h"
#include "sys_param.h"
#include "watcher_manager_kits.h"

using namespace testing::ext;
using namespace std;

void TestParameterChange(const char *key, const char *value, void *context)
{
    printf("TestParameterChange key:%s %s", key, value);
}

class WatcherAgentTest : public ::testing::Test {
public:
    WatcherAgentTest() {}
    virtual ~WatcherAgentTest() {}

    void SetUp() {}
    void TearDown() {}
    void TestBody() {}

    int TestAddWatcher()
    {
        int ret = SystemWatchParameter("test.permission.watcher.test1", TestParameterChange, nullptr);
        EXPECT_EQ(ret, 0);
        ret = SystemWatchParameter("test.permission.watcher.test1*", TestParameterChange, nullptr);
        EXPECT_EQ(ret, 0);
        // 非法
        ret = SystemWatchParameter("test.permission.watcher.tes^^^^t1*", TestParameterChange, nullptr);
        EXPECT_NE(ret, 0);
        // 被禁止
        ret = SystemWatchParameter("test.permission.read.test1*", TestParameterChange, nullptr);
        EXPECT_NE(ret, 0);
        return 0;
    }

    int TestDelWatcher()
    {
        int ret = SystemWatchParameter("test.permission.watcher.test1", nullptr, nullptr);
        EXPECT_EQ(ret, 0);
        ret = SystemWatchParameter("test.permission.watcher.test1*", nullptr, nullptr);
        EXPECT_EQ(ret, 0);
        // 非法
        ret = SystemWatchParameter("test.permission.watcher.tes^^^^t1*", nullptr, nullptr);
        EXPECT_NE(ret, 0);
        // 被禁止
        ret = SystemWatchParameter("test.permission.read.test1*", nullptr, nullptr);
        EXPECT_NE(ret, 0);
        return 0;
    }

    int TestRecvMessage()
    {
        return 0;
    }
};

HWTEST_F(WatcherAgentTest, TestAddWatcher, TestSize.Level1)
{
    WatcherAgentTest test;
    test.TestAddWatcher();
}

HWTEST_F(WatcherAgentTest, TestDelWatcher, TestSize.Level1)
{
    WatcherAgentTest test;
    test.TestDelWatcher();
}