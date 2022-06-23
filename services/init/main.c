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

static const pid_t INIT_PROCESS_PID = 1;

int main(int argc, char * const argv[])
{
    int isSecondStage = 0;
    (void)signal(SIGPIPE, SIG_IGN);
    // Number of command line parameters is 2
    if (argc == 2 && (strcmp(argv[1], "--second-stage") == 0)) {
        isSecondStage = 1;
    }
    if (getpid() != INIT_PROCESS_PID) {
        INIT_LOGE("Process id error %d!", getpid());
        return 0;
    }
    EnableInitLog(INIT_INFO);
    if (isSecondStage == 0) {
        SystemPrepare();
    } else {
        LogInit();
    }
    SystemInit();
    SystemExecuteRcs();
    SystemConfig();
    SystemRun();
    return 0;
}
