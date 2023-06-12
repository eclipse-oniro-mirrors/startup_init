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
#include <err.h>
#include <getopt.h>
#include <cinttypes>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "benchmark_fwk.h"
using namespace std;
using namespace init_benchmark_test;

namespace {
constexpr auto K = 1024;
using args_vector = std::vector<std::vector<int64_t>>;

static const std::vector<int> commonArgs {
    8,
    16,
    32,
    64,
    512,
    1 * K,
    8 * K,
    16 * K,
    32 * K,
    64 * K,
    128 * K,
};

static const std::vector<int> limitSizes {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
};
}

namespace init_benchmark_test {
std::map<std::string, std::pair<benchmark_func, std::string>> g_allBenchmarks;
std::mutex g_benchmarkLock;
static struct option g_benchmarkLongOptions[] = {
    {"init_cpu", required_argument, nullptr, 'c'},
    {"init_iterations", required_argument, nullptr, 'i'},
    {"help", no_argument, nullptr, 'h'},
    {nullptr, 0, nullptr, 0},
    };
}

static void PrintUsageAndExit()
{
    printf("Usage:\n");
    printf("init_benchmarks [--init_cpu=<cpu_to_isolate>]\n");
    printf("                [--init_iterations=<num_iter>]\n");
    printf("                [<original benchmark flags>]\n");
    printf("benchmark flags:\n");

    int argc = 2;
    char argv0[] = "init_benchmark";
    char argv1[] = "--help";
    char *argv[3] = { argv0, argv1, nullptr };
    benchmark::Initialize(&argc, argv);
    exit(1);
}

static void ShiftOptions(int argc, char **argv, std::vector<char *> *argvAfterShift)
{
    (*argvAfterShift)[0] = argv[0];
    for (int i = 1; i < argc; ++i) {
        char *optarg = argv[i];
        size_t index = 0;
        // Find if musl defined this arg.
        while (g_benchmarkLongOptions[index].name && strncmp(g_benchmarkLongOptions[index].name, optarg + 2, // 2 arg
            strlen(g_benchmarkLongOptions[index].name))) {
            ++index;
        }
        // Not defined.
        if (!g_benchmarkLongOptions[index].name) {
            argvAfterShift->push_back(optarg);
        } else if ((g_benchmarkLongOptions[index].has_arg == required_argument) && !strchr(optarg, '=')) {
            i++;
        }
    }
    argvAfterShift->push_back(nullptr);
}

static bench_opts_t ParseOptions(int argc, char **argv)
{
    bench_opts_t opts;
    int opt;
    char *errorCheck = nullptr;
    opterr = 0; // Don't show unrecognized option error.

    while ((opt = getopt_long(argc, argv, "c:i:a:h", g_benchmarkLongOptions, nullptr)) != -1) {
        switch (opt) {
            case 'c':
                if (!(*optarg)) {
                    printf("ERROR: no argument specified for init_cpu.\n");
                    PrintUsageAndExit();
                    break;
                }
                opts.cpuNum = strtol(optarg, &errorCheck, 10); // 10 base
                if (*errorCheck) {
                    errx(1, "ERROR: Args %s is not a valid integer.", optarg);
                }
                break;
            case 'i':
                if (!(*optarg)) {
                    printf("ERROR: no argument specified for init_iterations.\n");
                    PrintUsageAndExit();
                    break;
                }
                opts.iterNum = strtol(optarg, &errorCheck, 10); // 10 base
                if (*errorCheck != '\0' || opts.iterNum < 0) {
                    errx(1, "ERROR: Args %s is not a valid number of iterations.", optarg);
                }
                break;
            case 'h':
                PrintUsageAndExit();
                break;
            case '?':
                break;
            default:
                exit(1);
        }
    }
    return opts;
}

static void LockAndRun(benchmark::State &state, benchmark_func func, int cpuNum)
{
    if (cpuNum >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpuNum, &cpuset);

        if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
            printf("lock CPU failed, ERROR:%s\n", strerror(errno));
        }
    }

    reinterpret_cast<void (*)(benchmark::State &)>(func)(state);
}

static args_vector *ResolveArgs(args_vector *argsVector, std::string args,
    std::map<std::string, args_vector> &presetArgs)
{
    // Get it from preset args.
    if (presetArgs.count(args)) {
        return &presetArgs[args];
    }

    // Convert string to int.
    argsVector->push_back(std::vector<int64_t>());
    std::stringstream sstream(args);
    std::string arg;
    while (sstream >> arg) {
        char *errorCheck;
        int converted = static_cast<int>(strtol(arg.c_str(), &errorCheck, 10)); // 10 base
        if (*errorCheck) {
            errx(1, "ERROR: Args str %s contains an invalid macro or int.", args.c_str());
        }
        (*argsVector)[0].push_back(converted);
    }
    return argsVector;
}

static args_vector GetArgs(const std::vector<int> &sizes)
{
    args_vector args;
    for (int size : sizes) {
        args.push_back( {size} );
    }
    return args;
}

static args_vector GetArgs(const std::vector<int> &sizes, int value)
{
    args_vector args;
    for (int size : sizes) {
        args.push_back( {size, value} );
    }
    return args;
}

static args_vector GetArgs(const std::vector<int> &sizes, int value1, int value2)
{
    args_vector args;
    for (int size : sizes) {
        args.push_back( {size, value1, value2} );
    }
    return args;
}

static args_vector GetArgs(const std::vector<int> &sizes, const std::vector<int> &limits, int value)
{
    args_vector args;
    for (int size : sizes) {
        for (int limit : limits) {
            args.push_back( {size, limit, value} );
        }
    }
    return args;
}

static std::map<std::string, args_vector> GetPresetArgs()
{
    std::map<std::string, args_vector> presetArgs {
        {"COMMON_ARGS", GetArgs(commonArgs)},
        {"ALIGNED_ONEBUF", GetArgs(commonArgs, 0)},
        {"ALIGNED_TWOBUF", GetArgs(commonArgs, 0, 0)},
        {"STRING_LIMIT", GetArgs(commonArgs, limitSizes, 0)},
        {"MATH_COMMON", args_vector{{0}, {1}, {2}, {3}, {4}, {5}}},
        {"BENCHMARK_VARIABLE", args_vector{{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}}},
        {"REALPATH_VARIABLE", args_vector{{0}, {1}, {2}, {3}, {4}}},
        {"MMAP_SIZE", args_vector{{8}, {16}, {32}, {64}, {128}, {512}}},
    };

    return presetArgs;
}

static void RegisterSingleBenchmark(bench_opts_t opts, const std::string &funcName, args_vector *runArgs)
{
    if (g_allBenchmarks.find(funcName) == g_allBenchmarks.end()) {
        errx(1, "ERROR: No benchmark for function %s", funcName.c_str());
    }

    benchmark_func func = g_allBenchmarks.at(funcName).first;
    for (const std::vector<int64_t> &args : (*runArgs)) {
        // It will call LockAndRun(func, opts.cpuNum).
        auto registration = benchmark::RegisterBenchmark(funcName.c_str(), LockAndRun, func, opts.cpuNum)->Args(args);
        printf("opts.iterNum %ld \n", opts.iterNum);
        if (opts.iterNum > 0) {
            registration->Iterations(opts.iterNum);
        }
    }
}

static void RegisterAllBenchmarks(const bench_opts_t &opts, std::map<std::string, args_vector> &presetArgs)
{
    for (auto &entry : g_allBenchmarks) {
        auto &funcInfo = entry.second;
        args_vector arg_vector;
        args_vector *runArgs = ResolveArgs(&arg_vector, funcInfo.second, presetArgs);
        RegisterSingleBenchmark(opts, entry.first, runArgs);
    }
}

int main(int argc, char **argv)
{
    std::map<std::string, args_vector> presetArgs = GetPresetArgs();
    bench_opts_t opts = ParseOptions(argc, argv);
    std::vector<char *> argvAfterShift(argc);
    ShiftOptions(argc, argv, &argvAfterShift);
    RegisterAllBenchmarks(opts, presetArgs);
    if (setpriority(PRIO_PROCESS, 0, -20)) { // 20 max
        perror("Set priority of process failed.\n");
    }
    CreateLocalParameterTest(512); // test max 512
    int argcAfterShift = argvAfterShift.size();
    benchmark::Initialize(&argcAfterShift, argvAfterShift.data());
    benchmark::RunSpecifiedBenchmarks();
}
