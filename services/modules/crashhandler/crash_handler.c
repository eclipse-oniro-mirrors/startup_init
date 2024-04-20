
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
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/capability.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <linux/futex.h>

#include <libunwind.h>

#include "beget_ext.h"
#include "securec.h"
#include "init_cmds.h"
#include "init_log.h"
#include "crash_handler.h"

#define PATHSIZE_MAX 256
#define BEGET_DFX_CHECK(fd, retCode, exper, fmt, ...) \
    if (!(retCode)) {                \
        PrintLog(fd, fmt, ##__VA_ARGS__);     \
        BEGET_LOGE(fmt, ##__VA_ARGS__);       \
        exper;                       \
    }

static void *g_reservedChildStack = NULL;
static ProcessDumpRequest g_request;
static const SignalInfo g_platformSignals[] = {
    { SIGABRT, "SIGABRT" },
    { SIGBUS, "SIGBUS" },
    { SIGFPE, "SIGFPE" },
    { SIGILL, "SIGILL" },
    { SIGSEGV, "SIGSEGV" },
#if defined(SIGSTKFLT)
    { SIGSTKFLT, "SIGSTKFLT" },
#endif
    { SIGSYS, "SIGSYS" },
    { SIGTRAP, "SIGTRAP" },
};

__attribute__((noinline)) void PrintLog(int fd, const char *format, ...)
{
    char buf[BUF_SZ] = {0};
    va_list args;
    va_start(args, format);
    int size = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format, args);
    if (size == -1) {
        BEGET_LOGE("Failed to sprintf %s", format);
        va_end(args);
        return;
    }
    va_end(args);
    if (fd < 0) {
        return;
    }
    (void)write(fd, buf, strlen(buf));
}

static int SetRegister(unw_context_t *context, const ucontext_t *uc)
{
#if defined(__arm__)
    (void)memset_s(context, sizeof(*context), 0, sizeof(*context));
    context->regs[ARM_R0] = uc->uc_mcontext.arm_r0;
    context->regs[ARM_R1] = uc->uc_mcontext.arm_r1;
    context->regs[ARM_R2] = uc->uc_mcontext.arm_r2;
    context->regs[ARM_R3] = uc->uc_mcontext.arm_r3;
    context->regs[ARM_R4] = uc->uc_mcontext.arm_r4;
    context->regs[ARM_R5] = uc->uc_mcontext.arm_r5;
    context->regs[ARM_R6] = uc->uc_mcontext.arm_r6;
    context->regs[ARM_R7] = uc->uc_mcontext.arm_r7;
    context->regs[ARM_R8] = uc->uc_mcontext.arm_r8;
    context->regs[ARM_R9] = uc->uc_mcontext.arm_r9;
    context->regs[ARM_R10] = uc->uc_mcontext.arm_r10;
    context->regs[ARM_FP] = uc->uc_mcontext.arm_fp;
    context->regs[ARM_IP] = uc->uc_mcontext.arm_ip;
    context->regs[ARM_SP] = uc->uc_mcontext.arm_sp;
    context->regs[ARM_LR] = uc->uc_mcontext.arm_lr;
    context->regs[ARM_PC] = uc->uc_mcontext.arm_pc;
    BEGET_LOGE("fp:%08x ip:%08x sp:%08x lr:%08x pc:%08x\n",
        uc->uc_mcontext.arm_fp, uc->uc_mcontext.arm_ip, uc->uc_mcontext.arm_sp,
        uc->uc_mcontext.arm_lr, uc->uc_mcontext.arm_pc);
#elif defined(__aarch64__)
    // the ucontext.uc_mcontext.__reserved of libunwind is simplified with the system's own in aarch64
    if (memcpy_s(context, sizeof(unw_context_t), uc, sizeof(unw_context_t)) != 0) {
        return -1;
    }
#endif
    return 0;
}

static void PrintfRegister(const unw_context_t *context, int fd)
{
    PrintLog(fd, "Registers:\n");
#if defined(__arm__)
    PrintLog(fd, "r0:%08x r1:%08x r2:%08x r3:%08x\n",
        context->regs[ARM_R0], context->regs[ARM_R1], context->regs[ARM_R2], context->regs[ARM_R3]);
    PrintLog(fd, "r4:%08x r5:%08x r6:%08x r7:%08x\n",
        context->regs[ARM_R4], context->regs[ARM_R5], context->regs[ARM_R6], context->regs[ARM_R7]);
    PrintLog(fd, "r8:%08x r9:%08x r10:%08x\n",
        context->regs[ARM_R8], context->regs[ARM_R9], context->regs[ARM_R10]);
    PrintLog(fd, "fp:%08x ip:%08x sp:%08x lr:%08x pc:%08x\n",
        context->regs[ARM_FP], context->regs[ARM_IP], context->regs[ARM_SP],
        context->regs[ARM_LR], context->regs[ARM_PC]);
#elif defined(__aarch64__)
    PrintLog(fd, "x0:%016lx x1:%016lx x2:%016lx x3:%016lx\n", \
        context->uc_mcontext.regs[AARCH64_X0], context->uc_mcontext.regs[AARCH64_X1],
        context->uc_mcontext.regs[AARCH64_X2], context->uc_mcontext.regs[AARCH64_X3]);
    PrintLog(fd, "x4:%016lx x5:%016lx x6:%016lx x7:%016lx\n", \
        context->uc_mcontext.regs[AARCH64_X4], context->uc_mcontext.regs[AARCH64_X5],
        context->uc_mcontext.regs[AARCH64_X6], context->uc_mcontext.regs[AARCH64_X7]);
    PrintLog(fd, "x8:%016lx x9:%016lx x10:%016lx x11:%016lx\n", \
        context->uc_mcontext.regs[AARCH64_X8], context->uc_mcontext.regs[AARCH64_X9],
        context->uc_mcontext.regs[AARCH64_X10], context->uc_mcontext.regs[AARCH64_X11]);
    PrintLog(fd, "x12:%016lx x13:%016lx x14:%016lx x15:%016lx\n", \
        context->uc_mcontext.regs[AARCH64_X12], context->uc_mcontext.regs[AARCH64_X13],
        context->uc_mcontext.regs[AARCH64_X14], context->uc_mcontext.regs[AARCH64_X15]);
    PrintLog(fd, "x16:%016lx x17:%016lx x18:%016lx x19:%016lx\n", \
        context->uc_mcontext.regs[AARCH64_X16], context->uc_mcontext.regs[AARCH64_X17],
        context->uc_mcontext.regs[AARCH64_X18], context->uc_mcontext.regs[AARCH64_X19]);
    PrintLog(fd, "x20:%016lx x21:%016lx x22:%016lx x23:%016lx\n", \
        context->uc_mcontext.regs[AARCH64_X20], context->uc_mcontext.regs[AARCH64_X21],
        context->uc_mcontext.regs[AARCH64_X22], context->uc_mcontext.regs[AARCH64_X23]);
    PrintLog(fd, "x24:%016lx x25:%016lx x26:%016lx x27:%016lx\n", \
        context->uc_mcontext.regs[AARCH64_X24], context->uc_mcontext.regs[AARCH64_X25],
        context->uc_mcontext.regs[AARCH64_X26], context->uc_mcontext.regs[AARCH64_X27]);
    PrintLog(fd, "x28:%016lx x29:%016lx\n", \
        context->uc_mcontext.regs[AARCH64_X28], context->uc_mcontext.regs[AARCH64_X29]);
    PrintLog(fd, "lr:%016lx sp:%016lx pc:%016lx\n", \
        context->uc_mcontext.regs[AARCH64_X30], context->uc_mcontext.sp, context->uc_mcontext.pc);
#endif
}

static uint64_t GetTimeMilliseconds(void)
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return ((uint64_t)time.tv_sec * 1000) +    // 1000 : second to millisecond convert ratio
           (((uint64_t)time.tv_usec) / 1000);  // 1000 : microsecond to millisecond convert ratio
}

const char *GetSignalName(const int32_t signal)
{
    for (size_t i = 0; i < sizeof(g_platformSignals) / sizeof(g_platformSignals[0]); i++) {
        if (signal == g_platformSignals[i].sigNo) {
            return g_platformSignals[i].name;
        }
    }
    return "Uncare Signal";
}

static void GetMaps(int fd)
{
    char path[PATHSIZE_MAX] = {};
    FILE *fp = fopen("/proc/self/maps", "r");
    if (fp == NULL) {
        BEGET_LOGE("fopen /proc/self/maps failed");
        return;
    }
    while (fgets(path, PATHSIZE_MAX, fp) != NULL) {
        PrintLog(fd, "%s", path);
    }
    (void)fclose(fp);
    return;
}

__attribute__((noinline)) void ExecLocalDumpUnwinding(int fd, unw_context_t *ctx, size_t skipFrameNum)
{
    unw_cursor_t cursor;
    unw_init_local(&cursor, ctx);

    size_t index = 0;
    unw_word_t pc = 0;
    unw_word_t prevPc = 0;
    unw_word_t offset = 0;
    char symbol[SYMBOL_BUF_SIZE];
    GetMaps(fd);
    do {
        // skip 0 stack, as this is dump catcher. Caller don't need it.
        if (index < skipFrameNum) {
            index++;
            continue;
        }
        int ret = unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t *)(&pc));
        BEGET_DFX_CHECK(fd, ret == 0, break, "Failed to get current pc, stop.\n");
        size_t curIndex = index - skipFrameNum;
        BEGET_DFX_CHECK(fd, !(curIndex > 1 && prevPc == pc), break, "Invalid pc %l %l, stop.", prevPc, pc);
        prevPc = pc;

        (void)memset_s(&symbol, sizeof(symbol), 0, sizeof(symbol));
        if (unw_get_proc_name(&cursor, symbol, sizeof(symbol), (unw_word_t *)(&offset)) == 0) {
            PrintLog(fd, "#%02d %016p (%s+%lu)\n", curIndex, pc, symbol, offset);
        } else {
            PrintLog(fd, "#%02d %016p\n", curIndex, pc);
        }
        index++;
    } while ((unw_step(&cursor) > 0) && (index < BACK_STACK_MAX_STEPS));
    return;
}

static void CrashLocalHandler(const ProcessDumpRequest *request)
{
    int fd = GetKmsgFd();
    BEGET_ERROR_CHECK(fd >= 0, return, "Invalid fd");

    PrintLog(fd, "Pid:%d\n", request->pid);
    PrintLog(fd, "Uid:%d\n", request->uid);
    PrintLog(fd, "Reason:Signal(%s)\n", GetSignalName(request->siginfo.si_signo));

    unw_context_t context;
    SetRegister(&context, &(request->context));
    ExecLocalDumpUnwinding(fd, &context, 0);
    PrintfRegister(&context, fd);
    (void)fsync(fd);
    (void)close(fd);
}

static void ReserveChildThreadSignalStack(void)
{
    // reserve stack for fork
    g_reservedChildStack = mmap(NULL, LOCAL_HANDLER_STACK_SIZE, PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (g_reservedChildStack == NULL) {
        BEGET_LOGE("Failed to alloc memory for child stack.");
        return;
    }
    g_reservedChildStack = (void *)(((uint8_t *)g_reservedChildStack) + LOCAL_HANDLER_STACK_SIZE - 1);
}

static int DoCrashHandler(void *arg)
{
    BEGET_LOGI("DoCrashHandler");
    (void)arg;
    CrashLocalHandler(&g_request);
    ExecReboot("panic");
    return 0;
}

static void SignalHandler(int sig, siginfo_t *si, void *context)
{
    (void)memset_s(&g_request, sizeof(g_request), 0, sizeof(g_request));
    g_request.type = sig;
    g_request.tid = gettid();
    g_request.pid = getpid();
    g_request.timeStamp = GetTimeMilliseconds();
    BEGET_LOGI("CrashHandler :: sig(%d), pid(%d), tid(%d).", sig, g_request.pid, g_request.tid);

    int ret = memcpy_s(&(g_request.siginfo), sizeof(siginfo_t), si, sizeof(siginfo_t));
    if (ret < 0) {
        BEGET_LOGE("memcpy_s siginfo fail, ret=%d", ret);
    }
    ret = memcpy_s(&(g_request.context), sizeof(ucontext_t), context, sizeof(ucontext_t));
    if (ret < 0) {
        BEGET_LOGE("memcpy_s context fail, ret=%d", ret);
    }

    int pseudothreadTid = -1;
    pid_t childTid = clone(DoCrashHandler, g_reservedChildStack,
                           CLONE_THREAD | CLONE_SIGHAND | CLONE_VM | CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID,
                           &pseudothreadTid, NULL, NULL, &pseudothreadTid);
    if (childTid == -1) {
        BEGET_LOGE("Failed to create thread for crash local handler");
        ExecReboot("panic");
    } else {
        sleep(5); // wait 5s
        ExecReboot("panic");
    }

    BEGET_LOGI("child thread(%d) exit.", childTid);
}

void InstallLocalSignalHandler(void)
{
    ReserveChildThreadSignalStack();
    sigset_t set;
    sigemptyset(&set);
    struct sigaction action;
    memset_s(&action, sizeof(action), 0, sizeof(action));
    action.sa_sigaction = SignalHandler;
    action.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;

    for (size_t i = 0; i < sizeof(g_platformSignals) / sizeof(g_platformSignals[0]); i++) {
        int32_t sig = g_platformSignals[i].sigNo;
        sigemptyset(&action.sa_mask);
        sigaddset(&action.sa_mask, sig);

        sigaddset(&set, sig);
        if (sigaction(sig, &action, NULL) != 0) {
            BEGET_LOGE("Failed to register signal(%d)", sig);
        }
    }
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}