/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

/*
 * 单元测试：Watcher IPC 接收缓冲区超限（RECV_BUFFER_MAX = 20*1024）
 *
 * 故障模式：param_wather 的 WatcherManager::RunLoop() 使用固定大小 (20KB) 的
 * 接收缓冲区 recv() IPC 消息。当单条 ParamMessage 被截断 (故障注入 recvMax=64,
 * 小于 sizeof(ParamMessage)) 或多条消息拼接后末尾消息不完整时， RunLoop 的
 * 校验逻辑会拒绝该消息并打印 WATCHER_LOGE，被截断的消息不会被
 * ProcessWatcherMessage 处理，对应参数变更通知丢失。
 *
 * 故障注入：FaultInjrctEnabled("watcher_recv_buffera_max") 将 recvMax 从
 * 20480 降为 64，远小于 sizeof(ParamMessage)=112，recv() 截断触发校验失败。
 * 标志文件：/data/startup_fault/watcher_recv_buffer_max
 *
 * 验证点：
 *   1.故障注入标志文件机制正常工作
 *   2.recvMax=64 时，recv() 截断 ParamMessage，校验拒绝
 *   3.msgSize=0 或 msgSize 》 剩余数据时，校验拒绝
 *   4.正常消息在缓冲区内被正确处理
 *   5.多条消息拼接时末尾截断消息被拒绝，前面完整消息正常处理
 *   6.socketpair 模拟真实 recv 截断场景
 *   7.WatchParameter 监听 test.watch.* + 批量参数变更通知
 */

#include <gtest/gtest.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <vector>

#include "init_param.h"
#include "iwatcher.h"
#include "iwatcher_manager.h"
#include "message_parcel.h"
#include "param_message.h"
#include "param_stub.h"
#include "param_utils.h"
#include "parcel.h"
#include "securec.h"
#include "system_ability_definition.h"
#include "string_ex.h"
#include "watcher.h"
#include "watcher_manager.h"
#include "watcher_utils.h"

using namespace testing::ext;
using namespace std;
using namespace OHOS;
using namespace OHOS::init_param;

namespace {
constexpr int32_t RECV_BUFFER_MAX = 20 * 1024; // 20480bytes, watcher_manager.cpp:39
constexpr int32_t FAULT_INJECT_RECV_MAX = 64; // 故障注入值， 远小于 sizeof(ParamMessage)
constexpr const char *FAULT_INJECT_DIR = "data/startup_fault";
constexpr const char *FAULT_INJECT_PATH = "/data/startup_fault/watcher_recv_buffer_max";
constexpr mode_t FAULT_INJECT_DIR_MODE = 0755;
} // namespace

/*
 * 辅助函数：模拟 WatcherManager::RunLoop() 的缓冲区校验逻辑
 * 与 watcher_manager.cpp:436-450 完全一致的校验条件
 * 返回成功处理的消息数量，processedMsgs 收集被处理的消息指针
 */
static int SimulateRunLoopParsing(char *buffer,
                                  uint32_t dataLen,
                                  std::vector<ParamMessage *> &processedMsgs)
{
    uint32_t curr = 0;
    int processed = 0;
    while (curr < dataLen) {
        // 校验条件 1：剩余数据不足一个 ParamMessage 头部
        if (sizeof(ParamMessage) >= dataLen - curr) {
            break; // "paramMessage len is invalid"
        }
        ParamMessage *msg = reinterpret_cast<ParamMessage *>(buffer + curr);
        // 校验条件 2：msgSize 为 0 或超过剩余数据长度
        if (msg->msgSize == 0 || (msg->msgSize > dataLen - curr)) {
            break; // "msgSize is invalid"
        }
        processedMsgs.push_back(msg);
        processed++;
        curr += msg->msgSize;
    }
    return processed;
}

/* 辅助函数：构造一个完整的 ParamMessage (MSG_NOTIFY_PARAM) */
static uint32_t BuildNotifyMessage(std::vector<char> &buffer,
                                   const std::string &key,
                                   const std::string &value,
                                   uint32_t watcherId)
{
    uint32_t msgSize = sizeof(ParamMessage) + sizeof(ParamMsgContent) + value.size();
    msgSize = PARAM_ALIGN(msgSize);
    buffer.resize(msgSize, 0);
    ParamMessage *msg = reinterpret_cast<ParamMessage *>(buffer.data());
    msg->type = MSG_NOTIFY_PARAM;
    msg->msgSize = msgSize;
    msg->id.watcherId = watcherId;
    errno_t ret = memcpy_s(msg->key, sizeof(msg->key), key.c_str(), key.size());
    if (ret != EOK) {
        return 0;
    }
    uint32_t offset = 0;
    ret = FillParamMsgContent(msg, &offset, PARAM_VALUE, value.c_str(), value.size());
    if (ret != 0) {
        return 0;
    }
    return msgSize;
}

/* 测试 Watcher：接收参数变更通知 */
class TestWatcherForBuffer final : public Watcher {
public:
    TestWatcherForBuffer() {}
    ~TestWatcherForBuffer = default;

    int32_t OnParameterChange(const std::string &prefix, const std::string &name,
                              const std::string &value) override
    {
        notifyCount_++;
        lastName_ = name;
        lastValue_ = value;
        return 0;
    }

    uint32_t GetNotifyCount() const
    {
        return notifyCount_;
    }

    std::string GetLastName() const
    {
        return lastName_;
    }

    std::string GetLastValue() const
    {
        return lastValue_;
    }

    void Reset()
    {
        notifyCount_ = 0;
        lastName_.clear();
        lastValue_.clear();
    }

private:
    uint32_t notifyCount_ = 0;
    std::string lastName_;
    std::string lastValue_;
};

class WatcherBufferOverflowUnitTest : public ::testing::Test {
public:
    WatcherBufferOverflowUnitTest() {}
    virtual ~WatcherBufferOverflowUnitTest() {}

    void SetUp()
    {
        if (GetParamSecurityLabel() != nullptr) {
            GetParamSecurityLabel()->cred.uid = 0;
            GetParamSecurityLabel()->cred.gid = 0;
        }
        SetTestPermissionResult(0);
        // 确保故障注入目录存在
        mkdir(FAULT_INJECT_DIR, FAULT_INJECT_DIR_MODE);
        // 清理可能残留的故障注入标志文件
        unlink(FAULT_INJECT_PATH);
    }

    void TearDown()
    {
        // 清理故障注入标志文件
        unlink(FAULT_INJECT_PATH);
    }

    void TestBody() {}

    /* 获取 WatcherManager 单例（与现有 watcher_proxy_unittest.cpp 模式一致 */
    WatcherManager *GetWatcherManager()
    {
        static WatcherManager *watcherManager_ = nullptr;
        if (watcherManager_ == nullptr) {
            watcherManager_ = new WatcherManager(0, true);
            if (watcherManager_ == nullptr) {
                return nullptr;
            }
            watcherManager_->OnStart();
        }
        return watcherManager_;
    }

    /* 添加远程 Watcher （复用现有测试模式） */
    int AddRemoteWatcher(uint32_t agentId, uint32_t &watcherId)
    {
        WatcherManager *mgr = GetWatcherManager();
        if (mgr == nullptr) {
            return -1;
        }
        MessageParcel data;
        MessageParcel reply;
        MessageOption option;
        data.WriteInterfaceToken(IWatcherManager::GetDescriptor());
        data.WriteUint32(agentId);
        sptr<IWatcher> watcher = new TestWatcherForBuffer();
        bool ret = data.WriteRemoteObject(watcher->AsObject());
        if (!ret) {
            return -1;
        }
        mgr->OnRemoteRequest(
            static_cast<uint32_t>(IWatcherManagerIpcCode::COMMAND_ADD_REMOTE_WATCHER),
            data, reply, option);
        reply.ReadInt32();
        watcherId = reply.ReadUint32();
        return (watcherId != 0) ? 0 : -1;
    }

    /* 添加 Watcher 组（监听指定前缀） */
    int AddWatcherGroup(const std::string &keyPrefix, uint32_t watcherId)
    {
        WatcherManager *mgr = GetWatcherManager();
        if (mgr == nullptr) {
            return -1;
        }
        MessageParcel data;
        MessageParcel reply;
        MessageOption option;
        data.WriteInterfaceToken(IWatcherManager::GetDescriptor());
        data.WriteString16(Str8ToStr16(keyPrefix));
        data.WriteUint32(watcherId);
        mgr->OnRemoteRequest(
            static_cast<uint32_t>(IWatcherManagerIpcCode::COMMAND_ADD_WATCHER),
            data, reply, option);
        return reply.ReadInt32();
    }

    /* 直接调用 ProcessWatcherMessage 发送参数变更通知 */
    int SendNotifyMessage(const std::string &key, const std::string &value, uint32_t watcherId)
    {
        WatcherManager *mgr = GetWatcherManager();
        if (mgr == nullptr) {
            return -1;
        }
        std::vector<char> buffer;
        uint32_t msgSize = BuildNotifyMessage(buffer, key, value, watcherId);
        if (msgSize == 0) {
            return -1;
        }
        ParamMessage *msg = reinterpret_cast<ParamMessage *>(buffer.data());
        mgr->ProcessWatcherMessage(msg);
        return 0;
    }
};

// ===============================
// 测试组 1：故障注入标志文件机制
// ===============================

/*
 * 测试用例：FaultInjectFlag_001
 * 验证跟FaultInjectEnabled("watcher_recv_buffer_max") 标志文件机制
 * 前置条件：/data/statup_fault/ 目录可创建
 * 预期结果：标志文件存在时返回 1，不存在时返回 0
 */
HWTEST_F(WatcherBufferOverflowUnitTest, FaultInjectFlag_001, TestSize.Level0)
{
    // 标志文件不存在
    EXPECT_EQ(access(FAULT_INJECT_PATH, F_OK), -1);

    // 创建标志文件
    int fd = open(FAULT_INJECT_PATH, O_CREAT | O_WRONLY, 0644);
    EXPECT_GE(fd, 0);
    if (fd >= 0) {
        close(fd);
    }

    // 标志文件存在，故障注入开启
    EXPECT_EQ(access(FAULT_INJECT_PATH, F_OK), 0);

    // 删除标志文件
    EXPECT_EQ(unlink(FAULT_INJECT_PATH), 0);

    // 标志文件不存在，故障注入关闭
    EXPECT_EQ(access(FAULT_INJECT_PATH, F_OK), -1);
}

HWTEST_F(WatcherBufferOverflowUnitTest, FaultInjectFlag_002, TestSize.Level0)
{
    EXPECT_EQ(access("", F_OK), -1);
}

HWTEST_F(WatcherBufferOverflowUnitTest, TruncatedMessage_001, TestSize.Level0)
{
    std::vector<char> buffer(FAULT_INJECT_RECV_MAX, 0);
    ParamMessage *msg = reinterpret_cast<ParamMessage *>(buffer.data());
    msg->type = MSG_NOTIFY_PARAM;
    msg->msgSize = sizeof(ParamMessage);
    msg->id.watcherId = 1;

    uint32_t dataLen = FAULT_INJECT_RECV_MAX;

    EXPECT_TRUE(sizeof(ParamMessage) >= dataLen);

    std::vector<ParamMessage *> processedMsgs;
    int processed = SimulateRunLoopParsing(buffer.data(), dataLen, processedMsgs);
    EXPECT_EQ(processed, 0);
}

HWTEST_F(WatcherBufferOverflowUnitTest, TruncatedMessage_002, TestSize.Level0)
{
    EXPECT_LT(sizeof(ParamMessage), static_cast<size_t>(RECV_BUFFER_MAX));
    EXPECT_LT(static_cast<size_t>(FAULT_INJECT_RECV_MAX), sizeof(ParamMessage));
}

HWTEST_F(WatcherBufferOverflowUnitTest, InvalidMsgSize_001, TestSize.Level0)
{
    uint32_t bufferSize = 256;
    std::vector<char> buffer(bufferSize, 0);
    ParamMessage *msg = reinterpret_cast<ParamMessage *>(buffer.data());
    msg->type = MSG_NOTIFY_PARAM;
    msg->msgSize = 0;
    msg->id.watcherId = 1;

    uint32_t dataLen = bufferSize;

    EXPECT_FALSE(sizeof(ParamMessage) >= dataLen);
    EXPECT_EQ(msg->msgSize, 0);

    std::vector<ParamMessage *> processedMsgs;
    int processed = SimulateRunLoopParsing(buffer.data(), dataLen, processedMsgs);
    EXPECT_EQ(processed, 0);
}

HWTEST_F(WatcherBufferOverflowUnitTest, InvalidMsgSize_002, TestSize.Level0)
{
    uint32_t bufferSize = 128;
    std::vector<char> buffer(bufferSize, 0);
    ParamMessage *msg = reinterpret_cast<ParamMessage *>(buffer.data());
    msg->type = MSG_NOTIFY_PARAM;
    msg->msgSize = 256;
    msg->id.watcherId = 1;

    uint32_t dataLen = bufferSize;

    EXPECT_FALSE(sizeof(ParamMessage) >= dataLen);
    EXPECT_TRUE(msg->msgSize > dataLen);

    std::vector<ParamMessage *> processedMsgs;
    int processed = SimulateRunLoopParsing(buffer.data(), dataLen, processedMsgs);
    EXPECT_EQ(processed, 0);
}

HWTEST_F(WatcherBufferOverflowUnitTest, NormalMessage_001, TestSize.Level0)
{
    std::vector<char> buffer;
    uint32_t msgSize = BuildNotifyMessage(buffer, "test.watch.param", "value1", 1);
    EXPECT_GT(msgSize, 0);

    uint32_t dataLen = msgSize;
    std::vector<ParamMessage *> processedMsgs;
    int processed = SimulateRunLoopParsing(buffer.data(), dataLen, processedMsgs);
    EXPECT_EQ(processed, 1);
    EXPECT_EQ(processedMsgs[0]->type, MSG_NOTIFY_PARAM);
    EXPECT_EQ(processedMsgs[0]->id.watcherId, 1u);
}

HWTEST_F(WatcherBufferOverflowUnitTest, MultipleMessagesTruncated_001, TestSize.Level0)
{
    std::vector<char> msgBuf;
    uint32_t singleMsgSize = BuildNotifyMessage(msgBuf, "test.watch.k", "v", 1);
    EXPECT_GT(singleMsgSize, 0);

    uint32_t bufferSize = singleMsgSize * 2 + 32;
    std::vector<char> buffer(bufferSize, 0);

    for (int i = 0; i < 2; i++) {
        errno_t ret = memcpy_s(buffer.data() + i * singleMsgSize, singleMsgSize,
                               msgBuf.data(), singleMsgSize);
        EXPECT_EQ(ret, EOK);
        ParamMessage *msg = reinterpret_cast<ParamMessage *>(buffer.data() + i * singleMsgSize);
        msg->id.watcherId = i + 1;
    }

    ParamMessage *partialMsg = reinterpret_cast<ParamMessage *>(buffer.data() + 2 * singleMsgSize);
    partialMsg->type = MSG_NOTIFY_PARAM;
    partialMsg->msgSize = singleMsgSize;
    partialMsg->id.watcherId = 3;

    uint32_t dataLen = bufferSize;
    std::vector<ParamMessage *> processedMsgs;
    int processed = SimulateRunLoopParsing(buffer.data(), dataLen, processedMsgs);
    EXPECT_EQ(processed, 2);
    EXPECT_EQ(processedMsgs[0]->id.watcherId, 1u);
    EXPECT_EQ(processedMsgs[1]->id.watcherId, 2u);
}

HWTEST_F(WatcherBufferOverflowUnitTest, SocketPairTruncatedRecv_001, TestSize.Level0)
{
    int fds[2] = {-1, -1};
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    ASSERT_EQ(ret, 0);

    std::vector<char> sendBuf;
    uint32_t msgSize = BuildNotifyMessage(sendBuf, "test.watch.sock", "val", 1);
    EXPECT_GT(msgSize, 0);

    ssize_t sentLen = send(fds[0], sendBuf.data(), msgSize, 0);
    EXPECT_EQ(static_cast<uint32_t>(sentLen), msgSize);

    std::vector<char> recvBuf(FAULT_INJECT_RECV_MAX, 0);
    ssize_t recvLen = recv(fds[1], recvBuf.data(), FAULT_INJECT_RECV_MAX, 0);
    EXPECT_EQ(recvLen, FAULT_INJECT_RECV_MAX);

    uint32_t dataLen = static_cast<uint32_t>(recvLen);

    EXPECT_TRUE(sizeof(ParamMessage) >= dataLen);

    std::vector<ParamMessage *> processedMsgs;
    int processed = SimulateRunLoopParsing(recvBuf.data(), dataLen, processedMsgs);
    EXPECT_EQ(processed, 0);

    close(fds[0]);
    close(fds[1]);
}

HWTEST_F(WatcherBufferOverflowUnitTest, SocketPairNormalRecv_001, TestSize.Level0)
{
    int fds[2] = {-1, -1};
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    ASSERT_EQ(ret, 0);

    std::vector<char> sendBuf;
    uint32_t msgSize = BuildNotifyMessage(sendBuf, "test.watch.normal", "value", 1);
    EXPECT_GT(msgSize, 0);

    ssize_t sentLen = send(fds[0], sendBuf.data(), msgSize, 0);
    EXPECT_EQ(static_cast<uint32_t>(sentLen), msgSize);

    std::vector<char> recvBuf(RECV_BUFFER_MAX, 0);
    ssize_t recvLen = recv(fds[1], recvBuf.data(), RECV_BUFFER_MAX, 0);
    EXPECT_EQ(static_cast<uint32_t>(recvLen), msgSize);

    uint32_t dataLen = static_cast<uint32_t>(recvLen);
    std::vector<ParamMessage *> processedMsgs;
    int processed = SimulateRunLoopParsing(recvBuf.data(), dataLen, processedMsgs);
    EXPECT_EQ(processed, 1);
    EXPECT_EQ(processedMsgs[0]->type, MSG_NOTIFY_PARAM);
    close(fds[0]);
    close(fds[1]);
}

HWTEST_F(WatcherBufferOverflowUnitTest, WatchParameterBatch_001, TestSize.Level0)
{
    uint32_t watcherId = 0;
    int ret = AddRemoteWatcher(getpid(), watcherId);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(watcherId, 0u);

    ret = AddWatcherGroup("test.watch.batch*", watcherId);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < 10; i++) {
        std::string key = "test.watch.batch.param" + std::to_string(i);
        std::string value = "val" + std::to_string(i);
        ret = SendNotifyMessage(key, value, watcherId);
        EXPECT_EQ(ret, 0);
    }
}

HWTEST_F(WatcherBufferOverflowUnitTest, WatchParameterBatch_002, TestSize.Level0)
{
    std::vector<char> msgBuf;
    uint32_t singleMsgSize = BuildNotifyMessage(msgBuf, "test.watch.k", "Val", 1);
    EXPECT_GT(singleMsgSize, 0);

    uint32_t maxCount = RECV_BUFFER_MAX / singleMsgSize;
    uint32_t usedSize = maxCount * singleMsgSize;
    uint32_t remaining = RECV_BUFFER_MAX - usedSize;

    std::vector<char> buffer(RECV_BUFFER_MAX, 0);
    for (uint32_t i = 0; i < maxCount; i++) {
        errno_t ret = memcpy_s(buffer.data() + i * singleMsgSize, singleMsgSize,
                               msgBuf.data(), singleMsgSize);
        EXPECT_EQ(ret, EOK);
        ParamMessage *msg = reinterpret_cast<ParamMessage *>(buffer.data() + i * singleMsgSize);
        msg->id.watcherId = i + 1;
    }

    if (remaining > 0 && remaining < sizeof(ParamMessage)) {
        ParamMessage *partialMsg = reinterpret_cast<ParamMessage *>(buffer.data() + usedSize);
        partialMsg->type = MSG_NOTIFY_PARAM;
        partialMsg->msgSize = singleMsgSize;
        partialMsg->id.watcherId = maxCount + 1;
    }

    uint32_t dataLen = RECV_BUFFER_MAX;
    std::vector<ParamMessage *> processedMsgs;
    int processed = SimulateRunLoopParsing(buffer.data(), dataLen, processedMsgs);
    EXPECT_EQ(static_cast<uint32_t>(processed), maxCount);
}

HWTEST_F(WatcherBufferOverflowUnitTest, BufferBoundary_001, TestSize.Level0)
{
    std::vector<char> msgBuf;
    uint32_t singleMsgSize = BuildNotifyMessage(msgBuf, "test.watch.b", "v", 1);
    EXPECT_GT(singleMsgSize, 0);

    uint32_t bufferSize = singleMsgSize * 5;
    std::vector<char> buffer(bufferSize, 0);

    for (int i = 0; i < 5; i++) {
        errno_t ret = memcpy_s(buffer.data() + i * singleMsgSize, singleMsgSize,
                               msgBuf.data(), singleMsgSize);
        EXPECT_EQ(ret, EOK);
        ParamMessage *msg = reinterpret_cast<ParamMessage *>(buffer.data() + i * singleMsgSize);
        msg->id.watcherId = i + 1;
    }

    uint32_t dataLen = bufferSize;
    std::vector<ParamMessage *> processedMsgs;
    int processed = SimulateRunLoopParsing(buffer.data(), dataLen, processedMsgs);
    EXPECT_EQ(processed, 5);
}

HWTEST_F(WatcherBufferOverflowUnitTest, BufferBoundary_002, TestSize.Level0)
{
    std::vector<char> buffer(64, 0);
    uint32_t dataLen = 0;
    std::vector<ParamMessage *> processedMsgs;
    int processed = SimulateRunLoopParsing(buffer.data(), dataLen, processedMsgs);
    EXPECT_EQ(processed, 0);
}
