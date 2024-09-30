/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef LOOP_SYSTEST_H
#define LOOP_SYSTEST_H

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "loop_event.h"
#include "le_socket.h"
#include "le_task.h"
#include "list.h"

#ifndef BASE_DIR
#define BASE_DIR ""
#endif

#define SOCKET_NAME "looptest"
#if defined(__MUSL__)
    #define SOCKET_DIR BASEDIR "/dev/unix/socket"
#else
    #define SOCKET_DIR BASEDIR "/dev/socket"
#endif

#define MIN_TIME 0
#define MAX_TIME INT_MAX
#define INVALID_OFFSET 0xffffffff
#define MAX_MSG_LEN 128
#define MAX_FD_COUNT 16
#define DIR_MODE 0711

#define TLV_NAME_LEN 32
#define MAX_MSG_BLOCK_LEN (4 * 1024)
#define RECV_BLOCK_LEN 1024
#define MAX_MSG_TOTAL_LENGTH (64 * 1024)
#define EXTRAINFO_TOTAL_LENGTH_MAX (32 * 1024)
#define MAX_TLV_COUNT 128
#define MSG_MAGIC 0xEF201234

typedef enum {
    OK = 0,
    SYSTEM_ERROR = 0xD000000,
    ARG_INVALID,
    MSG_INVALID,
    MSG_TOO_LONG,
    TLV_NOT_SUPPORT,
    TLV_NONE,
    SANDBOX_NONE,
    SANDBOX_LOAD_FAIL,
    SANDBOX_INVALID,
    SANDBOX_MOUNT_FAIL,
    TIMEOUT,
    CHILD_CRASH,
    NATIVE_NOT_SUPPORT,
    ACCESS_TOKEN_INVALID,
    PERMISSION_NOT_SUPPORT,
    BUFFER_NOT_ENOUGH,
    FORK_FAIL,
    DEBUG_MODE_NOT_SUPPORT,
    ERROR_UTILS_MEM_FAIL,
    ERROR_FILE_RMDIR_FAIL,
    NODE_EXIST,
} ErrorCode;

typedef enum {
    TLV_BUNDLE_INFO = 0,
    TLV_MSG_FLAGS,
    TLV_DAC_INFO,
    TLV_DOMAIN_INFO,
    TLV_OWNER_INFO,
    TLV_ACCESS_TOKEN_INFO,
    TLV_PERMISSION,
    TLV_INTERNET_INFO,
    TLV_RENDER_TERMINATION_INFO,
    TLV_MAX
} MsgTlvType;

typedef enum {
    MSG_NORMAL = 0,
    MSG_GET_RENDER_TERMINATION_STATUS,
    MSG_NATIVE_PROCESS,
    MSG_DUMP,
    MSG_BEGET,
    MSG_BEGET_TIME,
    MSG_UPDATE_MOUNT_POINTS,
    MSG_RESTART,
    MAX_TYPE_INVALID
} MsgType;

typedef Result_ {
    int result;
    pid_t pid;
} Result;

typedef struct Message_ {
    uint32_t magic;
    uint32_t msgType;
    uint32_t msgLen;
    uint32_t msgId;
    uint32_t tlvCount;
    char buffer[MAX_MSG_LEN];
} Message;

typedef struct {
    Message msgHdr;
    Result result;
} ResponseMsg;

typedef struct {
    uint32_t count;
    uint32_t flags[0];
} MsgFlags;

typedef struct {
    struct ListNode node;
    uint32_t reqId;
    uint32_t retryCount;
    int fdCount;
    int fds[MAX_FD_COUNT];
    MsgFlags *msgFlags;
    MsgFlags *permissionFlags;
    Message *msg;
    struct ListNode msgBlocks;  // 保存实际的消息数据
} ReqMsgNode;

#endif