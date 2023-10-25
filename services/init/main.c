/*
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd.
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
#include <signal.h>
#include "init.h"
#include "init_log.h"
#include "init_utils.h"
#include "device.h"

static const pid_t INIT_PROCESS_PID = 1;

int main(int argc, char * const argv[])
{
    const char *uptime = NULL;
    long long upTimeInMicroSecs = 0;
    int isSecondStage = 0;
    (void)signal(SIGPIPE, SIG_IGN);
    // Number of command line parameters is 2
    if (argc > 1 && (strcmp(argv[1], "--second-stage") == 0)) {
        isSecondStage = 1;
        if (argc > 2) {
            uptime = argv[2];
        }
    } else {
        upTimeInMicroSecs = GetUptimeInMicroSeconds(NULL);
    }
    if (getpid() != INIT_PROCESS_PID) {
        INIT_LOGE("Process id error %d!", getpid());
        return 0;
    }
    EnableInitLog(INIT_INFO);

    // Updater mode
    if (isSecondStage == 0) {
        SystemPrepare(upTimeInMicroSecs);
    } else {
        LogInit();
    }

    SystemInit();
    SystemExecuteRcs();
    SystemConfig(uptime);
    SystemRun();
    return 0;
}
