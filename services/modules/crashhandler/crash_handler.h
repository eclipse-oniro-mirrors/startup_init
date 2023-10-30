/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#ifndef INIT_SIGNAL_HANDLER_H
#define INIT_SIGNAL_HANDLER_H

#include <pthread.h>
#include <inttypes.h>
#include <signal.h>
#include <ucontext.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOCAL_HANDLER_STACK_SIZE (64 * 1024) // 64K
#define FAULTLOG_FILE_PROP 0640
#define MAX_FRAME 64
#define BUF_SZ 512
#define NAME_LEN 128
#define PATH_LEN 1024
#define MAX_FATAL_MSG_SIZE 1024
#define SYMBOL_BUF_SIZE 1024
#define MIN_VALID_FRAME_COUNT 3
#define BACK_STACK_MAX_STEPS 64

#if defined(__arm__)
typedef enum ARM_REG {
    ARM_R0 = 0,
    ARM_R1,
    ARM_R2,
    ARM_R3,
    ARM_R4,
    ARM_R5,
    ARM_R6,
    ARM_R7,
    ARM_R8,
    ARM_R9,
    ARM_R10,
    ARM_FP,
    ARM_IP,
    ARM_SP,
    ARM_LR,
    ARM_PC
} ARM_REG;
#elif defined(__aarch64__)
enum AARCH64_REG {
    AARCH64_X0 = 0,
    AARCH64_X1,
    AARCH64_X2,
    AARCH64_X3,
    AARCH64_X4,
    AARCH64_X5,
    AARCH64_X6,
    AARCH64_X7,
    AARCH64_X8,
    AARCH64_X9,
    AARCH64_X10,
    AARCH64_X11,
    AARCH64_X12,
    AARCH64_X13,
    AARCH64_X14,
    AARCH64_X15,
    AARCH64_X16,
    AARCH64_X17,
    AARCH64_X18,
    AARCH64_X19,
    AARCH64_X20,
    AARCH64_X21,
    AARCH64_X22,
    AARCH64_X23,
    AARCH64_X24,
    AARCH64_X25,
    AARCH64_X26,
    AARCH64_X27,
    AARCH64_X28,
    AARCH64_X29,
    AARCH64_X30,
    AARCH64_SP = 31,
    AARCH64_PC
};
#endif

typedef struct ProcessDumpRequest_ {
    int32_t type;
    int32_t tid;
    int32_t pid;
    int32_t vmPid;
    uint32_t uid;
    uint64_t timeStamp;
    siginfo_t siginfo;
    ucontext_t context;
} ProcessDumpRequest;

typedef struct SignalInfo_ {
    int sigNo;
    const char *name;
} SignalInfo;

void InstallLocalSignalHandler(void);

#ifdef __cplusplus
}
#endif
#endif