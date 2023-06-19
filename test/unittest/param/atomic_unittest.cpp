/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include <climits>
#include <thread>
#include <cstdint>
#include <gtest/gtest.h>

#include "param_atomic.h"
#include "param_common.h"
#include "param_utils.h"

using namespace testing::ext;
const int THREAD_NUM = 5;
const int MAX_NUM = 10;

namespace init_ut {
class AtomicUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

/**
 * 测试方法：
 *  1，create 线程，执行全局变量写操作
 *      store with dirty
 *      sleep
 *      store clear dirty
 *      store commit ++
 *  2，read 现成，执行全局变量读参数
 *
 */
using AtomicTestData = struct {
    ATOMIC_UINT32 commitId;
    uint32_t data;
};

static AtomicTestData g_testData = { 0, 0 };

static void *TestSetData(void *args)
{
    while (g_testData.data < MAX_NUM) {
        uint32_t commitId = ATOMIC_LOAD_EXPLICIT(&g_testData.commitId, MEMORY_ORDER_RELAXED);
        ATOMIC_STORE_EXPLICIT(&g_testData.commitId, commitId | PARAM_FLAGS_MODIFY, MEMORY_ORDER_RELAXED);
        g_testData.data++;
        usleep(200 * 1000); // 200 * 1000 wait
        printf("TestSetData data: %d commit: %d \n", g_testData.data, g_testData.commitId & PARAM_FLAGS_COMMITID);
        uint32_t flags = commitId & ~PARAM_FLAGS_COMMITID;
        ATOMIC_STORE_EXPLICIT(&g_testData.commitId, (++commitId) | flags, MEMORY_ORDER_RELEASE);
        futex_wake(&g_testData.commitId, INT_MAX);
        usleep(100); // 100 wait
    }
    return nullptr;
}

static inline uint32_t TestReadCommitId(AtomicTestData *entry)
{
    uint32_t commitId = ATOMIC_LOAD_EXPLICIT(&entry->commitId, MEMORY_ORDER_ACQUIRE);
    while (commitId & PARAM_FLAGS_MODIFY) {
        futex_wait(&entry->commitId, commitId);
        commitId = ATOMIC_LOAD_EXPLICIT(&entry->commitId, MEMORY_ORDER_ACQUIRE);
    }
    return commitId & PARAM_FLAGS_COMMITID;
}

static inline int TestReadParamValue(AtomicTestData *entry, uint32_t *commitId)
{
    uint32_t data;
    uint32_t id = *commitId;
    do {
        *commitId = id;
        data = entry->data;
        id = TestReadCommitId(entry);
    } while (*commitId != id); // if change, must read
    return data;
}

static void *TestReadData(void *args)
{
    uint32_t data = 0;
    while (data < MAX_NUM) {
        uint32_t commitId = TestReadCommitId(&g_testData);
        data = TestReadParamValue(&g_testData, &commitId);
        printf("[ %d] TestReadData data: %d commit: %d \n", gettid(), data, commitId);
        usleep(10); // 10 wait
    }
    return nullptr;
}

HWTEST_F(AtomicUnitTest, AtomicUnitTest_001, TestSize.Level0)
{
    printf("AtomicUnitTest_001 \n");
    pthread_t writeThread = 0;
    pthread_t readThread[THREAD_NUM] = { 0 };
    pthread_create(&writeThread, nullptr, TestSetData, nullptr);
    for (size_t i = 0; i < THREAD_NUM; i++) {
        pthread_create(&readThread[i], nullptr, TestReadData, nullptr);
    }
    pthread_join(writeThread, nullptr);
    printf("AtomicUnitTest_001 end \n");
}
}