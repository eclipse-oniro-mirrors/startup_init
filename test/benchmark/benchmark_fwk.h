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
#ifndef STARTUP_INIT_BENCHMARK_FWK_H
#define STARTUP_INIT_BENCHMARK_FWK_H
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace init_benchmark_test {
using benchmark_func = void (*) (void);
extern std::mutex g_benchmarkLock;
extern std::map<std::string, std::pair<benchmark_func, std::string>> g_allBenchmarks;

static int  __attribute__((unused)) AddBenchmark(const std::string &funcName,
                                                 benchmark_func funcPtr, const std::string &arg = "")
{
    g_benchmarkLock.lock();
    g_allBenchmarks.emplace(std::string(funcName), std::make_pair(funcPtr, arg));
    g_benchmarkLock.unlock();
    return 0;
}

#define INIT_BENCHMARK(n) \
    int _init_benchmark_##n __attribute__((unused)) = AddBenchmark(std::string(#n), \
                                                                   reinterpret_cast<benchmark_func>(n))

#define INIT_BENCHMARK_WITH_ARG(n, arg) \
    int _init_benchmark_##n __attribute__((unused)) = AddBenchmark(std::string(#n), \
                                                                   reinterpret_cast<benchmark_func>(n), arg)
}

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef struct bench_opts_t {
    int cpuNum = -1;
    long iterNum = 0;
} BENCH_OPTS_T;

void CreateLocalParameterTest(int max);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif // STARTUP_INIT_BENCHMARK_FWK_H