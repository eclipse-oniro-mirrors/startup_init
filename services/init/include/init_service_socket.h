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
#ifndef INIT_SERVICE_SOCKET_
#define INIT_SERVICE_SOCKET_
#ifndef __LITEOS_A__
#include <linux/netlink.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define MAX_SOCK_NAME_LEN 16
#define SOCK_OPT_NUMS 6

#define IsConnectionBasedSocket(sockopt) \
    ((sockopt)->type == SOCK_STREAM || (sockopt)->type == SOCK_SEQPACKET)

enum SockOptionTab {
    SERVICE_SOCK_NAME = 0,
    SERVICE_SOCK_TYPE,
    SERVICE_SOCK_PERM,
    SERVICE_SOCK_UID,
    SERVICE_SOCK_GID,
    SERVICE_SOCK_SETOPT
};
typedef void *ServiceWatcher;
struct Service_;

// socket option
#define SOCKET_OPTION_INVALID 0x001      // option invalid
#define SOCKET_OPTION_PASSCRED 0x002     // SO_PASSCRED
#define SOCKET_OPTION_RCVBUFFORCE 0x004  // SO_RCVBUFFORCE

typedef union {
#ifndef __LITEOS_A__
    struct sockaddr_nl addrnl;
#endif
    struct sockaddr_un addrun;
} sockaddr_union;

typedef struct ServiceSocket_ {
    struct ServiceSocket_ *next;
    int family;          // socket family
    unsigned int type;   // socket type
    int protocol;        // socket protocol
    mode_t perm;         // Setting permissions
    uid_t uid;           // uid
    gid_t gid;           // gid
    unsigned int option; // setsocketopt
    int sockFd;
    ServiceWatcher watcher;
    char name[0];        // service name
} ServiceSocket;

int CreateServiceSocket(struct Service_ *service);
void CloseServiceSocket(struct Service_ *service);
int SocketAddWatcher(ServiceWatcher *watcherHandle, struct Service_ *service, int fd);
void SocketDelWatcher(ServiceWatcher watcherHandle);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif
