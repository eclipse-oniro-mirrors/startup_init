
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
#include "init_service.h"
#include "hookmgr.h"
#include "bootstage.h"
#include "crash_handler.h"

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

static void DoCriticalInit(void)
{
#ifndef OHOS_LITE
    Service service = {
        .pid = 1,
        .name = "init",
    };

    BEGET_LOGI("ServiceReap init begin");
    HookMgrExecute(GetBootStageHookMgr(), INIT_SERVICE_REAP, (void *)&service, NULL);
    BEGET_LOGI("ServiceReap init end!");
#endif
}

static void SignalHandler(int sig, siginfo_t *si, void *context)
{
    int32_t pid = getpid();
    if (pid == 1) {
        sleep(2);
        DoCriticalInit();
        ExecReboot("panic");
    } else {
        exit(-1);
    }
}

void InstallLocalSignalHandler(void)
{
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
