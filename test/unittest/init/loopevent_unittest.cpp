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

#include <pthread.h>
#include <sys/eventfd.h>
#include "init_unittest.h"
#include "init_utils.h"
#include "init_param.h"
#include "init_hashmap.h"
#include "loop_event.h"
#include "le_loop.h"
#include "init.h"
#include "param_utils.h"
#include "le_task.h"
#include "le_socket.h"
#include "le_epoll.h"
#include "param_message.h"
#include "param_manager.h"
#include "param_service.h"
#include "trigger_manager.h"

using namespace testing::ext;
using namespace std;

using HashTab = struct {
    HashNodeCompare nodeCompare;
    HashKeyCompare keyCompare;
    HashNodeFunction nodeHash;
    HashKeyFunction keyHash;
    HashNodeOnFree nodeFree;
    int maxBucket;
    uint32_t tableId;
    HashNode *buckets[0];
};

static void OnReceiveRequest(const TaskHandle task, const uint8_t *buffer, uint32_t nread)
{
    UNUSED(task);
    UNUSED(buffer);
    UNUSED(nread);
}

static void Close(ParamTaskPtr client)
{
    (void)(client);
    return;
}

static void ProcessAsyncEvent(const TaskHandle taskHandle, uint64_t eventId, const uint8_t *buffer, uint32_t buffLen)
{
    UNUSED(taskHandle);
    UNUSED(eventId);
    UNUSED(buffer);
    UNUSED(buffLen);
}

static int IncomingConnect(LoopHandle loop, TaskHandle server)
{
    UNUSED(loop);
    UNUSED(server);
    return 0;
}

static void ProcessWatchEventTest(WatcherHandle taskHandle, int fd, uint32_t *events, const void *context)
{
    UNUSED(taskHandle);
    UNUSED(fd);
    UNUSED(events);
    UNUSED(context);
}

static EventLoop *g_testLoop = nullptr;

static void *RunLoopThread(void *arg)
{
    LE_RunLoop((LoopHandle)g_testLoop);
    return nullptr;
}

namespace init_ut {
class LoopEventUnittest : public testing::Test {
public:
    LoopEventUnittest() {};
    virtual ~LoopEventUnittest() {};
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
    void TestBody(void) {};
    void StreamTaskTest()
    {
        LE_StreamInfo streamInfo = {};
        streamInfo.recvMessage = OnReceiveRequest;
        streamInfo.baseInfo.flags = TASK_STREAM | TASK_PIPE | TASK_CONNECT | TASK_TEST;
        streamInfo.server = (char *)"/data/testpipea";
        TaskHandle clientTaskHandle = nullptr;
        LE_AcceptStreamClient(LE_GetDefaultLoop(), (TaskHandle)GetParamWorkSpace()->serverTask,
            &clientTaskHandle, &streamInfo);
        if (clientTaskHandle == nullptr) {
            return;
        }
        ((StreamConnectTask *)clientTaskHandle)->stream.base.handleEvent(LE_GetDefaultLoop(),
            (TaskHandle)clientTaskHandle, Event_Read);
        ((StreamConnectTask *)clientTaskHandle)->stream.base.handleEvent(LE_GetDefaultLoop(),
            (TaskHandle)clientTaskHandle, Event_Write);

        ((StreamConnectTask *)clientTaskHandle)->stream.base.handleEvent(LE_GetDefaultLoop(),
            (TaskHandle)clientTaskHandle, 0);
        ((HashTab *)((EventLoop *)LE_GetDefaultLoop())->taskMap)->nodeFree(
            &((BaseTask *)(&clientTaskHandle))->hashNode);

        streamInfo.baseInfo.flags = TASK_STREAM | TASK_PIPE |  TASK_SERVER;
        streamInfo.server = (char *)"/data/testpipeb";
        TaskHandle clientTaskHandleb = nullptr;
        LE_CreateStreamClient(LE_GetDefaultLoop(), &clientTaskHandleb, &streamInfo);
        if (clientTaskHandleb == nullptr) {
            return;
        }
        ((StreamClientTask *)clientTaskHandleb)->stream.base.handleEvent(LE_GetDefaultLoop(),
            clientTaskHandleb, Event_Read);
        ((StreamClientTask *)clientTaskHandleb)->stream.base.handleEvent(LE_GetDefaultLoop(),
            clientTaskHandleb, Event_Write);
        ((StreamClientTask *)clientTaskHandleb)->stream.base.innerClose(LE_GetDefaultLoop(), clientTaskHandleb);
        
        TaskHandle clientTaskHandlec = nullptr;
        streamInfo.baseInfo.flags = TASK_STREAM | TASK_TCP | TASK_SERVER;
        streamInfo.server = (char *)"0.0.0.0:10110";
        LE_CreateStreamClient(LE_GetDefaultLoop(), &clientTaskHandlec, &streamInfo);
        if (clientTaskHandlec == nullptr) {
            return;
        }
        TaskHandle clientTaskHandled = nullptr;
        streamInfo.baseInfo.flags = TASK_STREAM | TASK_TCP | TASK_CONNECT;
        streamInfo.server = (char *)"127.0.0.1:10111";
        LE_CreateStreamClient(LE_GetDefaultLoop(), &clientTaskHandled, &streamInfo);
        if (clientTaskHandled == nullptr) {
            return;
        }
    }
    void LeTaskTest()
    {
        ParamTaskPtr serverTask = nullptr;
        LE_StreamServerInfo info = {};
        info.baseInfo.flags = TASK_STREAM | TASK_PIPE | TASK_SERVER | TASK_TEST;
        info.server = (char *)"/data/testpipe";
        info.baseInfo.close = Close;
        info.incommingConntect = IncomingConnect;
        LE_CreateStreamServer(LE_GetDefaultLoop(), &serverTask, &info);
        if (serverTask == nullptr) {
            return;
        }
        ((StreamServerTask *)serverTask)->base.handleEvent(LE_GetDefaultLoop(), serverTask, Event_Write);
        ((StreamServerTask *)serverTask)->base.handleEvent(LE_GetDefaultLoop(), serverTask, Event_Read);

        uint64_t eventId = 0;
        ParamStreamInfo paramStreamInfo = {};
        paramStreamInfo.flags = PARAM_TEST_FLAGS;
        paramStreamInfo.server = NULL;
        paramStreamInfo.close = Close;
        paramStreamInfo.recvMessage = ProcessMessage;
        paramStreamInfo.incomingConnect = NULL;
        ParamTaskPtr client = NULL;
        int ret = ParamStreamCreate(&client, GetParamWorkSpace()->serverTask, &paramStreamInfo, sizeof(ParamWatcher));
        PARAM_CHECK(ret == 0, return, "Failed to create client");

        BufferHandle handle = LE_CreateBuffer(LE_GetDefaultLoop(), 1 + sizeof(eventId));
        LE_Buffer *buffer = (LE_Buffer *)handle;
        AddBuffer((StreamTask *)client, buffer);
        ((StreamConnectTask *)client)->stream.base.handleEvent(LE_GetDefaultLoop(), (TaskHandle)(&client), Event_Write);

        LE_Buffer *next = nullptr;
        LE_Buffer *nextBuff = GetNextBuffer((StreamTask *)client, next);
        if (nextBuff != nullptr) {
            LE_FreeBuffer(LE_GetDefaultLoop(), (TaskHandle)&client, nextBuff);
        }

        ret = ParamStreamCreate(&client, GetParamWorkSpace()->serverTask, &paramStreamInfo, sizeof(ParamWatcher));
        PARAM_CHECK(ret == 0, return, "Failed to create client");
        ((StreamConnectTask *)(&client))->stream.base.handleEvent(LE_GetDefaultLoop(),
            (TaskHandle)(&client), Event_Read);
    }
    void ProcessEventTest()
    {
        ProcessEvent((EventLoop *)LE_GetDefaultLoop(), 1, Event_Read);
    }
    void ProcessasynEvent()
    {
        TaskHandle asynHandle = nullptr;
        LE_CreateAsyncTask(LE_GetDefaultLoop(), &asynHandle, ProcessAsyncEvent);
        if (asynHandle == nullptr) {
            return;
        }
        ((AsyncEventTask *)asynHandle)->stream.base.handleEvent(LE_GetDefaultLoop(), asynHandle, Event_Read);
        ((AsyncEventTask *)asynHandle)->stream.base.handleEvent(LE_GetDefaultLoop(), asynHandle, Event_Write);
        LE_StopAsyncTask(LE_GetDefaultLoop(), asynHandle);
    }
    void ProcessWatcherTask()
    {
        WatcherHandle handle = nullptr;
        LE_WatchInfo info = {};
        info.fd = -1;
        info.flags = WATCHER_ONCE;
        info.events = Event_Read;
        info.processEvent = ProcessWatchEventTest;
        LE_StartWatcher(LE_GetDefaultLoop(), &handle, &info, nullptr);
        if (handle == nullptr) {
            return;
        }
        ((WatcherTask *)handle)->base.handleEvent(LE_GetDefaultLoop(), (TaskHandle)handle, Event_Read);
    }
    void CreateSocketTest()
    {
        ParamTaskPtr clientTask = nullptr;
        LE_StreamServerInfo info = {};
        info.baseInfo.flags = TASK_PIPE | TASK_CONNECT | TASK_TEST;
        info.server = (char *)"/data/testpipe";
        info.baseInfo.close = Close;
        info.incommingConntect = IncomingConnect;
        LE_CreateStreamServer(LE_GetDefaultLoop(), &clientTask, &info);
        EXPECT_NE(clientTask, nullptr);
        if (clientTask == nullptr) {
            return;
        }
        LE_GetSocketFd(clientTask);
        AcceptSocket(-1, TASK_PIPE);
        AcceptSocket(-1, TASK_TCP);
    }
};

HWTEST_F(LoopEventUnittest, StreamTaskTest, TestSize.Level1)
{
    LoopEventUnittest loopevtest = LoopEventUnittest();
    loopevtest.StreamTaskTest();
}

HWTEST_F(LoopEventUnittest, LeTaskTest, TestSize.Level1)
{
    LoopEventUnittest loopevtest = LoopEventUnittest();
    loopevtest.LeTaskTest();
}
HWTEST_F(LoopEventUnittest, runServerTest, TestSize.Level1)
{
    LoopEventUnittest loopevtest = LoopEventUnittest();
    loopevtest.ProcessEventTest();
}
HWTEST_F(LoopEventUnittest, ProcessasynEvent, TestSize.Level1)
{
    LoopEventUnittest loopevtest = LoopEventUnittest();
    loopevtest.ProcessasynEvent();
}
HWTEST_F(LoopEventUnittest, CreateSocketTest, TestSize.Level1)
{
    LoopEventUnittest loopevtest = LoopEventUnittest();
    loopevtest.CreateSocketTest();
}
HWTEST_F(LoopEventUnittest, ProcessWatcherTask, TestSize.Level1)
{
    LoopEventUnittest loopevtest = LoopEventUnittest();
    loopevtest.ProcessWatcherTask();
}
HWTEST_F(LoopEventUnittest, RunLoopThread, TestSize.Level1)
{
    pthread_t tid = 0;
    int fd = eventfd(1, EFD_NONBLOCK | EFD_CLOEXEC);
    LE_CreateLoop((LoopHandle *)&g_testLoop);
    EventEpoll *epoll = (EventEpoll *)g_testLoop;
    struct epoll_event event = {};
    event.events = EPOLLIN;
    if (fd >= 0) {
        epoll_ctl(epoll->epollFd, EPOLL_CTL_ADD, fd, &event);
    }
    pthread_create(&tid, nullptr, RunLoopThread, nullptr);
    LE_StopLoop((LoopHandle)g_testLoop);
    event.events = EPOLLOUT;
    epoll_ctl(epoll->epollFd, EPOLL_CTL_MOD, fd, &event);
    pthread_join(tid, nullptr);
}
} // namespace init_ut
