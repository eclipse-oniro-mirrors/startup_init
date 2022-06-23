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
#include <fcntl.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <unistd.h>

#include "beget_ext.h"
#include "control_fd.h"
#include "init_utils.h"
#include "securec.h"

static CmdService g_cmdService;

CallbackControlFdProcess g_controlFdFunc = NULL;

static void CmdOnClose(const TaskHandle task)
{
    BEGET_LOGI("[control_fd] CmdOnClose");
}

static void CmdDisConnectComplete(const TaskHandle client)
{
    BEGET_LOGI("[control_fd] CmdDisConnectComplete");
}

static void CmdOnSendMessageComplete(const TaskHandle task, const BufferHandle handle)
{
    BEGET_LOGI("[control_fd] CmdOnSendMessageComplete ");
}

static int OpenFifo(bool read, const char *pipePath)
{
    // create pipe for cmd
    if (pipePath == NULL) {
        return -1;
    }

    int flags = read ? (O_RDONLY | O_TRUNC | O_NONBLOCK) : (O_WRONLY | O_TRUNC);
    return open(pipePath, flags);
}

static void CmdOnRecvMessage(const TaskHandle task, const uint8_t *buffer, uint32_t buffLen)
{
    BEGET_LOGI("[control_fd] CmdOnRecvMessage");
    if (buffer == NULL) {
        return;
    }
    // parse msg to exec
    CmdMessage *msg = (CmdMessage *)buffer;
    if ((msg->type < 0) || (msg->type >= ACTION_MAX) || (msg->cmd[0] == '\0') || (msg->fifoName[0] == '\0')) {
        BEGET_LOGE("[control_fd] Failed msg ");
        return;
    }
    char readPath[FIFO_PATH_SIZE] = {0};
    int ret = sprintf_s(readPath, sizeof(readPath) - 1, "/dev/fifo/%s1.%d", msg->fifoName, msg->pid);
    BEGET_ERROR_CHECK(ret > 0, return, "[control_fd] Failed sprintf_s err=%d", errno);
    int reader = OpenFifo(true, readPath); // read
    BEGET_ERROR_CHECK(reader > 0, return, "[control_fd] Failed to open filo");

    char writePath[FIFO_PATH_SIZE] = {0};
    ret = sprintf_s(writePath, sizeof(writePath) - 1, "/dev/fifo/%s0.%d", msg->fifoName, msg->pid);
    BEGET_ERROR_CHECK(ret > 0, return, "[control_fd] Failed sprintf_s err=%d", errno);
    int writer = OpenFifo(false, writePath); // write
    BEGET_ERROR_CHECK(writer > 0, return, "[control_fd] Failed to open filo");

    pid_t pid = fork();
    if (pid == 0) {
        (void)dup2(reader, STDIN_FILENO);
        (void)dup2(writer, STDOUT_FILENO);
        (void)dup2(writer, STDERR_FILENO); // Redirect fd to 0, 1, 2
        (void)close(reader);
        (void)close(writer);
        g_controlFdFunc(msg->type, msg->cmd, NULL);
        exit(0);
    } else if (pid < 0) {
        BEGET_LOGE("[control_fd] Failed fork service");
    }
    DestroyCmdFifo(reader, writer, readPath, writePath);
    return;
}

static int SendMessage(LoopHandle loop, TaskHandle task, const char *message)
{
    if (message == NULL) {
        BEGET_LOGE("[control_fd] Invalid parameter");
        return -1;
    }
    BufferHandle handle = NULL;
    uint32_t bufferSize = strlen(message) + 1;
    handle = LE_CreateBuffer(loop, bufferSize);
    char *buff = (char *)LE_GetBufferInfo(handle, NULL, &bufferSize);
    BEGET_ERROR_CHECK(buff != NULL, return -1, "[control_fd] Failed get buffer info");
    int ret = memcpy_s(buff, bufferSize, message, strlen(message) + 1);
    BEGET_ERROR_CHECK(ret == 0, LE_FreeBuffer(LE_GetDefaultLoop(), task, handle);
        return -1, "[control_fd] Failed memcpy_s err=%d", errno);
    LE_STATUS status = LE_Send(loop, task, handle, strlen(message) + 1);
    BEGET_ERROR_CHECK(status == LE_SUCCESS, return -1, "[control_fd] Failed le send msg");
    return 0;
}

static int CmdOnIncommingConntect(const LoopHandle loop, const TaskHandle server)
{
    BEGET_LOGI("[control_fd] CmdOnIncommingConntect");
    TaskHandle client = NULL;
    LE_StreamInfo info = {};
    info.baseInfo.flags = TASK_STREAM | TASK_PIPE | TASK_CONNECT;
    info.baseInfo.close = CmdOnClose;
    info.baseInfo.userDataSize = sizeof(CmdAgent);
    info.disConntectComplete = CmdDisConnectComplete;
    info.sendMessageComplete = CmdOnSendMessageComplete;
    info.recvMessage = CmdOnRecvMessage;
    int ret = LE_AcceptStreamClient(LE_GetDefaultLoop(), server, &client, &info);
    BEGET_ERROR_CHECK(ret == 0, return -1, "[control_fd] Failed accept stream")
    CmdAgent *agent = (CmdAgent *)LE_GetUserData(client);
    BEGET_ERROR_CHECK(agent != NULL, return -1, "[control_fd] Invalid agent");
    agent->task = client;
    ret = SendMessage(LE_GetDefaultLoop(), agent->task, "connect success.");
    BEGET_ERROR_CHECK(ret == 0, return -1, "[control_fd] Failed send msg");
    return 0;
}

void CmdServiceInit(const char *socketPath, CallbackControlFdProcess func)
{
    if ((socketPath == NULL) || (func == NULL)) {
        BEGET_LOGE("[control_fd] Invalid parameter");
        return;
    }
    LE_StreamServerInfo info = {};
    info.baseInfo.flags = TASK_STREAM | TASK_SERVER | TASK_PIPE;
    info.server = (char *)socketPath;
    info.socketId = -1;
    info.baseInfo.close = NULL;
    info.disConntectComplete = NULL;
    info.incommingConntect = CmdOnIncommingConntect;
    info.sendMessageComplete = NULL;
    info.recvMessage = NULL;
    g_controlFdFunc = func;
    (void)LE_CreateStreamServer(LE_GetDefaultLoop(), &g_cmdService.serverTask, &info);
}
