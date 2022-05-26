/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef CONTROL_FD_
#define CONTROL_FD_

#include <stdint.h>
#include "loop_event.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define INIT_CONTROL_FD_SOCKET_PATH "/dev/unix/socket/init_control_fd"

#define CONTROL_FD_FIFO_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)
#define FIFO_BUF_SIZE 4096
#define FIFO_PATH_SIZE 128

typedef struct CmdService_ {
    TaskHandle serverTask;
} CmdService;

typedef struct CmdAgent_ {
    TaskHandle task;
    WatcherHandle input; // watch stdin
    WatcherHandle reader; // watch read pipe
} CmdAgent;

typedef void (* CallbackControlFdProcess)(uint16_t type, const char *serviceCmd, const void *context);

typedef enum {
    ACTION_SANDBOX = 0,
    ACTION_DUMP,
    ACTION_PARAM_SHELL,
    ACTION_MAX
} ActionType;

typedef struct {
    uint16_t msgSize;
    uint16_t type;
    uint32_t pid;
    char fifoName[FIFO_PATH_SIZE];
    char cmd[0];
} CmdMessage;

void CmdServiceInit(const char *socketPath, CallbackControlFdProcess func);
void CmdClientInit(const char *socketPath, uint16_t type, const char *cmd, const char *fifoName);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif