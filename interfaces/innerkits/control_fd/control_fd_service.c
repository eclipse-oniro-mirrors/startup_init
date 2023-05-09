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
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "beget_ext.h"
#include "control_fd.h"
#include "init_utils.h"
#include "securec.h"

static CmdService g_cmdService;

CallbackControlFdProcess g_controlFdFunc = NULL;

static void OnClose(const TaskHandle task)
{
    CmdTask *agent = (CmdTask *)LE_GetUserData(task);
    BEGET_ERROR_CHECK(agent != NULL, return, "[control_fd] Can not get agent");
    OH_ListRemove(&agent->item);
    OH_ListInit(&agent->item);
}

CONTROL_FD_STATIC int CheckSocketPermission(const TaskHandle task)
{
    struct ucred uc = {-1, -1, -1};
    socklen_t len = sizeof(uc);
    if (getsockopt(LE_GetSocketFd(task), SOL_SOCKET, SO_PEERCRED, &uc, &len) < 0) {
        BEGET_LOGE("Failed to get socket option. err = %d", errno);
        return -1;
    }
    // Only root is permitted to use control fd of init.
    if (uc.uid != 0) { // non-root user
        errno = EPERM;
        return -1;
    }

    return 0;
}

CONTROL_FD_STATIC void CmdOnRecvMessage(const TaskHandle task, const uint8_t *buffer, uint32_t buffLen)
{
    if (buffer == NULL) {
        return;
    }
    CmdTask *agent = (CmdTask *)LE_GetUserData(task);
    BEGET_ERROR_CHECK(agent != NULL, return, "[control_fd] Can not get agent");

    // parse msg to exec
    CmdMessage *msg = (CmdMessage *)buffer;
    if ((msg->type >= ACTION_MAX) || (msg->cmd[0] == '\0') || (msg->ptyName[0] == '\0')) {
        BEGET_LOGE("[control_fd] Received msg is invaild");
        return;
    }

    if (CheckSocketPermission(task) < 0) {
        BEGET_LOGE("Check socket permission failed, err = %d", errno);
        return;
    }
#ifndef STARTUP_INIT_TEST
    agent->pid = fork();
    if (agent->pid == 0) {
        OpenConsole();
        char *realPath = GetRealPath(msg->ptyName);
        BEGET_ERROR_CHECK(realPath != NULL, return, "Failed get realpath, err=%d", errno);
        int n = strncmp(realPath, "/dev/pts/", strlen("/dev/pts/"));
        BEGET_ERROR_CHECK(n == 0, free(realPath); _exit(1), "pts path %s is invaild", realPath);
        int fd = open(realPath, O_RDWR);
        free(realPath);
        BEGET_ERROR_CHECK(fd >= 0, return, "Failed open %s, err=%d", msg->ptyName, errno);
        (void)dup2(fd, STDIN_FILENO);
        (void)dup2(fd, STDOUT_FILENO);
        (void)dup2(fd, STDERR_FILENO); // Redirect fd to 0, 1, 2
        (void)close(fd);
        if (g_controlFdFunc != NULL) {
            g_controlFdFunc(msg->type, msg->cmd, NULL);
        }
        exit(0);
    } else if (agent->pid < 0) {
        BEGET_LOGE("[control_fd] Failed to fork child process, err = %d", errno);
    }
#endif
    return;
}

CONTROL_FD_STATIC int SendMessage(LoopHandle loop, TaskHandle task, const char *message)
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

CONTROL_FD_STATIC int CmdOnIncommingConnect(const LoopHandle loop, const TaskHandle server)
{
    TaskHandle client = NULL;
    LE_StreamInfo info = {};
#ifndef STARTUP_INIT_TEST
    info.baseInfo.flags = TASK_STREAM | TASK_PIPE | TASK_CONNECT;
#else
    info.baseInfo.flags = TASK_STREAM | TASK_PIPE | TASK_CONNECT | TASK_TEST;
#endif
    info.baseInfo.close = OnClose;
    info.baseInfo.userDataSize = sizeof(CmdTask);
    info.disConnectComplete = NULL;
    info.sendMessageComplete = NULL;
    info.recvMessage = CmdOnRecvMessage;
    int ret = LE_AcceptStreamClient(LE_GetDefaultLoop(), server, &client, &info);
    BEGET_ERROR_CHECK(ret == 0, return -1, "[control_fd] Failed accept stream")
    CmdTask *agent = (CmdTask *)LE_GetUserData(client);
    BEGET_ERROR_CHECK(agent != NULL, return -1, "[control_fd] Invalid agent");
    agent->task = client;
    OH_ListInit(&agent->item);
    ret = SendMessage(LE_GetDefaultLoop(), agent->task, "connect success.");
    BEGET_ERROR_CHECK(ret == 0, return -1, "[control_fd] Failed send msg");
    OH_ListAddTail(&g_cmdService.head, &agent->item);
    return 0;
}

void CmdServiceInit(const char *socketPath, CallbackControlFdProcess func)
{
    if ((socketPath == NULL) || (func == NULL)) {
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
    g_controlFdFunc = func;
    (void)LE_CreateStreamServer(LE_GetDefaultLoop(), &g_cmdService.serverTask, &info);
}

static int ClientTraversalProc(ListNode *node, void *data)
{
    CmdTask *info = ListEntry(node, CmdTask, item);
    int pid = *(int *)data;
    return pid - info->pid;
}

void CmdServiceProcessDelClient(pid_t pid)
{
    ListNode *node = OH_ListFind(&g_cmdService.head, (void *)&pid, ClientTraversalProc);
    if (node != NULL) {
        CmdTask *agent = ListEntry(node, CmdTask, item);
        OH_ListRemove(&agent->item);
        OH_ListInit(&agent->item);
        LE_CloseTask(LE_GetDefaultLoop(), agent->task);
    }
}
