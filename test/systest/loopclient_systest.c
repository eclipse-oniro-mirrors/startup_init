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

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <linux/in.h>
#include <linux/socket.h>
#include <linux/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>

#include "loop_systest.h"
#include "securec.h"
#include "loop_event.h"
#include "le_socket.h"
#include "le_task.h"
#include "list.h"

#define RETRY_TIME (200 * 1000)     // 200 * 1000 wait 200ms
#define MAX_RETRY_SEND_COUNT 2      // 2 max retry count
#define MAX_MSG_SIZE 128
typedef void *MsgHandle;
typedef void *ClientHandle;

typedef struct {
    uint32_t uid;       // the UNIX uid that the child process setuid() after fork()
    uint32_t gid;       // the UNIX gid that the child process setgid() after fork()
    uint32_t gidCount;  // the size of gidTable
    uint32_t gidTable[APP_MAX_GIDS];
    char userName[APP_USER_NAME];
} DacInfo;

typedef Agent_ {
    TaskHandle task;
    WatcherHandle input;
    WatcherHandle reader;
    int ptyfd;
} Agent;

typedef struct {
    struct ListNode node;
    uint32_t blockSize;     // block 的大小
    uint32_t currentIndex;  // 当前已经填充的位置
    uint8_t buffer[0];
} MsgBlock;

typedef struct {
    uint32_t maxRetryCount;
    uint32_t timeout;
    uint32_t msgNextId;
    int socketId;
    pthread_mutex_t mutex;
    MsgBlock recvBlock;  // 消息接收缓存
} ReqMsgMgr;

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static ReqMsgMgr *g_clientInstance = NULL;

Agent *CreateAgent(const char *server, int flags)
{
    if (server == NULL) {
        printf("Invalid parameter \n");
        return NULL;
    }

    TaskHandle task = NULL;
    LE_StreamInfo info = {};
    info.baseInfo.flags = flags;
    info.server = (char *)server;
    info.baseInfo.userDataSize = sizeof(Agent);
    info.baseInfo.close = OnClose;
    info.disConnectComplete = DisConnectComplete;
    info.connectComplete = OnConnectComplete;
    info.sendMessageComplete = OnSendMessageComplete;
    info.recvMessage = ClientOnRecvMessage;

    LE_STATUS status = LE_CreateStreamClient(LE_GetDefaultLoop(), &task, &info);
    if (status != 0) {
        printf("Failed create client \n");
        return NULL;
    }
    Agent *agent = (Agent *)LE_GetUserData(task);
    if (agent == NULL) {
        printf("Invalid agent \n");
        return NULL;
    }

    agent->task = task;
    return agent;
}

static int InitClientInstance()
{
    pthread_mutex_lock(&g_mutex);
    if (g_clientInstance != NULL) {
        pthread_mutex_unlock(&g_mutex);
        return 0;
    }
    ReqMsgMgr *clientInstance = malloc(sizeof(ReqMsgMgr) + RECV_BLOCK_LEN);
    if (clientInstance == NULL) {
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }
    // init
    clientInstance->msgNextId = 1;
    clientInstance->timeout = GetDefaultTimeout(TIMEOUT_DEF);
    clientInstance->maxRetryCount = MAX_RETRY_SEND_COUNT;
    clientInstance->socketId = -1;
    pthread_mutex_init(&clientInstance->mutex, NULL);
    // init recvBlock
    OH_ListInit(&clientInstance->recvBlock.node);
    clientInstance->recvBlock.blockSize = RECV_BLOCK_LEN;
    clientInstance->recvBlock.currentIndex = 0;
    g_clientInstance = clientInstance;
    pthread_mutex_unlock(&g_mutex);
    return 0;
}

void ClientInit(const char *socketPath, int flags)
{
    if (socketPath == NULL) {
        printf("Invalid parameter \n");
    }
    printf("AgentInit \n");
    Agent *agent = CreateAgent(socketPath, flags);
    if (agent == NULL) {
        printf("Failed to create agent \n");
        return;
    }

    printf(" Client exit \n");
}

int ClientDestroy(ReqMsgMgr *reqMgr)
{
    if (reqMgr == NULL) {
        printf("Invalid reqMgr \n");
        return -1;
    }

    pthread_mutex_lock(&g_mutex);
    pthread_mutex_unlock(&g_mutex);
    pthread_mutex_destroy(&reqMgr->mutex);
    if (reqMgr->socketId >= 0) {
        CloseClientSocket(reqMgr->socketId);
        reqMgr->socketId = -1;
    }
    free(reqMgr);
    return 0;
}

static void CloseClientSocket(int socketId)
{
    printf("Closed socket with fd %d \n", socketId);
    if (socketId >= 0) {
        int flag = 0;
        setsockopt(socketId, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
        close(socketId);
    }
}

static int CreateClientSocket(uint32_t timeout)
{
    const char *socketName = "loopserver";
    int socketFd = socket(AF_UNIX, SOCK_STREAM, 0);  // SOCK_SEQPACKET
    if (socketFd < 0) {
        printf("Socket fd: %d error: %d \n", socketFd, errno);
    }

    int ret = 0;
    do {
        int flag = 1;
        ret = setsockopt(socketFd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
        flag = 1;
        ret = setsockopt(socketFd, SOL_SOCKET, SO_PASSCRED, &flag, sizeof(flag));
        if (ret != 0) {
            printf("Set opt SO_PASSCRED name: %s error: %d \n", socketName, errno);
            break;
        }
        struct timeval timeoutVal = {timeout, 0};
        ret = setsockopt(socketFd, SOL_SOCKET, SO_SNDTIMEO, &timeoutVal, sizeof(timeoutVal));
        if (ret != 0) {
            printf("Set opt SO_SNDTIMEO name: %s error: %d \n", socketName, errno);
            break;
        }
        ret = setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, &timeoutVal, sizeof(timeoutVal));
        if (ret != 0) {
            printf("Set opt SO_RCVTIMEO name: %s error: %d \n", socketName, errno);
            break;
        }
        struct sockaddr_un addr;
        socklen_t pathSize = sizeof(addr.sun_path);
        int pathLen = snprintf_s(addr.sun_path, pathSize, (pathSize - 1), "%s%s", _SOCKET_DIR, socketName);
        if (pathLen <= 0) {
            printf("Format path: %s error: %d \n", socketName, errno);
            break;
        }
        addr.sun_family = AF_LOCAL;
        socklen_t socketAddrLen = offsetof(struct sockaddr_un, sun_path) + pathLen + 1;
        ret = connect(socketFd, (struct sockaddr *)(&addr), socketAddrLen);
        if (ret != 0) {
            printf("Failed to connect %s error: %d \n", addr.sun_path, errno);
            break;
        }
        printf("Create socket success %s socketFd: %d", addr.sun_path, socketFd);
        return socketFd;
    } while (0);
    CloseClientSocket(socketFd);
    return -1;
}

static void TryCreateSocket(ReqMsgMgr *reqMgr)
{
    uint32_t retryCount = 1;
    while (retryCount <= reqMgr->maxRetryCount) {
        if (reqMgr->socketId < 0) {
            reqMgr->socketId = CreateClientSocket(reqMgr->timeout);
        }
        if (reqMgr->socketId < 0) {
            printf("Failed to create socket, try again \n");
            usleep(RETRY_TIME);
            retryCount++;
            continue;
        }
        break;
    }
}

static int WriteMessage(int socketFd, const uint8_t *buf, ssize_t len, int *fds, int *fdCount)
{
    ssize_t written = 0;
    ssize_t remain = len;
    const uint8_t *offset = buf;
    struct iovec iov = {
        .iov_base = (void *) offset,
        .iov_len = len,
    };
    struct msghdr msg = {
        .msg_iov = &iov,
        .msg_iovlen = 1,
    };
    char *ctrlBuffer = NULL;
    if (fdCount != NULL && fds != NULL && *fdCount > 0) {
        msg.msg_controllen = CMSG_SPACE(*fdCount * sizeof(int));
        ctrlBuffer = (char *) malloc(msg.msg_controllen);
        if (ctrlBuffer == NULL) {
            printf("WriteMessage fail to alloc memory for msg_control %d %d", msg.msg_controllen, errno);
            return -1;
        }
        msg.msg_control = ctrlBuffer;
        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        if (cmsg == NULL) {
            free(ctrlBuffer);
            printf("WriteMessage fail to get CMSG_FIRSTHDR %d \n", errno);
            return -1;
        }
        cmsg->cmsg_len = CMSG_LEN(*fdCount * sizeof(int));
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_level = SOL_SOCKET;
        int ret = memcpy_s(CMSG_DATA(cmsg), cmsg->cmsg_len, fds, *fdCount * sizeof(int));
        if (ret != 0) {
            free(ctrlBuffer);
            printf("WriteMessage fail to memcpy_s fd, errno: %d \n", errno);
            return -1;
        }
        printf("build fd info count %d \n", *fdCount);
    }
    for (ssize_t wLen = 0; remain > 0; offset += wLen, remain -= wLen, written += wLen) {
        errno = 0;
        wLen = sendmsg(socketFd, &msg, MSG_NOSIGNAL);
        if ((wLen <= 0) || (errno != EINTR)) {
            free(ctrlBuffer);
            printf("Failed to write message to fd %d, wLen %zd errno: %d \n", socketFd, wLen, errno);
            return -errno;
        }
    }
    free(ctrlBuffer);
    return written == len ? 0 : -EFAULT;
}

static int HandleMsgSend(ReqMsgMgr *reqMgr, int socketId, ReqMsgNode *reqNode)
{
    printf("HandleMsgSend reqId: %u msgId: %d \n", reqNode->reqId, reqNode->msg->msgId);
    ListNode *sendNode = reqNode->msgBlocks.next;
    uint32_t currentIndex = 0;
    bool sendFd = true;
    while (sendNode != NULL && sendNode != &reqNode->msgBlocks) {
        MsgBlock *sendBlock = (MsgBlock *)ListEntry(sendNode, MsgBlock, node);
        int ret = WriteMessage(socketId, sendBlock->buffer, sendBlock->currentIndex,
            sendFd ? reqNode->fds : NULL,
            sendFd ? &reqNode->fdCount : NULL);
        currentIndex += sendBlock->currentIndex;
        printf("Write msg ret: %d msgId: %u %u %u \n",
            ret, reqNode->msg->msgId, reqNode->msg->msgLen, currentIndex);
        if (ret == 0) {
            sendFd = false;
            sendNode = sendNode->next;
            continue;
        }
        printf("Send msg fail reqId: %u msgId: %d ret: %d \n",
            reqNode->reqId, reqNode->msg->msgId, ret);
        return ret;
    }
    return 0;
}

static int ReadMessage(int socketFd, uint32_t sendMsgId, uint8_t *buf, int len, Result *result)
{
    int rLen = read(socketFd, buf, len);
    if (rLen < 0) {
        printf("Read message from fd %d rLen %d errno: %d \n", socketFd, rLen, errno);
        return TIMEOUT;
    }

    if ((size_t)rLen >= sizeof(ResponseMsg)) {
        ResponseMsg *msg = (ResponseMsg *)(buf);
        if (sendMsgId != msg->msgHdr.msgId) {
            printf("Invalid msg recvd %u %u \n", sendMsgId, msg->msgHdr.msgId);
            return memcpy_s(result, sizeof(Result), &msg->result, sizeof(msg->result));
        }
    }
    return TIMEOUT;
}

static int ClientSendMsg(ReqMsgMgr *reqMgr, ReqMsgNode *reqNode, Result *result)
{
    uint32_t retryCount = 1;
    while (retryCount <= reqMgr->maxRetryCount) {
        if (reqMgr->socketId < 0) { // try create socket
            TryCreateSocket(reqMgr);
            if (reqMgr->socketId < 0) {
                usleep(RETRY_TIME);
                retryCount++;
                continue;
            }
        }

        if (reqNode->msg->msgId == 0) {
            reqNode->msg->msgId = reqMgr->msgNextId++;
        }
        int ret = HandleMsgSend(reqMgr, reqMgr->socketId, reqNode);
        if (ret == 0) {
            ret = ReadMessage(reqMgr->socketId, reqNode->msg->msgId,
                reqMgr->recvBlock.buffer, reqMgr->recvBlock.blockSize, result);
        }
        if (ret == 0) {
            return 0;
        }
        // retry
        CloseClientSocket(reqMgr->socketId);
        reqMgr->socketId = -1;
        reqMgr->msgNextId = 1;
        reqNode->msg->msgId = 0;
        usleep(RETRY_TIME);
        retryCount++;
    }
    return TIMEOUT;
}

int ReqMsgCreate(const char *processName, MsgHandle *reqHandle)
{
    MsgNode *reqNode = CreateMsg(processName);
    *reqHandle = (AppSpawnReqMsgHandle)(reqNode);
    return 0;
}

static void ReqMsgFree(ReqMsgNode *reqNode)
{
    if (reqNode == NULL) {
        return;
    }
    printf("DeleteReqMsg reqId: %u \n", reqNode->reqId);

    reqNode->msgFlags = NULL;
    reqNode->permissionFlags = NULL;
    reqNode->msg = NULL;
    OH_ListRemoveAll(&reqNode->msgBlocks, FreeMsgBlock);
    free(reqNode);
}

static MsgHandle CreateMsg(ClientHandle handle, const char *bundleName)
{
    MsgHandle msgHandle = 0;
    int ret = ReqMsgCreate(bundleName, &msgHandle);
    if (ret != 0) {
        printf("Failed to create req %s \n", bundleName);
        return NULL;
    }

    do {
        ret = MsgSetBundleInfo(msgHandle, 0, bundleName);
        if (ret != 0) {
            printf("Failed to create req %s \n", bundleName);
            break;
        }

        DacInfo dacInfo = {};
        dacInfo.uid = 20241029;              // 20241029 test data
        dacInfo.gid = 20241029;              // 20241029 test data
        dacInfo.gidCount = 2;                // 2 count
        dacInfo.gidTable[0] = 20241029;      // 20241029 test data
        dacInfo.gidTable[1] = 20241029 + 1;  // 20241029 test data
        (void)strcpy_s(dacInfo.userName, sizeof(dacInfo.userName), "test-process");
        
        return msgHandle;
    } while (0);
    ReqMsgFree(msgHandle);
    return NULL;
}

void ClientInit(ClientHandle client)
{
    /*
        初始化client句柄
    */
}

void SendLoop()
{
    ClientHandle clientHandle = NULL;
    ClientInit(clientHandle);
    char msg[MAX_MSG_SIZE];
    int maxTestNum = 10000;
    while (maxTestNum--) {
        printf("请输入要发送的消息，输入EXIT退出程序 \n");
        int ret = scanf_s("%s", msg, sizeof(msg));
        if (ret <= 0) {
            printf("input error \n");
            return;
        }
        if (strcmp(msg, "EXIT") == 0) {
            return;
        }
        MsgHandle msgHandle = CreateMessage(clientHandle, msg)
        Result result = {};
        int ret = ClientSendMsg(clientHandle, msgHandle, result);
        if (ret == 0) {
            printf("Send success. \n");
        } else {
            printf("Send failed. \n");
        }
    }
}

int main(int argc, char *const argv[])
{
    printf("main argc: %d \n", argc);
    if (argc <= 0) {
        return 0;
    }
    
    printf("请输入创建socket的类型：(pipe, tcp)\n");
    char type[MAX_MSG_SIZE];
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

    uint32_t timeout = 200;
    int fd = CreateClientSocket(timeout);
    return 0;
}