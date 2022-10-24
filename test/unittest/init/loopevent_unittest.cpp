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
#include <gtest/gtest.h>
#include <pthread.h>
#include <sys/eventfd.h>

#include "begetctl.h"
#include "init.h"
#include "init_hashmap.h"
#include "init_param.h"
#include "init_utils.h"
#include "le_epoll.h"
#include "le_loop.h"
#include "le_socket.h"
#include "le_task.h"
#include "loop_event.h"
#include "param_manager.h"
#include "param_message.h"
#include "param_utils.h"
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

extern "C" {
void OnClose(ParamTaskPtr client);
}
static LE_STATUS TestHandleTaskEvent(const LoopHandle loop, const TaskHandle task, uint32_t oper)
{
    return LE_SUCCESS;
}

static void OnReceiveRequest(const TaskHandle task, const uint8_t *buffer, uint32_t nread)
{
    UNUSED(task);
    UNUSED(buffer);
    UNUSED(nread);
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
    int CreateServerTask()
    {
        CheckTaskFlags(nullptr, Event_Write);
        ParamStreamInfo info = {};
        info.server = const_cast<char *>(PIPE_NAME);
        info.close = NULL;
        info.recvMessage = NULL;
        info.incomingConnect = OnIncomingConnect;
        return ParamServerCreate(&serverTask_, &info);
    }
    void StreamTaskTest ()
    {
        LE_StreamInfo streamInfo = {};
        streamInfo.recvMessage = OnReceiveRequest;
        streamInfo.baseInfo.flags = TASK_STREAM | TASK_PIPE | TASK_CONNECT | TASK_TEST;
        streamInfo.server = (char *)"/data/testpipea";
        TaskHandle clientTaskHandle = nullptr;
        LE_AcceptStreamClient(LE_GetDefaultLoop(), (TaskHandle)serverTask_, &clientTaskHandle, &streamInfo);
        if (clientTaskHandle == nullptr) {
            return;
        }
        ((StreamConnectTask *)clientTaskHandle)->stream.base.handleEvent(LE_GetDefaultLoop(),
            (TaskHandle)clientTaskHandle, Event_Read);
        ((StreamConnectTask *)clientTaskHandle)->stream.base.handleEvent(LE_GetDefaultLoop(),
            (TaskHandle)clientTaskHandle, Event_Write);

        ((StreamConnectTask *)clientTaskHandle)->stream.base.handleEvent(LE_GetDefaultLoop(),
            (TaskHandle)clientTaskHandle, 0);

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
        LE_StreamServerInfo info = {};
        info.baseInfo.flags = TASK_STREAM | TASK_PIPE | TASK_SERVER | TASK_TEST;
        info.server = (char *)"/data/testpipe";
        info.baseInfo.close = OnClose;
        info.incommingConnect = IncomingConnect;
        LE_CreateStreamServer(LE_GetDefaultLoop(), &serverTask_, &info);
        if (serverTask_ == nullptr) {
            return;
        }
        ((StreamServerTask *)serverTask_)->base.handleEvent(LE_GetDefaultLoop(), serverTask_, Event_Read);

        uint64_t eventId = 0;
        ParamStreamInfo paramStreamInfo = {};
        paramStreamInfo.flags = PARAM_TEST_FLAGS;
        paramStreamInfo.server = NULL;
        paramStreamInfo.close = OnClose;
        paramStreamInfo.recvMessage = ProcessMessage;
        paramStreamInfo.incomingConnect = NULL;
        ParamTaskPtr client = NULL;
        int ret = ParamStreamCreate(&client, serverTask_, &paramStreamInfo, sizeof(ParamWatcher));
        PARAM_CHECK(ret == 0, return, "Failed to create client");

        BufferHandle handle = LE_CreateBuffer(LE_GetDefaultLoop(), 1 + sizeof(eventId));
        LE_Buffer *buffer = (LE_Buffer *)handle;
        AddBuffer((StreamTask *)client, buffer);
        ((StreamConnectTask *)client)->stream.base.handleEvent(LE_GetDefaultLoop(), (TaskHandle)(client), Event_Write);

        ParamMessage *request = (ParamMessage *)CreateParamMessage(MSG_SET_PARAM, "name", sizeof(ParamMessage));
        ((StreamConnectTask *)client)->recvMessage(LE_GetDefaultLoop(), reinterpret_cast<uint8_t *>(request),
            sizeof(ParamMessage));

        LE_Buffer *next = nullptr;
        EXPECT_EQ(GetNextBuffer((StreamTask *)client, next), nullptr);
        ParamWatcher *watcher = (ParamWatcher *)ParamGetTaskUserData(client);
        PARAM_CHECK(watcher != nullptr, return, "Failed to get watcher");
        OH_ListInit(&watcher->triggerHead);
        OnClose(client);
        LE_FreeBuffer(LE_GetDefaultLoop(), (TaskHandle)client, nullptr);
        return;
    }
    void ProcessEventTest()
    {
        ProcessEvent((EventLoop *)LE_GetDefaultLoop(), 1, Event_Read);
        LE_BaseInfo info = {TASK_EVENT, NULL};
        int testfd = 65535; // 65535 is not exist fd
        BaseTask *task = CreateTask(LE_GetDefaultLoop(), testfd, &info, sizeof(StreamClientTask));
        if (task != nullptr) {
            task->handleEvent = TestHandleTaskEvent;
            ProcessEvent((EventLoop *)LE_GetDefaultLoop(), testfd, Event_Read);
            ((HashTab *)(((EventLoop *)LE_GetDefaultLoop())->taskMap))->nodeFree(&task->hashNode);
        }
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
        ((WatcherTask *)handle)->base.handleEvent(LE_GetDefaultLoop(), (TaskHandle)handle, 0);
        ((WatcherTask *)handle)->base.flags = WATCHER_ONCE;
        ((WatcherTask *)handle)->base.handleEvent(LE_GetDefaultLoop(), (TaskHandle)handle, Event_Read);
    }
    void CreateSocketTest()
    {
        ParamTaskPtr serverTask = nullptr;
        LE_StreamServerInfo info = {};
        info.baseInfo.flags = TASK_PIPE | TASK_CONNECT | TASK_TEST;
        info.server = (char *)"/data/testpipe";
        info.baseInfo.close = OnClose;
        info.incommingConnect = IncomingConnect;
        info.socketId = 1111; // 1111 is test fd
        LE_CreateStreamServer(LE_GetDefaultLoop(), &serverTask, &info);
        EXPECT_NE(serverTask, nullptr);
        if (serverTask == nullptr) {
            return;
        }
        ((StreamServerTask *)serverTask)->base.taskId.fd = -1;
        OnIncomingConnect(LE_GetDefaultLoop(), serverTask);
        LE_GetSocketFd(serverTask);
        AcceptSocket(-1, TASK_PIPE);
        AcceptSocket(-1, TASK_TCP);
        AcceptSocket(-1, TASK_TEST);
    }

private:
    ParamTaskPtr serverTask_ = NULL;
};

HWTEST_F(LoopEventUnittest, StreamTaskTest, TestSize.Level1)
{
    LoopEventUnittest loopevtest = LoopEventUnittest();
    loopevtest.CreateServerTask();
    loopevtest.StreamTaskTest();
    LE_StreamInfo streamInfo = {};
    streamInfo.recvMessage = OnReceiveRequest;
    streamInfo.baseInfo.flags = TASK_PIPE |  TASK_CONNECT;
    streamInfo.server = (char *)PIPE_NAME;
    TaskHandle clientTaskHandlec = nullptr;
    LE_CreateStreamClient(LE_GetDefaultLoop(), &clientTaskHandlec, &streamInfo);
    EXPECT_NE(clientTaskHandlec, nullptr);
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

static LoopHandle g_loop = nullptr;
static int g_timeCount = 0;
static void Test_ProcessTimer(const TimerHandle taskHandle, void *context)
{
    g_timeCount++;
    printf("Test_ProcessTimer %d\n", g_timeCount);
    if (g_timeCount > 1) {
        LE_StopLoop(g_loop);
    }
}

HWTEST_F(LoopEventUnittest, LoopRunTest, TestSize.Level1)
{
    ASSERT_EQ(LE_CreateLoop(&g_loop), 0);
    TimerHandle timer = nullptr;
    int ret = LE_CreateTimer(g_loop, &timer, Test_ProcessTimer, nullptr);
    ASSERT_EQ(ret, 0);
    ret = LE_StartTimer(g_loop, timer, 500, 2);
    ASSERT_EQ(ret, 0);
    LE_CloseLoop(g_loop);
    LE_RunLoop(g_loop);
    LE_CloseLoop(g_loop);
}
}  // namespace init_ut
