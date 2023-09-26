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

#include <benchmark/benchmark.h>
#include "benchmark_fwk.h"
#include "init_param.h"
#include "param_init.h"
#include "parameter.h"
#include "sys_param.h"

using namespace std;
using namespace init_benchmark_test;
namespace {
static int g_maxCount = 512;
}

static inline int TestRandom(void)
{
    return random();
}

namespace init_benchmark_param {
struct LocalParameterTestState {
    explicit LocalParameterTestState(int nprops) noexcept : nprops(nprops), valid(false)
    {
        static const char paramNameChars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_.";
        if (g_maxCount < nprops) {
            fprintf(stderr, "Invalid nprops %d\n", nprops);
            return;
        }
        names = new char *[nprops];
        nameLens = new int[nprops];
        values = new char *[nprops];
        valueLens = new int[nprops];

        srandom(nprops);
        int count = 0;
        for (int i = 0; i < nprops; i++) {
            // Make sure the name has at least 10 characters to make
            // it very unlikely to generate the same TestRandom name.
            nameLens[i] = (TestRandom() % (PARAM_NAME_LEN_MAX - 10)) + 10; // 10 name len
            names[i] = new char[PARAM_NAME_LEN_MAX + 1];
            size_t paramNameLen = sizeof(paramNameChars) - 1;
            for (int j = 0; j < nameLens[i]; j++) {
                if (j == 0 || names[i][j - 1] == '.' || j == nameLens[i] - 1) {
                    // Certain values are not allowed:
                    // - Don't start name with '.'
                    // - Don't allow '.' to appear twice in a row
                    // - Don't allow the name to end with '.'
                    // This assumes that '.' is the last character in the
                    // array so that decrementing the length by one removes
                    // the value from the possible values.
                    paramNameLen--;
                }
                names[i][j] = paramNameChars[TestRandom() % paramNameLen];
            }
            names[i][nameLens[i]] = 0;

            // Make sure the value contains at least 1 character.
            valueLens[i] = (TestRandom() % (PARAM_VALUE_LEN_MAX - 1)) + 1;
            values[i] = new char[PARAM_VALUE_LEN_MAX];
            for (int j = 0; j < valueLens[i]; j++) {
                values[i][j] = paramNameChars[TestRandom() % (sizeof(paramNameChars) - 1)];
            }

            if (SystemSetParameter(names[i], values[i]) != 0) {
                count++;
            }
        }
        if (count > 0) {
            fprintf(stderr, "Failed to add a property, count %d\n", count);
        }
        valid = true;
    }

    LocalParameterTestState(const LocalParameterTestState&) = delete;
    LocalParameterTestState & operator=(const LocalParameterTestState&) = delete;

    ~LocalParameterTestState() noexcept
    {
        for (int i = 0; i < nprops; i++) {
            delete names[i];
            delete values[i];
        }
        delete[] names;
        delete[] nameLens;
        delete[] values;
        delete[] valueLens;
    }

public:
    const int nprops;
    char **names;
    int *nameLens;
    char **values;
    int *valueLens;
    bool valid;
};
}

static init_benchmark_param::LocalParameterTestState *g_localParamTester = nullptr;

void CreateLocalParameterTest(int max)
{
    g_maxCount = max > 0 ? max : g_maxCount;
    g_localParamTester = new init_benchmark_param::LocalParameterTestState(g_maxCount);
    if (g_localParamTester == nullptr) {
        exit(0);
    }
}

/**
 * @brief for parameter get
 *
 * @param state
 */
static void BMCachedParameterGet(benchmark::State &state)
{
    if (g_localParamTester == nullptr || !g_localParamTester->valid) {
        fprintf(stderr, "Invalid nprops %d \n", g_maxCount);
        return;
    }

    CachedHandle handle = CachedParameterCreate(g_localParamTester->names[TestRandom() % g_maxCount], "4444444");
    for (auto _ : state) {
        benchmark::DoNotOptimize(CachedParameterGet(handle));
    }
    state.SetItemsProcessed(state.iterations());
}

/**
 * @brief for parameter get, static handle
 *
 * @param state
 */
static void BMCachedParameterGetChangedStatic(benchmark::State &state)
{
    if (g_localParamTester == nullptr || !g_localParamTester->valid) {
        fprintf(stderr, "Invalid nprops %d \n", g_maxCount);
        return;
    }

    for (auto _ : state) {
        static CachedHandle handle = CachedParameterCreate(
            g_localParamTester->names[TestRandom() % g_maxCount], "xxxxxx");
        int changed = 0;
        benchmark::DoNotOptimize(CachedParameterGetChanged(handle, &changed));
    }
    state.SetItemsProcessed(state.iterations());
}

/**
 * @brief for parameter get, global handle
 *
 * @param state
 */
static void BMCachedParameterGetChangedGlobal(benchmark::State &state)
{
    if (g_localParamTester == nullptr || !g_localParamTester->valid) {
        fprintf(stderr, "Invalid nprops %d \n", g_maxCount);
        return;
    }

    CachedHandle handle = CachedParameterCreate(g_localParamTester->names[TestRandom() % g_maxCount], "xxxxxxxxx");
    for (auto _ : state) {
        int changed = 0;
        benchmark::DoNotOptimize(CachedParameterGetChanged(handle, &changed));
    }
    state.SetItemsProcessed(state.iterations());
}

/**
 * @brief for parameter get, global handle
 *
 * @param state
 */
static void BMCachedParameterGetChangedGlobal2(benchmark::State &state)
{
    if (g_localParamTester == nullptr || !g_localParamTester->valid) {
        fprintf(stderr, "Invalid nprops %d \n", g_maxCount);
        return;
    }

    CachedHandle handle = nullptr;
    for (auto _ : state) {
        if (handle == nullptr) {
            handle = CachedParameterCreate(g_localParamTester->names[TestRandom() % g_maxCount], "xxxxxxxxx");
        }
        int changed = 0;
        benchmark::DoNotOptimize(CachedParameterGetChanged(handle, &changed));
    }
    state.SetItemsProcessed(state.iterations());
}

/**
 * @brief for get
 * data exist
 *
 * @param state
 */
static void BMSystemReadParam(benchmark::State &state)
{
    if (g_localParamTester == nullptr || !g_localParamTester->valid) {
        fprintf(stderr, "Invalid nprops %d \n", g_maxCount);
        return;
    }
    {
        char value[PARAM_VALUE_LEN_MAX] = {0};
        uint32_t len = PARAM_VALUE_LEN_MAX;
        SystemReadParam(g_localParamTester->names[TestRandom() % g_maxCount], value, &len);
    }
    for (auto _ : state) {
        char value[PARAM_VALUE_LEN_MAX] = {0};
        uint32_t len = PARAM_VALUE_LEN_MAX;
        SystemReadParam(g_localParamTester->names[TestRandom() % g_maxCount], value, &len);
    }
    state.SetItemsProcessed(state.iterations());
}

/**
 * @brief for get
 * data not exist
 *
 * @param state
 */
static void BMSystemReadParam_none(benchmark::State &state)
{
    if (g_localParamTester == nullptr || !g_localParamTester->valid) {
        fprintf(stderr, "Invalid nprops %d \n", g_maxCount);
        return;
    }
    {
        char value[PARAM_VALUE_LEN_MAX] = {0};
        uint32_t len = PARAM_VALUE_LEN_MAX;
        SystemReadParam("test.aaa.aaa.aaa", value, &len);
    }
    for (auto _ : state) {
        char value[PARAM_VALUE_LEN_MAX] = {0};
        uint32_t len = PARAM_VALUE_LEN_MAX;
        SystemReadParam("test.aaa.aaa.aaa", value, &len);
    }
    state.SetItemsProcessed(state.iterations());
}

/**
 * @brief for find
 *
 * @param state
 */
static void BMSystemFindParameter(benchmark::State &state)
{
    if (g_localParamTester == nullptr || !g_localParamTester->valid) {
        fprintf(stderr, "Invalid nprops %d \n", g_maxCount);
        return;
    }

    for (auto _ : state) {
        ParamHandle handle = 0;
        SystemFindParameter(g_localParamTester->names[TestRandom() % g_maxCount], &handle);
    }
    state.SetItemsProcessed(state.iterations());
}

/**
 * @brief for find, and read value
 *
 * @param state
 */
static void BMSystemGetParameterValue(benchmark::State &state)
{
    if (g_localParamTester == nullptr || !g_localParamTester->valid) {
        fprintf(stderr, "Invalid nprops %d \n", g_maxCount);
        return;
    }

    ParamHandle *handle = new ParamHandle[g_maxCount];
    for (int i = 0; i < g_maxCount; ++i) {
        SystemFindParameter(g_localParamTester->names[TestRandom() % g_maxCount], &handle[i]);
    }

    int i = 0;
    char value[PARAM_VALUE_LEN_MAX];
    for (auto _ : state) {
        uint32_t len = PARAM_VALUE_LEN_MAX;
        SystemGetParameterValue(handle[i], value, &len);
        i = (i + 1) % g_maxCount;
    }
    state.SetItemsProcessed(state.iterations());
    delete[] handle;
}

/**
 * @brief for find, and read commit id
 *
 * @param state
 */
static void BMSystemGetParameterCommitId(benchmark::State &state)
{
    if (g_localParamTester == nullptr || !g_localParamTester->valid) {
        fprintf(stderr, "Invalid nprops %d \n", g_maxCount);
        return;
    }

    ParamHandle *handle = new ParamHandle[g_maxCount];
    for (int i = 0; i < g_maxCount; ++i) {
        SystemFindParameter(g_localParamTester->names[TestRandom() % g_maxCount], &handle[i]);
    }

    int i = 0;
    for (auto _ : state) {
        uint32_t commitId = 0;
        SystemGetParameterCommitId(handle[i], &commitId);
        i = (i + 1) % g_maxCount;
    }
    state.SetItemsProcessed(state.iterations());
    delete[] handle;
}

static void BMTestRandom(benchmark::State &state)
{
    if (g_localParamTester == nullptr || !g_localParamTester->valid) {
        fprintf(stderr, "Invalid nprops %d \n", g_maxCount);
        return;
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(TestRandom());
    }
    state.SetItemsProcessed(state.iterations());
}

INIT_BENCHMARK(BMCachedParameterGet);
INIT_BENCHMARK(BMCachedParameterGetChangedStatic);
INIT_BENCHMARK(BMCachedParameterGetChangedGlobal);
INIT_BENCHMARK(BMCachedParameterGetChangedGlobal2);

INIT_BENCHMARK(BMSystemReadParam);
INIT_BENCHMARK(BMSystemReadParam_none);
INIT_BENCHMARK(BMSystemFindParameter);
INIT_BENCHMARK(BMSystemGetParameterValue);
INIT_BENCHMARK(BMSystemGetParameterCommitId);
INIT_BENCHMARK(BMTestRandom);
