/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef BASE_STARTUP_PARAM_MESSAGE_H
#define BASE_STARTUP_PARAM_MESSAGE_H
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "loop_event.h"
#include "param_osadp.h"
#include "param_utils.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define PARAM_TEST_FLAGS 0x01
typedef enum {
    MSG_SET_PARAM,
    MSG_WAIT_PARAM,
    MSG_ADD_WATCHER,
    MSG_DEL_WATCHER,
    MSG_NOTIFY_PARAM
} ParamMsgType;

typedef enum ContentType {
    PARAM_NAME,
    PARAM_VALUE,
    PARAM_LABEL,
    PARAM_WAIT_TIMEOUT,
    PARAM_NAME_VALUE,
} ContentType;

typedef struct {
    uint8_t type;
    uint8_t rev;
    uint16_t contentSize;
    char content[0];
} ParamMsgContent;

typedef struct {
    uint32_t type;
    uint32_t msgSize;
    union {
        uint32_t msgId;
        uint32_t watcherId;
        uint32_t waitId;
    } id;
    char key[PARAM_NAME_LEN_MAX + 4];
    char data[0];
} ParamMessage;

typedef struct {
    ParamMessage msg;
    int32_t result;
} ParamResponseMessage;

typedef int (*RecvMessage)(const ParamTaskPtr stream, const ParamMessage *msg);

typedef struct {
    uint32_t flags;
    char *server;
    LE_IncommingConnect incomingConnect;
    RecvMessage recvMessage;
    LE_Close close;
} ParamStreamInfo;

int ParamServiceStop(void);
int ParamServiceStart(void);

int ParamTaskClose(const ParamTaskPtr stream);
int ParamServerCreate(ParamTaskPtr *stream, const ParamStreamInfo *streamInfo);
int ParamStreamCreate(ParamTaskPtr *stream, ParamTaskPtr server,
                      const ParamStreamInfo *streamInfo, uint16_t userDataSize);
int ParamTaskSendMsg(const ParamTaskPtr stream, const ParamMessage *msg);

int ParamEventTaskCreate(ParamTaskPtr *stream, LE_ProcessAsyncEvent eventProcess);
int ParamEventSend(const ParamTaskPtr stream, uint64_t eventId, const char *content, uint32_t size);

void *ParamGetTaskUserData(const ParamTaskPtr stream);

int FillParamMsgContent(const ParamMessage *request, uint32_t *start, int type, const char *value, uint32_t length);
ParamMsgContent *GetNextContent(const ParamMessage *reqest, uint32_t *offset);
ParamMessage *CreateParamMessage(int type, const char *name, uint32_t msgSize);

int ConnectServer(int fd, const char *servername);

#ifdef STARTUP_INIT_TEST
int ProcessMessage(const ParamTaskPtr worker, const ParamMessage *msg);
int OnIncomingConnect(LoopHandle loop, TaskHandle server);
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif // BASE_STARTUP_PARAM_MESSAGE_H