/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "loop_systest.h"
#include "loop_event.h"
#include "le_socket.h"
#include "le_task.h"
#include "list.h"

#define SLEEP_DURATION 3000 // us
#define EXIT_TIMEOUT 1000000 // us
#define APP_STATE_IDLE 1
#define APP_STATE_SPAWNING 2
#define APP_MAX_TIME 3000000

typedef void (* CallbackControlProcess)(uint16_t type, const char *serviceCmd, const void *context);

static Message *g_message = NULL;
CallbackControlProcess g_controlFunc = NULL;

typedef struct {
    uint16_t tlvLen;
    uint16_t tlvType;
} Tlv;

typedef enum {
    ACTION_SANDBOX = 0,
    ACTION_DUMP,
    ACTION_MODULEMGR,
    ACTION_SPAWNTIME,
    ACTION_MAX
} ActionType;

typedef struct {
    uint16_t tlvLen;  // 对齐后的长度
    uint16_t tlvType;
    uint16_t dataLen;  // 数据的长度
    uint16_t dataType;
    char tlvName[TLV_NAME_LEN];
} TlvExt;

typedef struct {
    Message msgHdr;
    Result result;
}ResponseMsg;

typedef struct ReceiverCtx_ {
    uint32_t nextMsgId;              // 校验消息id
    uint32_t msgRecvLen;             // 已经接收的长度
    TimerHandle timer;               // 测试消息完整
    MsgNode *incompleteMsg;          // 保存不完整的消息
} ReceiverCtx;

typedef struct MyTask_ {
    TaskHandle stream;
    int id;
    ReceiverCtx ctx;
} MyTask;

typedef struct MyService_ {
    TaskHandle serverTask;
    struct ListNode head;
} MyService;

typedef struct MsgNode_ {
    MyTask *task;
    Message msgHeader;
    uint32_t tlvCount;
    uint32_t *tlvOffset;
    uint8_t *buffer;
} MsgNode;

static MyService g_service = NULL;
static AppMgr *g_appMgr = NULL;

int MakeDirRec(const char *path, mode_t mode, int lastPath)
{
    if (path == NULL || *path == '\0') {
        printf("Invalid path to create \n");
        return -1;
    }

    char buffer[PATH_MAX] = {0};
    const char slash = '/';
    const char *p = path;
    char *curPos = strchr(path, slash);
    while (curPos != NULL) {
        int len = curPos - p;
        p = curPos + 1;
        if (len == 0) {
            curPos = strchr(p, slash);
            continue;
        }

        int ret = memcpy_s(buffer, PATH_MAX, path, p - path - 1);
        if (ret != 0) {
            printf("Failed to copy path \n");
            return -1;
        }

        ret = mkdir(buffer, mode);
        if (ret == -1 && errno != EEXIST) {
            return errno;
        }
        curPos = strchr(p, slash);
    }

    if (lastPath) {
        if (mkdir(path, mode) == -1 && errno != EEXIST) {
            return errno;
        }
    }
    return 0;
}

static inline void SetFdCtrl(int fd, int opt)
{
    int option = fcntl(fd, F_GETFD);
    int ret = fcntl(fd, F_SETFD, option | opt);
    if (ret < 0) {
        printf("Set fd %d option %d %d result: %d \n", fd, option, opt, errno);
    }
}

static int CreatePipeServer(TaskHandle *server, const char *name)
{
    char path[128] = {0};
    int ret = snprintf_s(path, sizeof(path), sizeof(path) - 1, "%s%s", SOCKET_DIR, name);
    if (ret < 0) {
        printf("Failed to snprintf_s %d \n", ret);
    }
    int socketId = GetControlSocket(name);
    printf("get socket from env %s socketId %d \n", SOCKET_NAME, socketId);

    LE_StreamInfo info = {};
    info.baseInfo.flags = TASK_STREAM | TASK_PIPE | TASK_SERVER;
    info.socketId = socketId;
    info.server = server;
    info.baseInfo.close = NULL;
    info.incommingConnect = OnConnection;

    MakeDirRec(path, DIR_MODE, 0);
    ret = LE_CreateStreamServer(LE_GetDefaultLoop(), server, &info);
    if (ret < 0) {
        printf("Create server failed \n");
    }
    SetFdCtrl(LE_GetSocketFd(*server), FD_CLOEXEC);

    printf("CreateServer path %s fd %d \n", path, LE_GetSocketFd(*server));
    return 0;
}

static int CreateTcpServer(TaskHandle *server, const char *name)
{
    char path[128] = {0};
    int ret = snprintf_s(path, sizeof(path), sizeof(path) - 1, "%s%s", SOCKET_DIR, name);
    if (ret < 0) {
        printf("Failed to snprintf_s %d \n", ret);
    }
    int socketId = GetControlSocket(name);
    printf("get socket from env %s socketId %d \n", SOCKET_NAME, socketId);

    LE_StreamInfo info = {};
    info.baseInfo.flags = TASK_STREAM | TASK_TCP | TASK_SERVER;
    info.socketId = socketId;
    info.server = server;
    info.baseInfo.close = NULL;
    info.incommingConnect = OnConnection;

    MakeDirRec(path, DIR_MODE, 0);
    ret = LE_CreateStreamServer(LE_GetDefaultLoop(), server, &info);
    SetFdCtrl(LE_GetSocketFd(*server), FD_CLOEXEC);

    printf("CreateServer path %s fd %d \n", path, LE_GetSocketFd(*server));
    return ret;
}

void DeleteMsg(MsgNode *msgNode)
{
    if (msgNode == NULL) {
        return;
    }
    if (msgNode->buffer) {
        free(msgNode->buffer);
        msgNode->buffer = NULL;
    }
    if (msgNode->tlvOffset) {
        free(msgNode->tlvOffset);
        msgNode->tlvOffset = NULL;
    }
    free(msgNode);
}

static void WaitMsgCompleteTimeOut(const TimerHandle taskHandle, void *context)
{
    MyTask *task = (MyTask *)context;
    printf("Long time no msg complete so close connectionId: %u \n", connection->connectionId);
    DeleteMsg(connection->receiverCtx.incompleteMsg);
    connection->receiverCtx.incompleteMsg = NULL;
    LE_CloseStreamTask(LE_GetDefaultLoop(), connection->stream);
}

static inline int StartTimerForCheckMsg(MyTask *task)
{
    if (task->receiverCtx.timer != NULL) {
        return 0;
    }
    int ret = LE_CreateTimer(LE_GetDefaultLoop(), &task->receiverCtx.timer, WaitMsgCompleteTimeOut, task);
    if (ret == 0) {
        ret = LE_StartTimer(LE_GetDefaultLoop(), task->receiverCtx.timer, MAX_WAIT_MSG_COMPLETE, 1);
    }
    return ret;
}

static int SendMessage(LoopHandle loop, TaskHandle task, const char *message)
{
    if (message == NULL) {
        printf("message is null \n");
        return -1;
    }
    BufferHandle handle = NULL;
    uint32_t bufferSize = strlen(message) + 1;
    handle = LE_CreateBuffer(loop, bufferSize);
    char *buff = (char *)LE_GetBufferInfo(handle, NULL, &bufferSize);
    if (buff == NULL) {
        printf("Failed get buffer info \n");
        return -1;
    }

    int ret = memcpy_s(buff, bufferSize, message, strlen(message) + 1);
    if (ret != 0) {
        LE_FreeBuffer(loop, task, handle);
        printf("Failed memcpy_s err=%d \n", errno);
        return -1;
    }

    LE_STATUS status = LE_Send(loop, task, handle, strlen(message) + 1);
    if (status != LE_SUCCESS) {
        printf("Failed le_send msg \n");
        return -1;
    }
    return 0;
}

static int OnConnection(const LoopHandle loop, const TaskHandle server)
{
    TaskHandle stream = NULL;
    LE_StreamInfo info = {};
    info.baseInfo.flags = flags;
    info.baseInfo.close = OnClose;
    info.baseInfo.userDataSize = sizeof(MyTask);
    info.disConnectComplete = OnDisConnect;
    info.sendMessageComplete = sendMessageComplete;
    info.recvMessage = OnRecvMessage;

    int ret = LE_AcceptStreamClient(loop, server, &stream, &info);
    if (ret != 0) {
        printf("Failed to accept stream \n");
    }

    MyTask *agent = (MyTask *)LE_GetUserData(stream);
    // 收到数据后的处理
    static uint32_t connectionId = 0;
    agent->id = ++connectionId;
    agent->stream = stream;
    agent->ctx.timer = NULL;
    agent->incompleteMsg = NULL;
    
    printf("accept id: %d\n", agent->id);
    ret = SendMessage(loop, agent->stream, "Connect Success.");
    if (ret != 0) {
        printf("Failed to send msg \n");
        return -1;
    }

    return 0;
}

static void OnClose(const TaskHandle taskHandle)
{
    MyTask *task = (MyTask *)LE_GetUserData(taskHandle);
    if (task == NULL) {
        printf("Invalid task \n");
        return;
    }
}

static void OnDisConnect(const TaskHandle taskHandle)
{
    MyTask *task = (MyTask *)LE_GetUserData(taskHandle);
    if (task == NULL) {
        printf("Invalid task \n");
        return;
    }
    printf("task id: %d\n", task->id);
    OnClose(taskHandle);
}

static void SendMessageComplete(const TaskHandle taskHandle, BufferHandle handle)
{
    MyTask *task = (MyTask *)LE_GetUserData(taskHandle);
    if (task == NULL) {
        printf("Invalid task \n");
        return;
    }
    uint32_t bufferSize = sizeof(Message);
    Message *msg = (Message *)LE_GetBufferInfo(handle, NULL, &bufferSize);
    if (msg == NULL) {
        return;
    }
    printf("SendMessageComplete taskId: %u msgId %u msgType %u buf %s",
        task->id, msg->msgId, msg->msgType, msg->buffer);
}

void ServerInit(const char *server, LoopHandle loop, int flags)
{
    if (server == NULL || loop == NULL) {
        printf("Invalid parameter\n");
        return;
    }

    OH_ListInit(&g_service.head);
    LE_StreamServerInfo info = {};
    info.baseInfo.flags = flags;
    info.baseInfo.close = NULL;
    info.socketId = -1;
    info.server = server;
    info.disConnectComplete = NULL;
    info.incomingConnect = OnConnection;
    info.sendMessageComplete = NULL;
    info.recvMessage = NULL;

    int ret = LE_CreateStreamServer(loop, &g_service.serverTask, &info);
    if (ret != 0) {
        printf("Init server failed.\n");
    }
}

static int MsgRebuild(MsgNode *message, const Message *msg)
{
    if (CheckMsg(&message->msgHeader) != 0) {
        return MSG_INVALID;
    }
    if (msg->msgLen == sizeof(message->msgHeader)) {  // only has msg header
        return 0;
    }
    if (message->buffer == NULL) {
        message->buffer = calloc(1, msg->msgLen - sizeof(message->msgHeader));
        if (message->buffer == NULL) {
            printf("Failed to alloc memory for recv message \n");
            return -1;
        }
    }
    if (message->tlvOffset == NULL) {
        uint32_t totalCount = msg->tlvCount + TLV_MAX;
        message->tlvOffset = malloc(totalCount * sizeof(uint32_t));
        if (message->tlvOffset == NULL) {
            printf("Failed to alloc memory for recv message \n");
            return -1;
        }
        for (uint32_t i = 0; i < totalCount; i++) {
            message->tlvOffset[i] = INVALID_OFFSET;
        }
    }
    return 0;
}

AppMgr *CreateMessage()
{
    if (g_appMgr != NULL) {
        return g_appMgr;
    }
    AppMgr *appMgr = (AppMgr *)calloc(1, sizeof(AppMgr));
    if (appMgr == NULL) {
        printf("Failed to alloc memory \n");
        return NULL;
    }

    appMgr->content.longProcName = NULL;
    appMgr->content.longProcNameLen = 0;
    appMgr->content.mode = mode;
    appMgr->content.sandboxNsFlags = 0;
    appMgr->content.wdgOpened = 0;
    appMgr->servicePid = getpid();
    appMgr->server = NULL;
    appMgr->sigHandler = NULL;
    OH_ListInit(&appMgr->appQueue);
    OH_ListInit(&appMgr->diedQueue);
    appMgr->diedAppCount = 0;
    OH_ListInit(&appMgr->extData);
    g_appMgr = appMgr;
    g_appMgr->spawnTime.minTime = APP_MAX_TIME;
    g_appMgr->spawnTime.maxTime = 0;
    return appMgr;
}

AppMgr *GetAppMgr(void)
{
    return g_appMgr;
}

MgrContent *GetMgrContent(void)
{
    return g_appMgr == NULL ? NULL : &g_appMgr->content;
}

int GetMsgFromBuffer(const uint8_t *buffer, uint32_t bufferLen,
    Message **outMsg, uint32_t *msgRecvLen, uint32_t *reminder)
{
    if (buffer == NULL || outMsg == NULL || msgRecvLen == NULL || reminder == NULL) {
        return MSG_INVALID;
    }

    *reminder = 0;
    Message *message = *outMsg;
    if (message == NULL) {
        message = CreateMessage();
        if (message == NULL) {
            printf("Failed to create message \n");
            return SYSTEM_ERROR;
        }
        *outMsg = message;
    }

    uint32_t reminderLen = bufferLen;
    const uint8_t *reminderBuffer = buffer;
    if (*msgRecvLen < sizeof(Message)) {  // recv partial message
        if ((bufferLen + *msgRecvLen) >= sizeof(Message)) {
            int ret = memcpy_s(((uint8_t *)&message->msgHeader) + *msgRecvLen,
                sizeof(message->msgHeader) - *msgRecvLen,
                buffer, sizeof(message->msgHeader) - *msgRecvLen);
            if (ret != 0) {
                printf("Failed to copy recv buffer \n");
                return -1;
            }

            ret = MsgRebuild(message, &message->msgHeader);
            if (ret != 0) {
                printf("Failed to alloc buffer for receive msg \n");
                return -1;
            }
            reminderLen = bufferLen - (sizeof(message->msgHeader) - *msgRecvLen);
            reminderBuffer = buffer + sizeof(message->msgHeader) - *msgRecvLen;
            *msgRecvLen = sizeof(message->msgHeader);
        } else {
            int ret = memcpy_s(((uint8_t *)&message->msgHeader) + *msgRecvLen,
                sizeof(message->msgHeader) - *msgRecvLen, buffer, bufferLen);
            if (ret != 0) {
                printf("Failed to copy recv buffer \n");
                return -1;
            }
            *msgRecvLen += bufferLen;
            return 0;
        }
    }
    return 0;
}

int DecodeMsg(Message * message)
{
    if (message == NULL) {
        printf("decode empty message, failed! \n");
        return -1;
    }
    /*
        写解析消息的逻辑
    */
    return 0;
}

inline int IsNWebMode(const AppMgr *content)
{
    return (content != NULL) &&
        (content->content.mode == MODE_FOR_NWEB_SPAWN || content->content.mode == MODE_FOR_NWEB_COLD_RUN);
}

int ProcessTerminationStatusMsg(const MsgNode *message, Result *result)
{
    if (message == NULL || result == NULL) {
        return -1;
    }
    if (!IsNWebMode(g_appMgr)) {
        return -1;
    }
    result->result = -1;
    result->pid = 0;
    pid_t *pid = (pid_t *)GetMsgInfo(message, TLV_RENDER_TERMINATION_INFO);
    if (pid == NULL) {
        return -1;
    }
    // get render process termination status, only nwebspawn need this logic.
    result->pid = *pid;
    result->result = GetProcessTerminationStatus(*pid);
    return 0;
}

static int SendResponse(const MyTask *task, const Message *msg, int result, pid_t pid)
{
    printf("SendResponse connectionId: %u result: 0x%x pid: %d \n",
        task->id, result, pid);
    uint32_t bufferSize = sizeof(ResponseMsg);
    BufferHandle handle = LE_CreateBuffer(LE_GetDefaultLoop(), bufferSize);
    ResponseMsg *buffer = (ResponseMsg *)LE_GetBufferInfo(handle, NULL, &bufferSize);
    int ret = memcpy_s(buffer, bufferSize, msg, sizeof(Message));
    if (ret != 0) {
        LE_FreeBuffer(LE_GetDefaultLoop(), NULL, handle);
        printf("Failed to memcpy_s bufferSize \n");
        return -1;
    }

    buffer->result.result = result;
    buffer->result.pid = pid;
    return LE_Send(LE_GetDefaultLoop(), task->stream, handle, bufferSize);
}

void DeleteMsg(MsgNode *msgNode)
{
    if (msgNode == NULL) {
        return;
    }
    if (msgNode->buffer) {
        free(msgNode->buffer);
        msgNode->buffer = NULL;
    }
    if (msgNode->tlvOffset) {
        free(msgNode->tlvOffset);
        msgNode->tlvOffset = NULL;
    }
    free(msgNode);
}

int CheckMsg(const MsgNode *message)
{
    if (message == NULL) {
        return MSG_INVALID;
    }
    if (strlen(message->msgHeader.processName) <= 0) {
        printf("Invalid property processName %s \n", message->msgHeader.buffer);
        return MSG_INVALID;
    }
    if (message->tlvOffset == NULL) {
        printf("Invalid property tlv offset %s \n", message->msgHeader.buffer);
        return MSG_INVALID;
    }
    if (message->buffer == NULL) {
        printf("Invalid property buffer %s \n", message->msgHeader.buffer);
        return MSG_INVALID;
    }

    if (message->tlvOffset[TLV_BUNDLE_INFO] == INVALID_OFFSET ||
        message->tlvOffset[TLV_MSG_FLAGS] == INVALID_OFFSET ||
        message->tlvOffset[TLV_ACCESS_TOKEN_INFO] == INVALID_OFFSET ||
        message->tlvOffset[TLV_DOMAIN_INFO] == INVALID_OFFSET ||
        message->tlvOffset[TLV_DAC_INFO] == INVALID_OFFSET) {
        printf("No must tlv bundle: %u flags: %u token: %u domain %u %u \n",
            message->tlvOffset[TLV_BUNDLE_INFO], message->tlvOffset[TLV_MSG_FLAGS],
            message->tlvOffset[TLV_ACCESS_TOKEN_INFO],
            message->tlvOffset[TLV_DOMAIN_INFO], message->tlvOffset[TLV_DAC_INFO]);
        return MSG_INVALID;
    }

    return 0;
}

static void ProcessReqMsg(MyTask *task, MsgNode *message)
{
    int ret = CheckMsg(message);
    if (ret != 0) {
        SendResponse(task, &message->msgHeader, ret, 0);
        DeleteMsg(message);
        return;
    }

    message->task = task;
}

void *GetMsgExtInfo(const MsgNode *message, const char *name, uint32_t *len)
{
    if (name == NULL) {
        printf("Invalid name \n");
        return NULL;
    }
    if (message == NULL || message->buffer == NULL || message->tlvOffset == NULL) {
        return NULL;
    }
    printf("GetMsgExtInfo tlvCount %d name %s \n", message->tlvCount, name);

    for (uint32_t index = TLV_MAX; index < (TLV_MAX + message->tlvCount); index++) {
        if (message->tlvOffset[index] == INVALID_OFFSET) {
            return NULL;
        }
        uint8_t *data = message->buffer + message->tlvOffset[index];
        if (((Tlv *)data)->tlvType != TLV_MAX) {
            continue;
        }
        TlvExt *tlv = (TlvExt *)data;
        if (strcmp(tlv->tlvName, name) != 0) {
            continue;
        }
        if (len != NULL) {
            *len = tlv->dataLen;
        }
        return data + sizeof(TlvExt);
    }
    return NULL;
}

MsgNode *CreateMsg(void)
{
    MsgNode *message = (MsgNode *)calloc(1, sizeof(MsgNode));
    if (message == NULL) {
        printf("Failed to create message \n");
        return NULL;
    }

    message->buffer = NULL;
    message->tlvOffset = NULL;
    return message;
}

MsgNode *RebuildMsgNode(MsgNode *message, Process *info)
{
#ifdef DEBUG_BEGETCTL_BOOT
    if (message == NULL || info == NULL) {
        printf("params is null \n");
        return NULL;
    }

    uint32_t bufferLen = 0;
    MsgNode *node = CreateMsg();
    if (node == NULL) {
        printf("Failed to create MsgNode \n");
        return NULL;
    }

    int ret = memcpy_s(&node->msgHeader, sizeof(Message), &message->msgHeader, sizeof(Message));
    if (ret != 0) {
        printf("Failed to memcpy_s node->msgHeader \n");
        return NULL;
    }

    bufferLen = message->msgHeader.msgLen + info->message->msgHeader.msgLen - sizeof(Message);
    node->msgHeader.msgLen = bufferLen;
    node->msgHeader.msgType = MSG_NATIVE_PROCESS;
    node->msgHeader.tlvCount += message->msgHeader.tlvCount;
    ret = MsgRebuild(node, &node->msgHeader);
    if (ret != 0) {
        DeleteMsg(node);
        printf("Failed to alloc memory for recv message \n");
        return NULL;
    }

    uint32_t infoBufLen = info->message->msgHeader.msgLen - sizeof(Message);
    uint32_t msgBufLen = message->msgHeader.msgLen - sizeof(Message);
    ret = memcpy_s(node->buffer, bufferLen, info->message->buffer, infoBufLen);
    if (ret != 0) {
        DeleteMsg(node);
        printf("Failed to memcpy_s info buffer \n");
        return NULL;
    }
    ret = memcpy_s(node->buffer + infoBufLen, bufferLen - infoBufLen, message->buffer, msgBufLen);
    if (ret != 0) {
        DeleteMsg(node);
        printf("Failed to memcpy_s message->buffer \n");
        return NULL;
    }
    return node;
#endif
    return NULL;
}

static MsgNode *ProcessBegetctlMsg(MyTask *task, MsgNode *message)
{
    uint32_t len = 0;
    const char *msg = (const char *)GetMsgExtInfo(message, MSG_EXT_NAME_BEGET_PID, &len);
    if (msg == NULL) {
        printf("Failed to get extInfo \n");
        return NULL;
    }

    MsgNode *msgNode = RebuildMsgNode(message, info);
    if (msgNode == NULL) {
        printf("Failed to rebuild message node \n");
        return NULL;
    }

    int ret = DecodeMsg(msgNode);
    if (ret != 0) {
        DeletenMsg(msgNode);
        return NULL;
    }
    return msgNode;
}

static void ProcessBegetMsg(MyTask *task, MsgNode *message)
{
    Message *msg = &message->msgHeader;

    MsgNode *msgNode = ProcessBegetctlMsg(connection, message);
    if (msgNode == NULL) {
        SendResponse(task, msg, DEBUG_MODE_NOT_SUPPORT, 0);
        DeleteMsg(message);
        return;
    }
    ProcessReqMsg(task, msgNode);
    DeleteMsg(message);
    DeleteMsg(msgNode);
}

static void ProcessRecvMsg(MyTask *task, MsgNode *message)
{
    Message *msg = &message->msgHeader;
    printf("Recv message header magic 0x%x type %u id %u len %u %s \n",
        msg->magic, msg->msgType, msg->msgId, msg->msgLen, msg->buffer);
    if (task->receiverCtx.nextMsgId != msg->msgId) {
        printf("Invalid msg id %u %u \n", task->receiverCtx.nextMsgId, msg->msgId);
    }
    task->receiverCtx.nextMsgId++;

    int ret;
    switch (msg->msgType) {
        case MSG_GET_RENDER_TERMINATION_STATUS: {  // get status
            Result result = {0};
            ret = ProcessTerminationStatusMsg(message, &result);
            SendResponse(task, msg, ret == 0 ? result.result : ret, result.pid);
            DeleteMessage(message);
            break;
        }
        case MSG_NORMAL: {
            ProcessReqMsg(task, message);
            break;
        }
        case MSG_DUMP:
            SendResponse(task, msg, 0, 0);
            DeleteMessage(message);
            break;
        case MSG_BEGET: {
            ProcessBegetMsg(task, message);
            break;
        }
        case MSG_BEGET_TIME:
            SendResponse(task, msg, MIN_TIME, MAX_TIME);
            DeleteMessage(message);
            break;
        case MSG_UPDATE_MOUNT_POINTS:
            ret = ProcessRemountMsg(task, message);
            SendResponse(task, msg, ret, 0);
            break;
        case MSG_RESTART:
            ProcessRestartMsg(task, message);
            break;
        default:
            SendResponse(task, msg, MSG_INVALID, 0);
            DeleteMessage(message);
            break;
    }
}

static void OnReceiveRequest(const TaskHandle taskHandle, const uint8_t *buffer, uint32_t buffLen)
{
    MyTask *task = (MyTask *)LE_GetUserData(taskHandle);
    if (task == NULL) {
        printf("Failed to get client form socket\n");
        LE_CloseTask(LE_GetDefaultLoop(), taskHandle);
        return;
    }

    if (buffLen >= MAX_MSG_TOTAL_LENGTH) {
        printf("Message too long %u \n", buffLen);
        LE_CloseTask(LE_GetDefaultLoop(), taskHandle);
        return;
    }

    uint32_t reminder = 0;
    uint32_t currLen = 0;
    Message *message = task->ctx.incompleteMsg; // incomplete msg
    task->ctx.incompleteMsg = NULL;
    int ret = 0;
    do {
        printf("OnReceiveRequest connectionId: %u start: 0x%x buffLen %d",
            task->id, *(uint32_t *)(buffer + currLen), buffLen - currLen);

        ret = GetMsgFromBuffer(buffer + currLen, buffLen - currLen,
            &message, &task->ctx.msgRecvLen, &reminder);
        if (ret != 0) {
            break;
        }

        if (task->ctx.msgRecvLen != message->msgLen) {  // recv complete msg
            task->ctx.incompleteMsg = message;
            message = NULL;
            break;
        }
        task->ctx.msgRecvLen = 0;
        if (task->ctx.timer) {
            LE_StopTimer(LE_GetDefaultLoop(), task->ctx.timer);
            task->ctx.timer = NULL;
        }
        // decode msg
        ret = DecodeMsg(message);
        if (ret != 0) {
            break;
        }
        (void)ProcessRecvMsg(task, message);
        message = NULL;
        currLen += buffLen - reminder;
    } while (reminder > 0);

    if (task->ctx.incompleteMsg != NULL) { // Start the detection timer
        ret = StartTimerForCheckMsg(task);
        if (ret != 0) {
            LE_CloseStreamTask(LE_GetDefaultLoop(), taskHandle);
        }
    }
}

void ServiceInit(const char *socketPath, CallbackControlProcess func, LoopHandle loop)
{
    if ((socketPath == NULL) || (func == NULL) || (loop == NULL)) {
        BEGET_LOGE("[control_fd] Invalid parameter");
        return;
    }
    OH_ListInit(&g_cmdService.head);
    LE_StreamServerInfo info = {};
    info.baseInfo.flags = TASK_STREAM | TASK_SERVER | TASK_PIPE;
    info.server = (char *)socketPath;
    info.socketId = -1;
    info.baseInfo.close = NULL;
    info.disConnectComplete = NULL;
    info.incommingConnect = CmdOnIncommingConnect;
    info.sendMessageComplete = NULL;
    info.recvMessage = NULL;
    g_controlFunc = func;
    if (g_controlFdLoop == NULL) {
        g_controlFdLoop = loop;
    }
    (void)LE_CreateStreamServer(g_controlFdLoop, &g_cmdService.serverTask, &info);
}

void ProcessControl(uint16_t type, const char *serviceCmd, const void *context)
{
    if ((type >= ACTION_MAX) || (serviceCmd == NULL)) {
        return;
    }
    switch (type) {
        case ACTION_SANDBOX :
            ProcessSandboxControlFd(type, serviceCmd);
            break;
        case ACTION_DUMP :
            ProcessDumpServiceControlFd(type, serviceCmd);
            break;
        case ACTION_MODULEMGR :
            ProcessModuleMgrControlFd(type, serviceCmd);
            break;
        default :
            printf("Unknown control fd type. \n")
            break;
    }
}

void Init(const char *socketPath)
{
    ServiceInit(socketPath, ProcessControl, LE_GetDefaultLoop());
    return;
}

void TestApp()
{
    int testNum = 0;
    int ret = scanf_s("%d", &testNum);
    if (ret <= 0) {
        printf("input error \n");
        return;
    }

    char name[128];
    char context[128];
    for (int i = 0; i < testNum; ++i) {
        printf("请输入要测试的应用名称：(sandbox, dump, moudlemgr) \n");
        ret = scanf_s("%s", name, sizeof(name));
        if (ret <= 0) {
            printf("input error \n");
            return;
        }

        int app = -1;
        if (strcmp(name, "sandbox") == 0) {
            app = ACTION_SANDBOX;
        } else if (strcmp(name, "dump") == 0) {
            app = ACTION_DUMP;
        } else if (strcmp(name, "modulemgr") == 0) {
            app = ACTION_MODULEMGR;
        } else {
            printf("input error \n");
            return;
        }

        printf("请输入对应的应用输入参数：\n");
        ret = scanf_s("%s", context, sizeof(context));
        if (ret <= 0) {
            printf("input error \n");
            return;
        }

        g_controlFunc(app, context, NULL);
    }
}

int main(int argc, char *const argv[])
{
    printf("main argc: %d \n", argc);
    if (argc <= 0) {
        return 0;
    }
    
    printf("请输入创建socket的类型：(pipe, tcp)\n");
    char type[128];
    int ret = scanf_s("%s", type, sizeof(type));
    if (ret <= 0) {
        printf("input error \n");
        return 0;
    }

    int flags;
    char *server;
    if (strcmp(type, "pipe") == 0) {
        flags = TASK_STREAM | TASK_PIPE |TASK_SERVER | TASK_TEST;
        server = (char *)"/data/testpipe";
    } else if (strcmp(type, "tcp") == 0) {
        flags = TASK_STREAM | TASK_TCP |TASK_SERVER | TASK_TEST;
        server = (char *)"127.0.0.1:7777";
    } else {
        printf("输入有误，请输入pipe或者tcp!");
        system("pause");
        return 0;
    }

    Init(INIT_CONTROL_SOCKET_PATH);
    TestApp();
    
    return 0;
}