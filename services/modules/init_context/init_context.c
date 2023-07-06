/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include "init_context.h"

#include <poll.h>
#ifdef WITH_SELINUX
#include <policycoreutils.h>
#include <selinux/selinux.h>
#endif
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "init_module_engine.h"
#include "plugin_adapter.h"
#include "init_cmds.h"
#include "init_utils.h"
#include "securec.h"

#ifdef STARTUP_INIT_TEST
#define TIMEOUT_DEF  2
#else
#define TIMEOUT_DEF  5
#endif

static SubInitInfo g_subInitInfo[INIT_CONTEXT_MAIN] = {};
static const char *g_subContext[INIT_CONTEXT_MAIN] = {
    "u:r:chipset_init:s0"
};

static void SubInitMain(InitContextType type, int readFd, int writeFd);
static void HandleRecvMessage(SubInitInfo *subInfo, char *buffer, uint32_t size);
static int CreateSocketPair(int socket[2]);
static int SubInitSetSelinuxContext(InitContextType type);

static int SubInitRun(const SubInitForkArg *arg)
{
    PLUGIN_LOGW("SubInitRun %d ", arg->type);
    SubInitSetSelinuxContext(arg->type);
#ifndef STARTUP_INIT_TEST
    close(arg->socket[0]);
#endif
    SubInitMain(arg->type, arg->socket[1], arg->socket[1]);
    close(arg->socket[1]);
#ifndef STARTUP_INIT_TEST
    _exit(PROCESS_EXIT_CODE);
#else
    return 0;
#endif
}

#ifndef STARTUP_INIT_TEST
pid_t SubInitFork(int (*childFunc)(const SubInitForkArg *arg), const SubInitForkArg *args)
{
    pid_t pid = fork();
    if (pid == 0) {
        childFunc(args);
    }
    return pid;
}
#endif

static int SubInitStart(InitContextType type)
{
    PLUGIN_CHECK(type < INIT_CONTEXT_MAIN, return -1, "Invalid type %d", type);
    SubInitInfo *subInfo = &g_subInitInfo[type];
    if (subInfo->state != SUB_INIT_STATE_IDLE) {
        return 0;
    }
    SubInitForkArg arg = { 0 };
    arg.type = type;
    int ret = CreateSocketPair(arg.socket);
    PLUGIN_CHECK(ret == 0, return -1, "Failed to create socket for %d", type);

    subInfo->state = SUB_INIT_STATE_STARTING;
    pid_t pid = SubInitFork(SubInitRun, &arg);
    if (pid < 0) {
        close(arg.socket[0]);
        close(arg.socket[1]);
        subInfo->state = SUB_INIT_STATE_IDLE;
        return -1;
    }
#ifndef STARTUP_INIT_TEST
    close(arg.socket[1]);
#endif
    subInfo->sendFd = arg.socket[0];
    subInfo->recvFd = arg.socket[0];
    subInfo->state = SUB_INIT_STATE_RUNNING;
    subInfo->subPid = pid;
    return 0;
}

static void SubInitStop(pid_t pid)
{
    for (size_t i = 0; i < ARRAY_LENGTH(g_subInitInfo); i++) {
        if (g_subInitInfo[i].subPid == pid) {
            close(g_subInitInfo[i].sendFd);
            g_subInitInfo[i].subPid = 0;
            g_subInitInfo[i].state = SUB_INIT_STATE_IDLE;
        }
    }
}

static int SubInitExecuteCmd(InitContextType type, const char *name, const char *cmdContent)
{
    static char buffer[MAX_CMD_LEN] = {0};
    PLUGIN_CHECK(type < INIT_CONTEXT_MAIN, return -1, "Invalid type %d", type);
    PLUGIN_CHECK(name != NULL, return -1, "Invalid cmd name");
    SubInitInfo *subInfo = &g_subInitInfo[type];
    PLUGIN_CHECK(subInfo->state == SUB_INIT_STATE_RUNNING, return -1, "Sub init %d is not running ", type);

    int len = 0;
    if (cmdContent != NULL) {
        len = snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, "%s %s", name, cmdContent);
    } else {
        len = snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, "%s ", name);
    }
    PLUGIN_CHECK(len > 0, return -1, "Failed to format cmd %s", name);
    buffer[len] = '\0';
    PLUGIN_LOGV("send cmd '%s'", buffer);
    int ret = send(subInfo->sendFd, buffer, len, 0);
    PLUGIN_CHECK(ret > 0, return errno, "Failed to send cmd %s to %d errno %d", name, subInfo->type, errno);

    // block and wait result
    ssize_t rLen = TEMP_FAILURE_RETRY(read(subInfo->recvFd, buffer, sizeof(buffer)));
    while ((rLen < 0) && (errno == EAGAIN)) {
        rLen = TEMP_FAILURE_RETRY(read(subInfo->recvFd, buffer, sizeof(buffer)));
    }
    PLUGIN_CHECK(rLen >= 0 && rLen < sizeof(buffer), return errno,
        "Failed to read result from %d for cmd %s errno %d", subInfo->type, name, errno);
    // change to result
    buffer[rLen] = '\0';
    PLUGIN_LOGV("recv cmd result %s", buffer);
    return atoi(buffer);
}

static int CreateSocketPair(int socket[2])
{
    int ret = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, socket);
    PLUGIN_CHECK(ret == 0, return -1, "Create socket fail errno %d", errno);

    int opt = 1;
    struct timeval timeout = {TIMEOUT_DEF, 0};
    do {
        ret = setsockopt(socket[0], SOL_SOCKET, SO_PASSCRED, &opt, sizeof(opt));
        PLUGIN_CHECK(ret == 0, break, "Failed to set opt for %d errno %d", socket[0], errno);
        ret = setsockopt(socket[1], SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        PLUGIN_CHECK(ret == 0, break, "Failed to set opt for %d errno %d", socket[1], errno);
        ret = setsockopt(socket[0], SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        PLUGIN_CHECK(ret == 0, break, "Failed to set opt for %d errno %d", socket[0], errno);
    } while (0);
    if (ret != 0) {
        close(socket[0]);
        close(socket[1]);
    }
    return ret;
}

static int CheckSocketPermission(const SubInitInfo *subInfo)
{
    struct ucred uc = {-1, -1, -1};
    socklen_t len = sizeof(uc);
    // Only root is permitted to use control fd of init.
    if (getsockopt(subInfo->recvFd, SOL_SOCKET, SO_PEERCRED, &uc, &len) < 0 || uc.uid != 0) {
        INIT_LOGE("Failed to get socket option. err = %d", errno);
        errno = EPERM;
        return -1;
    }
    return 0;
}

static int HandleRecvMessage_(SubInitInfo *subInfo, char *buffer, uint32_t size)
{
    if (CheckSocketPermission(subInfo) != 0) {
        return -1;
    }
    ssize_t rLen = TEMP_FAILURE_RETRY(read(subInfo->recvFd, buffer, size));
    while ((rLen < 0) && (errno == EAGAIN)) {
        rLen = TEMP_FAILURE_RETRY(read(subInfo->recvFd, buffer, size));
    }
    PLUGIN_CHECK(rLen >= 0, return errno, "Read message for %d fail errno %d", subInfo->type, errno);
    buffer[rLen] = '\0';
    PLUGIN_LOGI("Exec cmd '%s' in sub init %s", buffer, g_subContext[subInfo->type]);
    int index = 0;
    const char *cmd = GetMatchCmd(buffer, &index);
    PLUGIN_CHECK(cmd != NULL, return -1, "Can not find cmd %s", buffer);
    DoCmdByIndex(index, buffer + strlen(cmd) + 1, NULL);
    return 0;
}

static void HandleRecvMessage(SubInitInfo *subInfo, char *buffer, uint32_t size)
{
    int ret = HandleRecvMessage_(subInfo, buffer, size);
    int len = snprintf_s(buffer, size, size - 1, "%d", ret);
    PLUGIN_CHECK(len > 0, return, "Failed to format result %d", ret);
    buffer[len] = '\0';
    ret = send(subInfo->sendFd, buffer, len, 0);
    PLUGIN_CHECK(ret > 0, return, "Failed to send result to %d errno %d", subInfo->type, errno);
}

static void SubInitMain(InitContextType type, int readFd, int writeFd)
{
    PLUGIN_LOGI("SubInitMain, sub init %s[%d] enter", g_subContext[type], getpid());
#ifndef STARTUP_INIT_TEST
    (void)prctl(PR_SET_NAME, "chipset_init");
    const int timeout = 30000; // 30000 30s
#else
    const int timeout = 1000; // 1000 1s
#endif
    char buffer[MAX_CMD_LEN] = {0};
    struct pollfd pfd = {};
    pfd.events = POLLIN;
    pfd.fd = readFd;
    SubInitInfo subInfo = {};
    subInfo.type = type;
    subInfo.recvFd = readFd;
    subInfo.sendFd = writeFd;
    while (1) {
        pfd.revents = 0;
        int ret = poll(&pfd, 1, timeout);
        if (ret == 0) {
            PLUGIN_LOGI("Poll sub init timeout, sub init %d exit", type);
            return;
        } else if (ret < 0) {
            PLUGIN_LOGE("Failed to poll sub init socket!");
            return;
        }
        if (pfd.revents & POLLIN) {
            HandleRecvMessage(&subInfo, buffer, sizeof(buffer));
        }
    }
}

static int SubInitSetSelinuxContext(InitContextType type)
{
    PLUGIN_CHECK(type < INIT_CONTEXT_MAIN, return -1, "Invalid type %d", type);
#ifdef INIT_SUPPORT_CHIPSET_INIT
#ifdef WITH_SELINUX
    setcon(g_subContext[type]);
#endif
#endif
    return 0;
}

#ifdef STARTUP_INIT_TEST
SubInitInfo *GetSubInitInfo(InitContextType type)
{
    PLUGIN_CHECK(type < INIT_CONTEXT_MAIN, return NULL, "Invalid type %d", type);
    return &g_subInitInfo[type];
}
#endif

MODULE_CONSTRUCTOR(void)
{
    for (size_t i = 0; i < ARRAY_LENGTH(g_subContext); i++) {
        SubInitContext context = {
            (InitContextType)i,
            SubInitStart,
            SubInitStop,
            SubInitExecuteCmd,
            SubInitSetSelinuxContext
        };
        InitSubInitContext((InitContextType)i, &context);
    }
}
