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
#include <fcntl.h>
#include <limits.h>

#include "list.h"
#include "loop_event.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define INIT_CONTROL_FD_SOCKET_PATH "/dev/unix/socket/init_control_fd"

#define CONTROL_FD_FIFO_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)
#define PTY_BUF_SIZE 4096
#define PTY_PATH_SIZE 128

#ifdef STARTUP_INIT_TEST
#define CONTROL_FD_STATIC
#else
#define CONTROL_FD_STATIC static
#endif

typedef struct CmdService_ {
    TaskHandle serverTask;
    struct ListNode head;
} CmdService;

typedef struct CmdAgent_ {
    TaskHandle task;
    WatcherHandle input; // watch stdin
    WatcherHandle reader; // watch read pty
    int ptyFd;
} CmdAgent;

typedef struct CmdTask_ {
    TaskHandle task;
    struct ListNode item;
    int ptyFd;
    pid_t pid;
} CmdTask;

typedef void (* CallbackControlFdProcess)(uint16_t type, const char *serviceCmd, const void *context);

typedef enum {
    ACTION_SANDBOX = 0,
    ACTION_DUMP,
    ACTION_MODULEMGR,
    ACTION_MAX
} ActionType;

typedef struct {
    uint16_t msgSize;
    uint16_t type;
    char ptyName[PTY_PATH_SIZE];
    char cmd[0];
} CmdMessage;

void CmdServiceInit(const char *socketPath, CallbackControlFdProcess func);
void CmdClientInit(const char *socketPath, uint16_t type, const char *cmd);
void CmdServiceProcessDelClient(pid_t pid);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif