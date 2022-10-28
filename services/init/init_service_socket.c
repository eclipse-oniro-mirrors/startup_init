/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include "init_service_socket.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include "init_cmdexecutor.h"
#include "init_log.h"
#include "init_service.h"
#include "loop_event.h"
#include "securec.h"
#define SOCKET_BUFF_SIZE (256 * 1024)

#define HOS_SOCKET_DIR "/dev/unix/socket"
#define HOS_SOCKET_ENV_PREFIX "OHOS_SOCKET_"
#define MAX_SOCKET_ENV_PREFIX_LEN 64
#define MAX_SOCKET_FD_LEN 16

static int GetSocketAddr(struct sockaddr_un *addr, const char *name)
{
    (void)memset_s(addr, sizeof(struct sockaddr_un), 0x0, sizeof(struct sockaddr_un));
    addr->sun_family = AF_UNIX;
    size_t addrLen = sizeof(addr->sun_path);
    int ret = snprintf_s(addr->sun_path, addrLen, addrLen - 1, HOS_SOCKET_DIR "/%s", name);
    INIT_ERROR_CHECK(ret >= 0, return -1, "Failed to format addr %s", name);
    return 0;
}

static int SetSocketAddr(ServiceSocket *sockopt, sockaddr_union *addr)
{
    int ret = 0;
    if (sockopt->family == AF_NETLINK) {
#ifndef __LITEOS_A__
        if (memset_s(&(addr->addrnl), sizeof(addr->addrnl), 0, sizeof(addr->addrnl)) != EOK) {
            INIT_LOGE("Failed to clear socket address");
            return -1;
        }
        addr->addrnl.nl_family = AF_NETLINK;
        addr->addrnl.nl_pid = getpid();
        addr->addrnl.nl_groups = 0xffffffff;
#else
        INIT_LOGE("No support in liteos kernel");
        return -1;
#endif
    } else {
        ret = GetSocketAddr(&(addr->addrun), sockopt->name);
        INIT_ERROR_CHECK(ret == 0, return -1, "Failed to format addr %s", sockopt->name);
        if (access(addr->addrun.sun_path, F_OK) == 0) {
            INIT_LOGI("%s already exist, remove it", addr->addrun.sun_path);
            unlink(addr->addrun.sun_path);
        }
    }
    return ret;
}

static int SetSocketOptionAndBind(ServiceSocket *sockopt)
{
    if (sockopt->option & SOCKET_OPTION_PASSCRED) {
        int on = 1;
        if (setsockopt(sockopt->sockFd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on))) {
            INIT_LOGE("Failed to setsockopt");
            return -1;
        }
    }
    if (sockopt->option & SOCKET_OPTION_RCVBUFFORCE) {
        int buffSize = SOCKET_BUFF_SIZE;
        if (setsockopt(sockopt->sockFd, SOL_SOCKET, SO_RCVBUFFORCE, &buffSize, sizeof(buffSize))) {
            INIT_LOGE("Failed to setsockopt");
            return -1;
        }
    }
    sockaddr_union addr = {};
    if (SetSocketAddr(sockopt, &addr) != 0) {
        INIT_LOGE("Failed to set socket addr");
        return -1;
    }
    if (sockopt->family == AF_NETLINK) {
#ifndef __LITEOS_A__
        if (bind(sockopt->sockFd, (struct sockaddr *)&(addr.addrnl), sizeof(addr.addrnl))) {
            INIT_LOGE("Create socket for service %s failed: %d", sockopt->name, errno);
            return -1;
        }
#else
        INIT_LOGE("No support in liteos kernel");
        return -1;
#endif
    } else {
        if (bind(sockopt->sockFd, (struct sockaddr *)&(addr.addrun), sizeof(addr.addrun))) {
            INIT_LOGE("Create socket for service %s failed: %d", sockopt->name, errno);
            return -1;
        }
        if (lchown(addr.addrun.sun_path, sockopt->uid, sockopt->gid)) {
            INIT_LOGE("lchown fail %d ", errno);
            unlink(addr.addrun.sun_path);
            return -1;
        }
        if (fchmodat(AT_FDCWD, addr.addrun.sun_path, sockopt->perm, AT_SYMLINK_NOFOLLOW)) {
            INIT_LOGE("fchmodat fail %d ", errno);
            unlink(addr.addrun.sun_path);
            return -1;
        }
    }
    return 0;
}

static int CreateSocket(ServiceSocket *sockopt)
{
    INIT_ERROR_CHECK(sockopt != NULL, return SERVICE_FAILURE, "Invalid socket options");
    INIT_LOGV("Socket name: %s, family: %d, type: %u, protocol: %d, perm: %u, uid: %u, gid: %u, option: %u",
        sockopt->name, sockopt->family, sockopt->type, sockopt->protocol,
        sockopt->perm, sockopt->uid,    sockopt->gid,  sockopt->option);
    if (sockopt->sockFd >= 0) {
        close(sockopt->sockFd);
        sockopt->sockFd = -1;
    }
    sockopt->sockFd = socket(sockopt->family, sockopt->type, sockopt->protocol);
    INIT_ERROR_CHECK(sockopt->sockFd >= 0, return -1, "socket fail %d ", errno);

    int ret = SetSocketOptionAndBind(sockopt);
    if (ret != 0) {
        close(sockopt->sockFd);
        return -1;
    }
    INIT_LOGI("CreateSocket %s success", sockopt->name);

    PluginExecCmdByName("restoreContentRecurse", HOS_SOCKET_DIR);
    return sockopt->sockFd;
}

static int SetSocketEnv(int fd, const char *name)
{
    INIT_ERROR_CHECK(name != NULL, return SERVICE_FAILURE, "Invalid name");
    char pubName[MAX_SOCKET_ENV_PREFIX_LEN] = { 0 };
    char val[MAX_SOCKET_FD_LEN] = { 0 };
    INIT_CHECK_RETURN_VALUE(snprintf_s(pubName, sizeof(pubName), sizeof(pubName) - 1, HOS_SOCKET_ENV_PREFIX "%s",
        name) >= 0, -1);
    INIT_CHECK_RETURN_VALUE(snprintf_s(val, sizeof(val), sizeof(val) - 1, "%d", fd) >= 0, -1);
    int ret = setenv(pubName, val, 1);
    INIT_ERROR_CHECK(ret >= 0, return -1, "setenv fail %d ", errno);
    fcntl(fd, F_SETFD, 0);
    return 0;
}

static void ProcessWatchEvent_(const WatcherHandle watcherHandle, int fd, uint32_t *events, const void *context)
{
    *events = 0;
    Service *service = (Service *)context;
    ServiceSocket *tmpSock = service->socketCfg;
    while (tmpSock != NULL) {
        if (tmpSock->sockFd == fd) {
            tmpSock->watcher = NULL;
            break;
        }
        tmpSock = tmpSock->next;
    }
    if (tmpSock == NULL) { // not found socket
        INIT_LOGE("Service %s not match socket fd %d!", service->name, fd);
        close(fd);
        return;
    }
    INIT_LOGI("Socket information detected, fd:%d service name:%s", fd, service->name);
    SocketDelWatcher(watcherHandle);
    if (ServiceStart(service) != SERVICE_SUCCESS) {
        INIT_LOGE("Service %s start failed!", service->name);
    }
}

int SocketAddWatcher(ServiceWatcher *watcherHandle, Service *service, int fd)
{
    WatcherHandle handle;
    LE_WatchInfo info = {};
    info.fd = fd;
    info.flags = WATCHER_ONCE;
    info.events = Event_Read;
    info.processEvent = ProcessWatchEvent_;
    int ret = LE_StartWatcher(LE_GetDefaultLoop(), &handle, &info, service);
    INIT_LOGI("Watcher socket fd %d for service %s", fd, service->name);
    *watcherHandle = (ServiceWatcher)handle;
    return ret;
}

void SocketDelWatcher(ServiceWatcher watcherHandle)
{
    LE_RemoveWatcher(LE_GetDefaultLoop(), (WatcherHandle)watcherHandle);
}

int CreateServiceSocket(Service *service)
{
    INIT_CHECK(service != NULL && service->socketCfg != NULL, return 0);
    INIT_CHECK(service->socketCfg->sockFd == -1, return 0);
    int ret = 0;
    ServiceSocket *tmpSock = service->socketCfg;
    while (tmpSock != NULL) {
        PluginExecCmdByName("setSockCreateCon", service->name);
        int fd = CreateSocket(tmpSock);
        PluginExecCmdByName("setSockCreateCon", NULL);
        INIT_CHECK_RETURN_VALUE(fd >= 0, -1);
        if (IsOnDemandService(service)) {
            if (IsConnectionBasedSocket(tmpSock)) {
                ret = listen(tmpSock->sockFd, MAX_SOCKET_FD_LEN);
                INIT_CHECK_RETURN_VALUE(ret == 0, -1);
            }
            if (strcmp(service->name, "ueventd") != 0) {
                ret = SocketAddWatcher(&tmpSock->watcher, service, tmpSock->sockFd);
                INIT_CHECK_RETURN_VALUE(ret == 0, -1);
            }
        }
        ret = SetSocketEnv(fd, tmpSock->name);
        INIT_CHECK_RETURN_VALUE(ret >= 0, -1);
        tmpSock = tmpSock->next;
    }
    return 0;
}

void CloseServiceSocket(Service *service)
{
    INIT_CHECK(service != NULL && service->socketCfg != NULL, return);
    struct sockaddr_un addr;
    ServiceSocket *sockopt = service->socketCfg;
    while (sockopt != NULL) {
        if (sockopt->watcher != NULL) {
            SocketDelWatcher(sockopt->watcher);
        }
        if (sockopt->sockFd >= 0) {
            close(sockopt->sockFd);
            sockopt->sockFd = -1;
        }
        if (GetSocketAddr(&addr, sockopt->name) == 0) {
            unlink(addr.sun_path);
        }
        sockopt = sockopt->next;
    }
    return;
}
