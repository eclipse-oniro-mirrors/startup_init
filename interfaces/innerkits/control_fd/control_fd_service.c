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
#include <limits.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <unistd.h>

#include "beget_ext.h"
#include "control_fd.h"
#include "init_utils.h"
#include "securec.h"

static CmdService g_cmdService;

CallbackControlFdProcess g_controlFdFunc = NULL;

static int ReadFifoPath(const char *name, bool read, pid_t pid, char *resolvedPath)
{
    if ((name == NULL) || (pid < 0) || resolvedPath == NULL) {
        return -1;
    }
    int flag = read ? 1 : 0;
    char path[PATH_MAX] = {0};
    int ret = sprintf_s(path, sizeof(path) - 1, "/dev/fifo/%s%d.%d", name, flag, pid);
    BEGET_ERROR_CHECK(ret > 0, return -1, "[control_fd] Failed sprintf_s err=%d", errno);
    char *realPath = realpath(path, resolvedPath);
    BEGET_ERROR_CHECK(realPath != NULL, return -1, "[control_fd] Failed get real path %s, err=%d", path, errno);
    int flags = read ? (O_RDONLY | O_TRUNC | O_NONBLOCK) : (O_WRONLY | O_TRUNC);
    int fd = open(resolvedPath, flags);
    return fd;
}

static void CmdOnRecvMessage(const TaskHandle task, const uint8_t *buffer, uint32_t buffLen)
{
    if (buffer == NULL) {
        return;
    }
    // parse msg to exec
    CmdMessage *msg = (CmdMessage *)buffer;
    if ((msg->type < 0) || (msg->type >= ACTION_MAX) || (msg->cmd[0] == '\0') || (msg->fifoName[0] == '\0')) {
        BEGET_LOGE("[control_fd] Failed msg ");
        return;
    }
    char readPath[PATH_MAX] = {0};
    int reader = ReadFifoPath(msg->fifoName, true, msg->pid, readPath); // read
    BEGET_ERROR_CHECK(reader > 0, return, "[control_fd] Failed to open fifo read, err=%d", errno);

    char writePath[PATH_MAX] = {0};
    int writer = ReadFifoPath(msg->fifoName, false, msg->pid, writePath); // write
    BEGET_ERROR_CHECK(writer > 0, return, "[control_fd] Failed to open fifo write, err=%d", errno);

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
    TaskHandle client = NULL;
    LE_StreamInfo info = {};
    info.baseInfo.flags = TASK_STREAM | TASK_PIPE | TASK_CONNECT;
    info.baseInfo.close = NULL;
    info.baseInfo.userDataSize = sizeof(CmdAgent);
    info.disConntectComplete = NULL;
    info.sendMessageComplete = NULL;
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
