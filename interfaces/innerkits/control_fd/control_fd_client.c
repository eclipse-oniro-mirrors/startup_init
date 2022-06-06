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
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#include "beget_ext.h"
#include "control_fd.h"
#include "init_utils.h"
#include "securec.h"

static char g_FifoReadPath[FIFO_PATH_SIZE] = {0};
static int g_FifoReadFd = -1;

static char g_FifoWritePath[FIFO_PATH_SIZE] = {0};
static int g_FifoWriteFd = -1;

static void ProcessFifoWrite(const WatcherHandle taskHandle, int fd, uint32_t *events, const void *context)
{
    if ((fd < 0) || (events == NULL) || (context == NULL)) {
        BEGET_LOGE("[control_fd] Invaild fifo write parameter");
        return;
    }
    int fifow = *((int *)context);
    if (fifow < 0) {
        BEGET_LOGE("[control_fd] invaild fifo write fd");
        return;
    }
    char rbuf[FIFO_BUF_SIZE] = {0};
    int rlen = read(fd, rbuf, FIFO_BUF_SIZE - 1);
    int ret = fflush(stdin);
    BEGET_ERROR_CHECK(ret == 0, return, "[control_fd] Failed fflush err=%d", errno);
    if (rlen > 0) {
        int wlen = write(fifow, rbuf, rlen);
        BEGET_ERROR_CHECK(wlen == rlen, return, "[control_fd] Failed write fifo err=%d", errno);
    }
    printf("#");
    ret = fflush(stdout);
    BEGET_ERROR_CHECK(ret == 0, return, "[control_fd] Failed fflush err=%d", errno);
    *events = Event_Read;
}

static void ProcessFifoRead(const WatcherHandle taskHandle, int fd, uint32_t *events, const void *context)
{
    if ((fd < 0) || (events == NULL)) {
        BEGET_LOGE("[control_fd] Invaild fifo read parameter");
        return;
    }
    char buf[FIFO_BUF_SIZE] = {0};
    int readlen = read(fd, buf, FIFO_BUF_SIZE - 1);
    if (readlen > 0) {
        fprintf(stdout, "%s", buf);
    } else {
        DestroyCmdFifo(g_FifoReadFd, g_FifoWriteFd, g_FifoReadPath, g_FifoWritePath);
        return;
    }
    int ret = fflush(stdout);
    BEGET_ERROR_CHECK(ret == 0, return, "[control_fd] Failed fflush err=%d", errno);
    printf("#");
    ret = fflush(stdout);
    BEGET_ERROR_CHECK(ret == 0, return, "[control_fd] Failed fflush err=%d", errno);
    *events = Event_Read;
}

void DestroyCmdFifo(int rfd, int wfd, const char *readPath, const char *writePath)
{
    if (rfd >= 0) {
        (void)close(rfd);
    }
    if (wfd >= 0) {
        (void)close(wfd);
    }

    if (readPath != NULL) {
        BEGET_CHECK_ONLY_ELOG(unlink(readPath) == 0, "Failed unlink fifo %s", readPath);
    }
    if (writePath != NULL) {
        BEGET_CHECK_ONLY_ELOG(unlink(writePath) == 0, "Failed unlink fifo %s", writePath);
    }
}

static void CmdOnRecvMessage(const TaskHandle task, const uint8_t *buffer, uint32_t buffLen)
{
    BEGET_LOGI("[control_fd] CmdOnRecvMessage %s len %d.", (char *)buffer, buffLen);
}

static void CmdOnConntectComplete(const TaskHandle client)
{
    BEGET_LOGI("[control_fd] CmdOnConntectComplete");
}

static void CmdOnClose(const TaskHandle task)
{
    BEGET_LOGI("[control_fd] CmdOnClose");
    DestroyCmdFifo(g_FifoReadFd, g_FifoWriteFd, g_FifoReadPath, g_FifoWritePath);
}

static void CmdDisConnectComplete(const TaskHandle client)
{
    BEGET_LOGI("[control_fd] CmdDisConnectComplete");
}

static void CmdAgentInit(WatcherHandle handle, const char *path, bool read, ProcessWatchEvent func)
{
    if (path == NULL) {
        BEGET_LOGE("[control_fd] Invaild parameter");
        return;
    }
    BEGET_LOGI("[control_fd] client open %s", (read ? "read" : "write"));
    char *realPath = GetRealPath(path);
    if (realPath == NULL) {
        BEGET_LOGE("[control_fd] Failed get real path %s", path);
        return;
    }
    if (read == true) {
        g_FifoReadFd = open(realPath, O_RDONLY | O_TRUNC | O_NONBLOCK);
        BEGET_ERROR_CHECK(g_FifoReadFd >= 0, free(realPath); return, "[control_fd] Failed to open fifo read");
        BEGET_LOGI("[control_fd] g_FifoReadFd is %d", g_FifoReadFd);
    } else {
        g_FifoWriteFd = open(realPath, O_WRONLY | O_TRUNC);
        BEGET_ERROR_CHECK(g_FifoWriteFd >= 0, free(realPath); return, "[control_fd] Failed to open fifo write");
        BEGET_LOGI("[control_fd] g_FifoWriteFd is %d", g_FifoWriteFd);
    }
    free(realPath);
    // start watcher for stdin
    LE_WatchInfo info = {};
    info.flags = 0;
    info.events = Event_Read;
    info.processEvent = func;
    if (read == true) {
        info.fd = g_FifoReadFd; // read fifo0
        BEGET_ERROR_CHECK(LE_StartWatcher(LE_GetDefaultLoop(), &handle, &info, NULL) == LE_SUCCESS,
            return, "[control_fd] Failed le_loop start watcher fifo read");
    } else {
        info.fd = STDIN_FILENO; // read stdin and write fifo1
        BEGET_ERROR_CHECK(LE_StartWatcher(LE_GetDefaultLoop(), &handle, &info, &g_FifoWriteFd) == LE_SUCCESS,
            return, "[control_fd] Failed le_loop start watcher stdin");
    }
    return;
}

static void CmdOnSendMessageComplete(const TaskHandle task, const BufferHandle handle)
{
    BEGET_LOGI("[control_fd] CmdOnSendMessageComplete");
    CmdAgent *agent = (CmdAgent *)LE_GetUserData(task);
    BEGET_ERROR_CHECK(agent != NULL, return, "[control_fd] Invalid agent");
    CmdAgentInit(agent->input, g_FifoWritePath, false, ProcessFifoWrite);
    printf("#");
    int ret = fflush(stdout);
    BEGET_ERROR_CHECK(ret == 0, return, "[control_fd] Failed fflush err=%d", errno);
}

static CmdAgent *CmdAgentCreate(const char *server)
{
    if (server == NULL) {
        BEGET_LOGE("[control_fd] Invaild parameter");
        return NULL;
    }
    TaskHandle task = NULL;
    LE_StreamInfo info = {};
    info.baseInfo.flags = TASK_STREAM | TASK_PIPE | TASK_CONNECT;
    info.server = (char *)server;
    info.baseInfo.userDataSize = sizeof(CmdAgent);
    info.baseInfo.close = CmdOnClose;
    info.disConntectComplete = CmdDisConnectComplete;
    info.connectComplete = CmdOnConntectComplete;
    info.sendMessageComplete = CmdOnSendMessageComplete;
    info.recvMessage = CmdOnRecvMessage;
    LE_STATUS status = LE_CreateStreamClient(LE_GetDefaultLoop(), &task, &info);
    BEGET_ERROR_CHECK(status == 0, return NULL, "[control_fd] Failed create client");
    CmdAgent *agent = (CmdAgent *)LE_GetUserData(task);
    BEGET_ERROR_CHECK(agent != NULL, return NULL, "[control_fd] Invalid agent");
    agent->task = task;
    return agent;
}

static int CreateFifo(const char *pipeName)
{
    if (pipeName == NULL) {
        BEGET_LOGE("[control_fd] Invaild parameter");
        return -1;
    }
    // create fifo for cmd
    CheckAndCreateDir(pipeName);
    int ret = mkfifo(pipeName, CONTROL_FD_FIFO_MODE);
    if (ret != 0) {
        if (errno != EEXIST) {
            return -1;
        }
    }
    return 0;
}

static int SendCmdMessage(const CmdAgent *agent, uint16_t type, const char *cmd, const char *fifoName)
{
    if ((agent == NULL) || (cmd == NULL) || (fifoName == NULL)) {
        BEGET_LOGE("[control_fd] Invaild parameter");
        return -1;
    }
    int ret = 0;
    BufferHandle handle = NULL;
    uint32_t bufferSize = sizeof(CmdMessage) + strlen(cmd) + FIFO_PATH_SIZE + 1;
    handle = LE_CreateBuffer(LE_GetDefaultLoop(), bufferSize);
    char *buff = (char *)LE_GetBufferInfo(handle, NULL, NULL);
    BEGET_ERROR_CHECK(buff != NULL, return -1, "[control_fd] Failed get buffer info");
    CmdMessage *message = (CmdMessage *)buff;
    message->msgSize = bufferSize;
    message->type = type;
    message->pid = getpid();
    ret = strcpy_s(message->fifoName, FIFO_PATH_SIZE - 1, fifoName);
    BEGET_ERROR_CHECK(ret == 0, LE_FreeBuffer(LE_GetDefaultLoop(), agent->task, handle);
        return -1, "[control_fd] Failed to copy fifo name %s", fifoName);
    ret = strcpy_s(message->cmd, bufferSize - sizeof(CmdMessage) - FIFO_PATH_SIZE, cmd);
    BEGET_ERROR_CHECK(ret == 0, LE_FreeBuffer(LE_GetDefaultLoop(), agent->task, handle);
        return -1, "[control_fd] Failed to copy cmd %s", cmd);
    ret = LE_Send(LE_GetDefaultLoop(), agent->task, handle, bufferSize);
    BEGET_ERROR_CHECK(ret == 0, return -1, "[control_fd] Failed LE_Send msg type %d, pid %d, cmd %s",
        message->type, message->pid, message->cmd);
    return 0;
}

static int CmdMakeFifoInit(const char *fifoPath)
{
    if (fifoPath == NULL) {
        BEGET_LOGE("[control_fd] Invaild parameter");
        return -1;
    }
    int ret = sprintf_s(g_FifoReadPath, sizeof(g_FifoReadPath) - 1, "/dev/fifo/%s0.%d", fifoPath, getpid());
    BEGET_ERROR_CHECK(ret > 0, return -1, "[control_fd] Failed sprintf_s err=%d", errno);
    ret = CreateFifo(g_FifoReadPath);
    BEGET_ERROR_CHECK(ret == 0, return -1, "[control_fd] Failed create fifo err=%d", errno);
    
    ret = sprintf_s(g_FifoWritePath, sizeof(g_FifoWritePath) - 1, "/dev/fifo/%s1.%d", fifoPath, getpid());
    BEGET_ERROR_CHECK(ret > 0, return -1, "[control_fd] Failed sprintf_s err=%d", errno);
    ret = CreateFifo(g_FifoWritePath);
    BEGET_ERROR_CHECK(ret == 0, return -1, "[control_fd] Failed create fifo err=%d", errno);
    return 0;
}

void CmdClientInit(const char *socketPath, uint16_t type, const char *cmd, const char *fifoName)
{
    if ((socketPath == NULL) || (cmd == NULL)) {
        BEGET_LOGE("[control_fd] Invaild parameter");
    }
    BEGET_LOGI("[control_fd] CmdAgentInit");
    int ret = CmdMakeFifoInit(fifoName);
    BEGET_ERROR_CHECK(ret == 0, return, "[control_fd] Failed init fifo");
    CmdAgent *agent = CmdAgentCreate(socketPath);
    BEGET_ERROR_CHECK(agent != NULL, return, "[control_fd] Failed to create agent");
    CmdAgentInit(agent->reader, g_FifoReadPath, true, ProcessFifoRead);
    ret = SendCmdMessage(agent, type, cmd, fifoName);
    BEGET_ERROR_CHECK(ret == 0, return, "[control_fd] Failed send message");
    LE_RunLoop(LE_GetDefaultLoop());
    BEGET_LOGI("Cmd Client exit ");
    LE_CloseStreamTask(LE_GetDefaultLoop(), agent->task);
    free(agent);
    DestroyCmdFifo(g_FifoReadFd, g_FifoWriteFd, g_FifoReadPath, g_FifoWritePath);
}
