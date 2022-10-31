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
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include "beget_ext.h"
#include "control_fd.h"
#include "securec.h"

CONTROL_FD_STATIC void ProcessPtyWrite(const WatcherHandle taskHandle, int fd, uint32_t *events, const void *context)
{
    if ((fd < 0) || (events == NULL) || (context == NULL)) {
        BEGET_LOGE("[control_fd] Invalid fifo write parameter");
        return;
    }
    CmdAgent *agent = (CmdAgent *)context;
    char rbuf[PTY_BUF_SIZE] = {0};
    ssize_t rlen = read(fd, rbuf, PTY_BUF_SIZE - 1);
    int ret = fflush(stdin);
    BEGET_ERROR_CHECK(ret == 0, return, "[control_fd] Failed fflush err=%d", errno);
    if (rlen > 0) {
        ssize_t wlen = write(agent->ptyFd, rbuf, rlen);
        BEGET_ERROR_CHECK(wlen == rlen, return, "[control_fd] Failed write fifo err=%d", errno);
    }
    ret = fflush(stdout);
    BEGET_ERROR_CHECK(ret == 0, return, "[control_fd] Failed fflush err=%d", errno);
    *events = Event_Read;
}

CONTROL_FD_STATIC void ProcessPtyRead(const WatcherHandle taskHandle, int fd, uint32_t *events, const void *context)
{
    if ((fd < 0) || (events == NULL) || (context == NULL)) {
        BEGET_LOGE("[control_fd] Invalid fifo read parameter");
        return;
    }
    CmdAgent *agent = (CmdAgent *)context;
    char buf[PTY_BUF_SIZE] = {0};
    long readlen = read(fd, buf, PTY_BUF_SIZE - 1);
    if (readlen > 0) {
        fprintf(stdout, "%s", buf);
    } else {
        (void)close(agent->ptyFd);
        return;
    }
    int ret = fflush(stdout);
    BEGET_ERROR_CHECK(ret == 0, return, "[control_fd] Failed fflush err=%d", errno);
    *events = Event_Read;
}

CONTROL_FD_STATIC void CmdClientOnRecvMessage(const TaskHandle task, const uint8_t *buffer, uint32_t buffLen)
{
    BEGET_LOGI("[control_fd] CmdOnRecvMessage %s len %d.", (char *)buffer, buffLen);
}

CONTROL_FD_STATIC void CmdOnConnectComplete(const TaskHandle client)
{
    BEGET_LOGI("[control_fd] CmdOnConnectComplete");
}

CONTROL_FD_STATIC void CmdOnClose(const TaskHandle task)
{
    BEGET_LOGI("[control_fd] CmdOnClose");
    CmdAgent *agent = (CmdAgent *)LE_GetUserData(task);
    BEGET_ERROR_CHECK(agent != NULL, return, "[control_fd] Invalid agent");
    (void)close(agent->ptyFd);
    agent->ptyFd = -1;
    LE_StopLoop(LE_GetDefaultLoop());
}

CONTROL_FD_STATIC void CmdDisConnectComplete(const TaskHandle client)
{
    BEGET_LOGI("[control_fd] CmdDisConnectComplete");
}

CONTROL_FD_STATIC void CmdOnSendMessageComplete(const TaskHandle task, const BufferHandle handle)
{
    BEGET_LOGI("[control_fd] CmdOnSendMessageComplete");
}

CONTROL_FD_STATIC CmdAgent *CmdAgentCreate(const char *server)
{
    if (server == NULL) {
        BEGET_LOGE("[control_fd] Invalid parameter");
        return NULL;
    }
    TaskHandle task = NULL;
    LE_StreamInfo info = {};
    info.baseInfo.flags = TASK_STREAM | TASK_PIPE | TASK_CONNECT;
    info.server = (char *)server;
    info.baseInfo.userDataSize = sizeof(CmdAgent);
    info.baseInfo.close = CmdOnClose;
    info.disConnectComplete = CmdDisConnectComplete;
    info.connectComplete = CmdOnConnectComplete;
    info.sendMessageComplete = CmdOnSendMessageComplete;
    info.recvMessage = CmdClientOnRecvMessage;
    LE_STATUS status = LE_CreateStreamClient(LE_GetDefaultLoop(), &task, &info);
    BEGET_ERROR_CHECK(status == 0, return NULL, "[control_fd] Failed create client");
    CmdAgent *agent = (CmdAgent *)LE_GetUserData(task);
    BEGET_ERROR_CHECK(agent != NULL, return NULL, "[control_fd] Invalid agent");
    agent->task = task;
    return agent;
}

CONTROL_FD_STATIC int SendCmdMessage(const CmdAgent *agent, uint16_t type, const char *cmd, const char *ptyName)
{
    if ((agent == NULL) || (cmd == NULL) || (ptyName == NULL)) {
        BEGET_LOGE("[control_fd] Invalid parameter");
        return -1;
    }
    BufferHandle handle = NULL;
    uint32_t bufferSize = sizeof(CmdMessage) + strlen(cmd) + PTY_PATH_SIZE + 1;
    handle = LE_CreateBuffer(LE_GetDefaultLoop(), bufferSize);
    char *buff = (char *)LE_GetBufferInfo(handle, NULL, NULL);
    BEGET_ERROR_CHECK(buff != NULL, return -1, "[control_fd] Failed get buffer info");
    CmdMessage *message = (CmdMessage *)buff;
    message->msgSize = bufferSize;
    message->type = type;
    int ret = strcpy_s(message->ptyName, PTY_PATH_SIZE - 1, ptyName);
    BEGET_ERROR_CHECK(ret == 0, LE_FreeBuffer(LE_GetDefaultLoop(), agent->task, handle);
        return -1, "[control_fd] Failed to copy pty name %s", ptyName);
    ret = strcpy_s(message->cmd, bufferSize - sizeof(CmdMessage) - PTY_PATH_SIZE, cmd);
    BEGET_ERROR_CHECK(ret == 0, LE_FreeBuffer(LE_GetDefaultLoop(), agent->task, handle);
        return -1, "[control_fd] Failed to copy cmd %s", cmd);
    ret = LE_Send(LE_GetDefaultLoop(), agent->task, handle, bufferSize);
    BEGET_ERROR_CHECK(ret == 0, return -1, "[control_fd] Failed LE_Send msg type %d, cmd %s",
        message->type, message->cmd);
    return 0;
}

CONTROL_FD_STATIC int InitPtyInterface(CmdAgent *agent, uint16_t type, const char *cmd)
{
    if ((cmd == NULL) || (agent == NULL)) {
        return -1;
    }
#ifndef STARTUP_INIT_TEST
    // initialize terminal
    struct termios term;
    int ret = tcgetattr(STDIN_FILENO, &term);
    BEGET_ERROR_CHECK(ret == 0, return -1, "Failed tcgetattr stdin, err=%d", errno);
    cfmakeraw(&term);
    term.c_cc[VTIME] = 0;
    term.c_cc[VMIN] = 1;
    ret = tcsetattr(STDIN_FILENO, TCSANOW, &term);
    BEGET_ERROR_CHECK(ret == 0, return -1, "Failed tcsetattr term, err=%d", errno);
    // open master pty and get slave pty
    int pfd = open("/dev/ptmx", O_RDWR | O_CLOEXEC);
    BEGET_ERROR_CHECK(pfd >= 0, return -1, "Failed open pty err=%d", errno);
    BEGET_ERROR_CHECK(grantpt(pfd) >= 0, close(pfd); return -1, "Failed to call grantpt");
    BEGET_ERROR_CHECK(unlockpt(pfd) >= 0, close(pfd); return -1, "Failed to call unlockpt");
    char ptsbuffer[PTY_PATH_SIZE] = {0};
    ret = ptsname_r(pfd, ptsbuffer, sizeof(ptsbuffer));
    BEGET_ERROR_CHECK(ret >= 0, close(pfd); return -1, "Failed to get pts name err=%d", errno);
    BEGET_LOGI("ptsbuffer is %s", ptsbuffer);
    agent->ptyFd = pfd;

    LE_WatchInfo info = {};
    info.flags = 0;
    info.events = Event_Read;
    info.processEvent = ProcessPtyRead;
    info.fd = pfd; // read ptmx
    BEGET_ERROR_CHECK(LE_StartWatcher(LE_GetDefaultLoop(), &agent->reader, &info, agent) == LE_SUCCESS,
        close(pfd); return -1, "[control_fd] Failed le_loop start watcher ptmx read");
    info.processEvent = ProcessPtyWrite;
    info.fd = STDIN_FILENO; // read stdin and write ptmx
    BEGET_ERROR_CHECK(LE_StartWatcher(LE_GetDefaultLoop(), &agent->input, &info, agent) == LE_SUCCESS,
        close(pfd); return -1, "[control_fd] Failed le_loop start watcher stdin read and write ptmx");
    ret = SendCmdMessage(agent, type, cmd, ptsbuffer);
    BEGET_ERROR_CHECK(ret == 0, close(pfd); return -1, "[control_fd] Failed send message");
#endif
    return 0;
}

void CmdClientInit(const char *socketPath, uint16_t type, const char *cmd)
{
    if ((socketPath == NULL) || (cmd == NULL)) {
        BEGET_LOGE("[control_fd] Invalid parameter");
    }
    BEGET_LOGI("[control_fd] CmdAgentInit");
    CmdAgent *agent = CmdAgentCreate(socketPath);
    BEGET_ERROR_CHECK(agent != NULL, return, "[control_fd] Failed to create agent");
#ifndef STARTUP_INIT_TEST
    int ret = InitPtyInterface(agent, type, cmd);
    if (ret != 0) {
        return;
    }
    LE_RunLoop(LE_GetDefaultLoop());
    LE_CloseLoop(LE_GetDefaultLoop());
#endif
    BEGET_LOGI("Cmd Client exit ");
}
