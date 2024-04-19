/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>
#include <thread>
#include <sys/eventfd.h>
#include <cstdarg>

#include "begetctl.h"
#include "cJSON.h"
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

namespace init_ut {
const std::string TCP_SERVER = "127.0.0.1:7777";
const std::string PIPE_SERVER = STARTUP_INIT_UT_PATH "/dev/unix/socket/testsocket";
const std::string WATCHER_FILE = STARTUP_INIT_UT_PATH "/test_watcher_file";
const std::string FORMAT_STR = "{ \"cmd\":%d, \"message\":\"%s\" }";

static LoopHandle g_loopClient_ = nullptr;
static LoopHandle g_loopServer_ = nullptr;
static int g_maxCount = 0;
static int g_timeCount = 0;
static int g_cmd = 2;
static void DecodeMessage(const char *buffer, size_t nread, uint32_t &cmd)
{
    cJSON *root = cJSON_ParseWithLength(buffer, nread);
    if (root == nullptr) {
        EXPECT_NE(root, nullptr);
        printf("Invalid message %s  \n", buffer);
        return;
    }
    printf("Message: %s  \n", cJSON_GetStringValue(cJSON_GetObjectItem(root, "message")));
    cmd = cJSON_GetNumberValue(cJSON_GetObjectItem(root, "cmd"));
    printf("cmd: %d \n", cmd);
    cJSON_Delete(root);
    return;
}

static void SendMessage(const LoopHandle loopHandle, const TaskHandle taskHandle, const char *message, ...)
{
    uint32_t bufferSize = 1024; // 1024 buffer size
    BufferHandle handle = LE_CreateBuffer(loopHandle, bufferSize);
    char *buffer = (char *)LE_GetBufferInfo(handle, nullptr, &bufferSize);

    va_list vargs;
    va_start(vargs, message);
    if (vsnprintf_s(buffer, bufferSize, bufferSize - 1, message, vargs) == -1) {
        LE_FreeBuffer(loopHandle, taskHandle, handle);
        va_end(vargs);
        EXPECT_EQ(1, 0);
        return;
    }
    va_end(vargs);
    int ret = LE_Send(loopHandle, taskHandle, handle, bufferSize);
    EXPECT_EQ(ret, 0);
}

static void TestOnClose(const TaskHandle taskHandle)
{
}

static LE_STATUS TestHandleTaskEvent(const LoopHandle loop, const TaskHandle task, uint32_t oper)
{
    return LE_SUCCESS;
}

static void TestOnReceiveRequest(const TaskHandle task, const uint8_t *buffer, uint32_t nread)
{
    EXPECT_NE(buffer, nullptr);
    if (buffer == nullptr) {
        return;
    }
    printf("Server receive message %s \n", reinterpret_cast<const char *>(buffer));
    uint32_t cmd = 0;
    DecodeMessage(reinterpret_cast<const char *>(buffer), nread, cmd);
    SendMessage(g_loopServer_, task, reinterpret_cast<const char *>(buffer));
}

static void TestClientOnReceiveRequest(const TaskHandle task, const uint8_t *buffer, uint32_t nread)
{
    printf("Client receive message %s \n", reinterpret_cast<const char *>(buffer));
    EXPECT_NE(buffer, nullptr);
    if (buffer == nullptr) {
        return;
    }
    uint32_t cmd = 0;
    DecodeMessage(reinterpret_cast<const char *>(buffer), nread, cmd);
    if (cmd == 5 || cmd == 2) { // 2 5 close server
        LE_StopLoop(g_loopClient_);
    }
}

static void ProcessAsyncEvent(const TaskHandle taskHandle, uint64_t eventId, const uint8_t *buffer, uint32_t buffLen)
{
    UNUSED(taskHandle);
    UNUSED(eventId);
    UNUSED(buffer);
    UNUSED(buffLen);
}

static void TestSendMessageComplete(const TaskHandle taskHandle, BufferHandle handle)
{
    printf("SendMessage result %d \n", LE_GetSendResult(handle));
    uint32_t bufferSize = 1024; // 1024 buffer size
    char *buffer = (char *)LE_GetBufferInfo(handle, nullptr, &bufferSize);
    uint32_t cmd = 0;
    DecodeMessage(reinterpret_cast<const char *>(buffer), bufferSize, cmd);
    if (cmd == 5) { // 5 close server
        LE_StopLoop(g_loopServer_);
    }
}

static int TestTcpIncomingConnect(LoopHandle loop, TaskHandle server)
{
    PARAM_CHECK(server != nullptr, return -1, "Error server");
    printf("Tcp connect incoming \n");
    TaskHandle stream;
    LE_StreamInfo info = {};
    info.baseInfo.flags = TASK_STREAM | TASK_TCP | TASK_CONNECT;
    info.baseInfo.close = TestOnClose;
    info.baseInfo.userDataSize = 0;
    info.disConnectComplete = nullptr;
    info.sendMessageComplete = TestSendMessageComplete;
    info.recvMessage = TestOnReceiveRequest;
    LE_STATUS ret = LE_AcceptStreamClient(loop, server, &stream, &info);
    EXPECT_EQ(ret, 0);
    return 0;
}

static int TestPipIncomingConnect(LoopHandle loop, TaskHandle server)
{
    PARAM_CHECK(server != nullptr, return -1, "Error server");
    printf("Pipe connect incoming \n");
    TaskHandle stream;
    LE_StreamInfo info = {};
    info.baseInfo.flags = TASK_STREAM | TASK_PIPE | TASK_CONNECT;
    info.baseInfo.close = TestOnClose;
    info.baseInfo.userDataSize = 0;
    info.disConnectComplete = nullptr;
    info.sendMessageComplete = TestSendMessageComplete;
    info.recvMessage = TestOnReceiveRequest;
    LE_STATUS ret = LE_AcceptStreamClient(loop, server, &stream, &info);
    EXPECT_EQ(ret, 0);
    return 0;
}

static void TestConnectComplete(const TaskHandle client)
{
    printf("Connect complete \n");
}

static void TestDisConnectComplete(const TaskHandle client)
{
    printf("DisConnect complete \n");
    LE_StopLoop(g_loopClient_);
}

static void TestProcessTimer(const TimerHandle taskHandle, void *context)
{
    g_timeCount++;
    printf("ProcessTimer %d\n", g_timeCount);
    if (g_maxCount == 2) { // 2 stop
        if (g_timeCount >= g_maxCount) {
            LE_StopLoop(g_loopClient_);
        }
    }
    if (g_maxCount == 3) { // 3 stop timer
        if (g_timeCount >= g_maxCount) {
            LE_StopTimer(g_loopClient_, taskHandle);
            LE_StopLoop(g_loopClient_);
        }
    }
    if (g_maxCount == 10) { // 10 write watcher file
        FILE *tmpFile = fopen(WATCHER_FILE.c_str(), "wr");
        if (tmpFile != nullptr) {
            fprintf(tmpFile, "%s", "test watcher file 22222222222");
            (void)fflush(tmpFile);
            fclose(tmpFile);
        }
        LE_StopTimer(g_loopClient_, taskHandle);
        LE_StopLoop(g_loopClient_);
    }
}

static void ProcessWatchEventTest(WatcherHandle taskHandle, int fd, uint32_t *events, const void *context)
{
    UNUSED(taskHandle);
    UNUSED(fd);
    UNUSED(events);
    UNUSED(context);
    printf("Process watcher event \n");
    LE_StopLoop(g_loopClient_);
}

class LoopServerUnitTest : public testing::Test {
public:
    LoopServerUnitTest() {};
    virtual ~LoopServerUnitTest() {};
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
    void TestBody(void) {};

    // for thread to create tcp\pipe server
    void RunServer(void)
    {
        TaskHandle tcpServer = nullptr;
        TaskHandle pipeServer = nullptr;
        LE_STATUS ret = LE_CreateLoop(&g_loopServer_);
        EXPECT_EQ(ret, 0);
        // create server for tcp
        LE_StreamServerInfo info = {};
        info.baseInfo.flags = TASK_STREAM | TASK_TCP | TASK_SERVER;
        info.socketId = -1;
        info.server = const_cast<char *>(TCP_SERVER.c_str());
        info.baseInfo.close = TestOnClose;
        info.incommingConnect = TestTcpIncomingConnect;
        ret = LE_CreateStreamServer(g_loopServer_, &tcpServer, &info);
        EXPECT_EQ(ret, 0);

        info.baseInfo.flags = TASK_STREAM | TASK_PIPE | TASK_SERVER;
        info.socketId = -1;
        info.server = const_cast<char *>(PIPE_SERVER.c_str());
        info.baseInfo.close = TestOnClose;
        info.incommingConnect = TestPipIncomingConnect;
        ret = LE_CreateStreamServer(g_loopServer_, &pipeServer, &info);
        EXPECT_EQ(ret, 0);

        printf("Run server pipeServer_ \n");
        // run loop for server
        LE_RunLoop(g_loopServer_);

        printf("Run server pipeServer_ \n");
        LE_CloseStreamTask(g_loopServer_, pipeServer);
        pipeServer = nullptr;
        LE_CloseStreamTask(g_loopServer_, tcpServer);
        tcpServer = nullptr;
        LE_CloseLoop(g_loopServer_);
        g_loopServer_ = nullptr;
    }

    void StartServer()
    {
        std::thread(&LoopServerUnitTest::RunServer, this).detach();
        sleep(1);
    }

    TaskHandle CreateConnect(const char *tcpServer, uint32_t flags)
    {
        if (g_loopClient_ == nullptr) {
            LE_STATUS ret = LE_CreateLoop(&g_loopClient_);
            EXPECT_EQ(ret, 0);
        }

        TaskHandle task = nullptr;
        LE_StreamInfo info = {};
        info.baseInfo.flags = TASK_STREAM | flags | TASK_CONNECT;
        info.server = const_cast<char *>(tcpServer);
        info.baseInfo.userDataSize = 0;
        info.baseInfo.close = TestOnClose;
        info.disConnectComplete = TestDisConnectComplete;
        info.connectComplete = TestConnectComplete;
        info.sendMessageComplete = nullptr;
        info.recvMessage = TestClientOnReceiveRequest;
        LE_STATUS status = LE_CreateStreamClient(g_loopClient_, &task, &info);
        EXPECT_EQ(status, 0);
        return task;
    }

    WatcherHandle CreateWatcherTask(int fd, const char *fileName)
    {
        if (g_loopClient_ == nullptr) {
            LE_STATUS ret = LE_CreateLoop(&g_loopClient_);
            EXPECT_EQ(ret, 0);
        }
        WatcherHandle handle = nullptr;
        LE_WatchInfo info = {};
        info.fd = fd;
        info.flags = WATCHER_ONCE;
        info.events = EVENT_READ | EVENT_WRITE;
        info.processEvent = ProcessWatchEventTest;
        LE_STATUS status = LE_StartWatcher(g_loopClient_, &handle, &info, nullptr);
        EXPECT_EQ(status, 0);
        return handle;
    }

    TimerHandle CreateTimerTask(int repeat)
    {
        if (g_loopClient_ == nullptr) {
            LE_STATUS ret = LE_CreateLoop(&g_loopClient_);
            EXPECT_EQ(ret, 0);
        }
        TimerHandle timer = nullptr;
        int ret = LE_CreateTimer(g_loopClient_, &timer, TestProcessTimer, nullptr);
        EXPECT_EQ(ret, 0);
        ret = LE_StartTimer(g_loopClient_, timer, 500, repeat); // 500 ms
        EXPECT_EQ(ret, 0);
        return timer;
    }
private:
    std::thread *serverThread_ = nullptr;
};

HWTEST_F(LoopServerUnitTest, TestRunServer, TestSize.Level1)
{
    LoopServerUnitTest test;
    test.StartServer();
}

HWTEST_F(LoopServerUnitTest, TestPipConnect, TestSize.Level1)
{
    g_cmd = 2; // 2 only close client
    LoopServerUnitTest test;
    TaskHandle pipe = test.CreateConnect(PIPE_SERVER.c_str(), TASK_PIPE);
    EXPECT_NE(pipe, nullptr);
    SendMessage(g_loopClient_, pipe, FORMAT_STR.c_str(), g_cmd, "connect success");
    LE_RunLoop(g_loopClient_);
    LE_CloseStreamTask(g_loopClient_, pipe);
    LE_CloseLoop(g_loopClient_);
    g_loopClient_ = nullptr;
}

HWTEST_F(LoopServerUnitTest, TestTcpConnect, TestSize.Level1)
{
    g_cmd = 2; // 2 only close client
    LoopServerUnitTest test;
    TaskHandle tcp = test.CreateConnect(TCP_SERVER.c_str(), TASK_TCP);
    EXPECT_NE(tcp, nullptr);
    SendMessage(g_loopClient_, tcp, FORMAT_STR.c_str(), g_cmd, "connect success");
    LE_RunLoop(g_loopClient_);
    LE_CloseStreamTask(g_loopClient_, tcp);
    LE_CloseLoop(g_loopClient_);
    g_loopClient_ = nullptr;
}

HWTEST_F(LoopServerUnitTest, TestTimer, TestSize.Level1)
{
    LoopServerUnitTest test;
    g_maxCount = 2; // 2 stop
    TimerHandle timer = test.CreateTimerTask(2);
    EXPECT_NE(timer, nullptr);
    LE_RunLoop(g_loopClient_);
    LE_CloseLoop(g_loopClient_);
    g_loopClient_ = nullptr;
}

HWTEST_F(LoopServerUnitTest, TestTimer2, TestSize.Level1)
{
    LoopServerUnitTest test;
    g_maxCount = 3; // 3 stop timer
    TimerHandle timer = test.CreateTimerTask(3);
    EXPECT_NE(timer, nullptr);
    LE_RunLoop(g_loopClient_);
    LE_CloseLoop(g_loopClient_);
    g_loopClient_ = nullptr;
}

HWTEST_F(LoopServerUnitTest, TestWatcher, TestSize.Level1)
{
    int fd = open(WATCHER_FILE.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd >= 0) {
        write(fd, WATCHER_FILE.c_str(), WATCHER_FILE.size());
    }
    EXPECT_GE(fd, 0);
    printf("Watcher fd %d \n", fd);
    LoopServerUnitTest test;
    WatcherHandle watcher = test.CreateWatcherTask(3, WATCHER_FILE.c_str());
    EXPECT_NE(watcher, nullptr);
    g_maxCount = 10; // 10 write watcher file
    TimerHandle timer = test.CreateTimerTask(1);
    EXPECT_NE(timer, nullptr);

    LE_RunLoop(g_loopClient_);
    LE_RemoveWatcher(g_loopClient_, watcher);
    close(fd);
    LE_CloseLoop(g_loopClient_);
    g_loopClient_ = nullptr;
}

HWTEST_F(LoopServerUnitTest, TestStopServer, TestSize.Level1)
{
    g_cmd = 5; // 5 close server
    LoopServerUnitTest test;
    TaskHandle pip = test.CreateConnect(PIPE_SERVER.c_str(), TASK_PIPE);
    EXPECT_NE(pip, nullptr);
    SendMessage(g_loopClient_, pip, FORMAT_STR.c_str(), g_cmd, "connect success");
    LE_RunLoop(g_loopClient_);
    LE_CloseStreamTask(g_loopClient_, pip);
    LE_CloseLoop(g_loopClient_);
    g_loopClient_ = nullptr;
}

HWTEST_F(LoopServerUnitTest, Init_LoopServerTest_ServerTimeout008, TestSize.Level1)
{
    int flag = TASK_STREAM | TASK_PIPE | TASK_SERVER | TASK_TEST;
    int serverSock = CreateSocket(flag, "/data/test1pipe");
    EXPECT_NE(serverSock, -1);
    int ret = AcceptSocket(serverSock, flag);
    EXPECT_EQ(ret, -1);
}
}  // namespace init_ut
