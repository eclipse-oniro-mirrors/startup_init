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

#include <cinttypes>
#include <sys/mount.h>
#include "fs_manager/fs_manager.h"
#include "init_log.h"
#include "init_param.h"
#include "param_stub.h"
#include "securec.h"
#include "systemcapability.h"
#include "service_control.h"
#include "control_fd.h"
#include "loop_event.h"
#include "fd_holder.h"
#include "fd_holder_internal.h"

using namespace testing::ext;
using namespace std;

namespace init_ut {

extern "C" {
void CmdDisConnectComplete(const TaskHandle client);
void CmdOnSendMessageComplete(const TaskHandle task, const BufferHandle handle);
void CmdOnClose(const TaskHandle task);
void CmdOnConnectComplete(const TaskHandle client);
void CmdOnRecvMessage(const TaskHandle task, const uint8_t *buffer, uint32_t buffLen);
int InitPtyInterface(CmdAgent *agent, uint16_t type, const char *cmd);
void ProcessPtyRead(const WatcherHandle taskHandle, int fd, uint32_t *events, const void *context);
void ProcessPtyWrite(const WatcherHandle taskHandle, int fd, uint32_t *events, const void *context);
int CmdOnIncommingConnect(const LoopHandle loop, const TaskHandle server);
CmdAgent *CmdAgentCreate(const char *server);
void CmdClientOnRecvMessage(const TaskHandle task, const uint8_t *buffer, uint32_t buffLen);
int SendCmdMessage(const CmdAgent *agent, uint16_t type, const char *cmd, const char *ptyName);
int SendMessage(LoopHandle loop, TaskHandle task, const char *message);
int *GetFdsFromMsg(size_t *outFdCount, pid_t *requestPid, struct msghdr msghdr);
int BuildSendData(char *buffer, size_t size, const char *serviceName, bool hold, bool poll);
}

class InnerkitsUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

/**
* @tc.name: ReadFstabFromFile_unitest
* @tc.desc: read fstab from test file.
* @tc.type: FUNC
* @tc.require:
* @tc.author:
*/
HWTEST_F(InnerkitsUnitTest, ReadFstabFromFile_unitest, TestSize.Level1)
{
    Fstab *fstab = nullptr;
    const std::string fstabFile1 = "/data/fstab.updater1";
    fstab = ReadFstabFromFile(fstabFile1.c_str(), false);
    EXPECT_EQ(fstab, nullptr);
    const std::string fstabFile2 = STARTUP_INIT_UT_PATH"/mount_unitest/ReadFstabFromFile1.fstable";
    fstab = ReadFstabFromFile(fstabFile2.c_str(), false);
    EXPECT_NE(fstab, nullptr);
    ParseFstabPerLine(const_cast<char *>("test"), fstab, true, nullptr);
    ReleaseFstab(fstab);
}

/**
* @tc.name: FindFstabItemForPath_unitest
* @tc.desc: read fstab from test file, then find item according to path.
* @tc.type: FUNC
* @tc.require:
* @tc.author:
*/
HWTEST_F(InnerkitsUnitTest, FindFstabItemForPath_unitest, TestSize.Level1)
{
    const std::string fstabFile1 = STARTUP_INIT_UT_PATH"/mount_unitest/ReadFstabFromFile1.fstable";
    Fstab *fstab = nullptr;
    fstab = ReadFstabFromFile(fstabFile1.c_str(), false);
    ASSERT_NE(fstab, nullptr);
    FstabItem* item = nullptr;
    const std::string path1 = "";
    item = FindFstabItemForPath(*fstab, path1.c_str());
    if (item == nullptr) {
        SUCCEED();
    }
    const std::string path2 = "/data";
    item = FindFstabItemForPath(*fstab, path2.c_str());
    if (item != nullptr) {
        SUCCEED();
    }
    const std::string path3 = "/data2";
    item = FindFstabItemForPath(*fstab, path3.c_str());
    if (item == nullptr) {
        SUCCEED();
    }
    const std::string path4 = "/data2/test";
    item = FindFstabItemForPath(*fstab, path4.c_str());
    if (item != nullptr) {
        SUCCEED();
    }
    ReleaseFstab(fstab);
    fstab = nullptr;
}

/**
* @tc.name: FindFstabItemForMountPoint_unitest
* @tc.desc: read fstab from test file, then find item that matches with the mount point.
* @tc.type: FUNC
* @tc.require:
* @tc.author:
*/
HWTEST_F(InnerkitsUnitTest, FindFstabItemForMountPoint_unitest, TestSize.Level1)
{
    const std::string fstabFile1 = STARTUP_INIT_UT_PATH"/mount_unitest/ReadFstabFromFile1.fstable";
    Fstab *fstab = nullptr;
    fstab = ReadFstabFromFile(fstabFile1.c_str(), false);
    ASSERT_NE(fstab, nullptr);
    FstabItem* item = nullptr;
    const std::string mp1 = "/data";
    const std::string mp2 = "/data2";
    item = FindFstabItemForMountPoint(*fstab, mp2.c_str());
    if (item == nullptr) {
        SUCCEED();
    }
    const std::string mp3 = "/data";
    item = FindFstabItemForMountPoint(*fstab, mp3.c_str());
    if (item != nullptr) {
        SUCCEED();
    }
    ReleaseFstab(fstab);
    fstab = nullptr;
}

/**
* @tc.name: GetMountFlags_unitest
* @tc.desc: read fstab from test file, then find the item and get mount flags.
* @tc.type: FUNC
* @tc.require:
* @tc.author:
*/
HWTEST_F(InnerkitsUnitTest, GetMountFlags_unitest, TestSize.Level1)
{
    const std::string fstabFile1 = STARTUP_INIT_UT_PATH"/mount_unitest/ReadFstabFromFile1.fstable";
    Fstab *fstab = nullptr;
    fstab = ReadFstabFromFile(fstabFile1.c_str(), true);
    ASSERT_NE(fstab, nullptr);
    struct FstabItem* item = nullptr;
    const std::string mp = "/hos";
    item = FindFstabItemForMountPoint(*fstab, mp.c_str());
    if (item == nullptr) {
        SUCCEED();
    }
    const int bufferSize = 512;
    char fsSpecificOptions[bufferSize] = {0};
    unsigned long flags = GetMountFlags(item->mountOptions, fsSpecificOptions, bufferSize, item->mountPoint);
    EXPECT_EQ(flags, static_cast<unsigned long>(MS_NOSUID | MS_NODEV | MS_NOATIME));
    ReleaseFstab(fstab);
    fstab = nullptr;
}

/**
* @tc.name: GetSlotInfo_unittest
* @tc.desc: get the number of slots and get current slot.
* @tc.type: FUNC
* @tc.require:issueI5NTX2
* @tc.author:
*/
HWTEST_F(InnerkitsUnitTest, GetSlotInfo_unittest, TestSize.Level1)
{
    EXPECT_NE(GetBootSlots(), -1);
    EXPECT_NE(GetCurrentSlot(), -1);
}

/**
* @tc.name: LoadFstabFromCommandLine_unittest
* @tc.desc: get fstab from command line.
* @tc.type: FUNC
* @tc.require:issueI5NTX2
* @tc.author:
*/
HWTEST_F(InnerkitsUnitTest, LoadFstabFromCommandLine_unittest, TestSize.Level1)
{
    EXPECT_NE(LoadFstabFromCommandLine(), (Fstab *)NULL);
}

/**
* @tc.name: GetBlockDevicePath_unittest
* @tc.desc: get block device path according to valid or invalid partition.
* @tc.type: FUNC
* @tc.require:issueI5NTX2
* @tc.author:
*/
HWTEST_F(InnerkitsUnitTest, GetBlockDevicePath_unittest, TestSize.Level1)
{
    char devicePath[MAX_BUFFER_LEN] = {0};
    EXPECT_EQ(GetBlockDevicePath("/vendor", devicePath, MAX_BUFFER_LEN), 0);
    EXPECT_EQ(GetBlockDevicePath("/misc", devicePath, MAX_BUFFER_LEN), 0);
    EXPECT_EQ(GetBlockDevicePath("/invalid", devicePath, MAX_BUFFER_LEN), -1);
    unlink(BOOT_CMD_LINE);
    EXPECT_EQ(GetBlockDevicePath("/invalid", devicePath, MAX_BUFFER_LEN), -1);
    GetCurrentSlot();
}

/**
* @tc.name: DoFormat_unittest
* @tc.desc: format file system, includes ext4 and f2fs type.
* @tc.type: FUNC
* @tc.require:
* @tc.author:
*/
HWTEST_F(InnerkitsUnitTest, DoFormat_unittest, TestSize.Level1)
{
    EXPECT_NE(DoFormat("/testpath", "ext4"), -1);
    EXPECT_NE(DoFormat("/testpath", "f2fs"), -1);
    EXPECT_EQ(DoFormat("/testpath", "notFs"), -1);
    EXPECT_EQ(DoFormat(nullptr, nullptr), -1);
}

/**
* @tc.name: MountAllWithFstabFile_unittest
* @tc.desc: mount partitions according to fstab that read from file.
* @tc.type: FUNC
* @tc.require:issueI5NTX2
* @tc.author:
*/
HWTEST_F(InnerkitsUnitTest, MountAllWithFstabFile_unittest, TestSize.Level1)
{
    EXPECT_NE(MountAllWithFstabFile(STARTUP_INIT_UT_PATH"/etc/fstab.required", 0), 1);
    EXPECT_NE(UmountAllWithFstabFile(STARTUP_INIT_UT_PATH"/etc/fstab.required"), 1);
    EXPECT_EQ(MountAllWithFstabFile("/testErrorFile", 0), -1);
    EXPECT_EQ(MountAllWithFstabFile(nullptr, 0), -1);
    EXPECT_EQ(GetMountStatusForMountPoint(nullptr), -1);
    FstabItem fstabItem;
    fstabItem.fsType = strdup("notSupport");
    EXPECT_EQ(MountOneItem(nullptr), -1);
    EXPECT_EQ(MountOneItem(&fstabItem), 0);
    if (fstabItem.fsType != nullptr) {
        free(fstabItem.fsType);
        fstabItem.fsType = nullptr;
    }
}

#define SYSCAP_MAX_SIZE 100

// TestSysCap
HWTEST_F(InnerkitsUnitTest, TestSysCap, TestSize.Level1)
{
    bool ret = HasSystemCapability("test.cap");
    EXPECT_EQ(ret, false);
    ret = HasSystemCapability(nullptr);
    EXPECT_EQ(ret, false);
    ret = HasSystemCapability("ArkUI.ArkUI.Napi");
    EXPECT_EQ(ret, true);
    ret = HasSystemCapability("SystemCapability.ArkUI.ArkUI.Napi");
    EXPECT_EQ(ret, true);
    char *wrongName = reinterpret_cast<char *>(malloc(SYSCAP_MAX_SIZE));
    ASSERT_NE(wrongName, nullptr);
    EXPECT_EQ(memset_s(wrongName, SYSCAP_MAX_SIZE, 1, SYSCAP_MAX_SIZE), 0);
    HasSystemCapability(wrongName);
    free(wrongName);
}

// TestControlService
HWTEST_F(InnerkitsUnitTest, TestControlService, TestSize.Level1)
{
    TestSetParamCheckResult("startup.service.ctl.", 0777, 0);
    ServiceControl("deviceinfoservice", START);
    SystemWriteParam("startup.service.ctl.deviceinfoservice", "2");
    ServiceControl("deviceinfoservice", RESTART);
    ServiceControl("deviceinfoservice", STOP);
    SystemWriteParam("startup.service.ctl.deviceinfoservice", "0");
    ServiceControl("param_watcher", RESTART);
    EXPECT_EQ(ServiceControl(nullptr, RESTART), -1);
    const char *argv[] = {"testArg"};
    ServiceControlWithExtra("deviceinfoservice", RESTART, argv, 1);
    ServiceControlWithExtra(nullptr, RESTART, argv, 1);
    ServiceControlWithExtra(nullptr, 3, argv, 1); // 3 is action
    ServiceControlWithExtra("notservie", RESTART, argv, 1);
    ServiceControlWithExtra("deviceinfoservice", 3, argv, 1); // 3 is action
    ServiceSetReady("deviceinfoservice");
    ServiceSetReady(nullptr);
    ServiceWaitForStatus("deviceinfoservice", SERVICE_READY, 1);
    ServiceWaitForStatus("deviceinfoservice", SERVICE_READY, -1);
    ServiceWaitForStatus(nullptr, SERVICE_READY, 1);
    StartServiceByTimer("deviceinfoservice", 1);
    StartServiceByTimer("deviceinfoservice", 0);
    StartServiceByTimer(nullptr, 0);
    StopServiceTimer("deviceinfoservice");
}

static int TestIncommingConnect(const LoopHandle loop, const TaskHandle server)
{
    UNUSED(loop);
    UNUSED(server);
    return 0;
}

// TestControlFd
HWTEST_F(InnerkitsUnitTest, TestControlFd, TestSize.Level1)
{
    CmdClientInit("/data/testSock1", ACTION_DUMP, "cmd");
    CmdClientInit("/data/testSock1", ACTION_DUMP, "cmd");
    CmdClientInit(INIT_CONTROL_FD_SOCKET_PATH, ACTION_DUMP, nullptr);
    CmdClientInit(nullptr, ACTION_DUMP, "cmd");
    CmdDisConnectComplete(nullptr);
    CmdOnSendMessageComplete(nullptr, nullptr);
    CmdOnConnectComplete(nullptr);
    CmdClientOnRecvMessage(nullptr, nullptr, 0);
    CmdAgentCreate(nullptr);
    CmdAgent *agent = CmdAgentCreate(INIT_CONTROL_FD_SOCKET_PATH);
    EXPECT_NE(agent, nullptr);
    SendCmdMessage(agent, ACTION_DUMP, "cmd", "test");
    SendCmdMessage(agent, ACTION_DUMP, "cmd", nullptr);
    SendMessage(nullptr, nullptr, nullptr);
    uint32_t events = 0;
    InitPtyInterface(agent, 0, "cmd");
    InitPtyInterface(agent, 0, nullptr);
    InitPtyInterface(nullptr, 0, nullptr);
    mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    CheckAndCreatFile("/data/init_ut/testInput", mode);
    int fd = open("/data/init_ut/testInput", O_RDWR);
    perror("write failed");
    EXPECT_GT(fd, 0);
    EXPECT_GT(write(fd, "test", strlen("test")), 0);
    perror("write failed");
    lseek(fd, 0, SEEK_SET);
    ProcessPtyRead(nullptr, fd, &events, (void *)agent);
    ProcessPtyRead(nullptr, fd, &events, (void *)agent);
    ProcessPtyRead(nullptr, STDERR_FILENO, &events, nullptr);
    lseek(fd, 0, SEEK_SET);
    ProcessPtyWrite(nullptr, fd, &events, (void *)agent);
    ProcessPtyWrite(nullptr, fd, &events, (void *)agent);
    ProcessPtyWrite(nullptr, STDERR_FILENO, &events, nullptr);
    close(fd);

    CmdOnClose(agent->task);
}

HWTEST_F(InnerkitsUnitTest, TestControlFdServer, TestSize.Level1)
{
    CmdServiceInit(nullptr, nullptr);
    CmdServiceInit("/data/testSock1", [](uint16_t type, const char *serviceCmd, const void *context) {
        UNUSED(type);
        UNUSED(serviceCmd);
        UNUSED(context);
        });

    TaskHandle testServer;
    LE_StreamServerInfo info = {};
    info.baseInfo.flags = TASK_STREAM | TASK_SERVER | TASK_PIPE | TASK_TEST;
    info.server = (char *)"/data/testSock1";
    info.socketId = -1;
    info.baseInfo.close = NULL;
    info.disConnectComplete = NULL;
    info.incommingConnect = TestIncommingConnect;
    info.sendMessageComplete = NULL;
    info.recvMessage = NULL;
    (void)LE_CreateStreamServer(LE_GetDefaultLoop(), &testServer, &info);
    CmdOnIncommingConnect(LE_GetDefaultLoop(), testServer);

    CmdOnRecvMessage(testServer, nullptr, 0);
    CmdMessage *cmdMsg = (CmdMessage *)malloc(sizeof(CmdMessage) + strlen("test"));
    cmdMsg->type = ACTION_DUMP;
    cmdMsg->ptyName[0] = '\0';;
    CmdOnRecvMessage(testServer, (uint8_t *)(&cmdMsg), 0);
    cmdMsg->type = ACTION_DUMP;
    cmdMsg->cmd[0] = 'a';
    cmdMsg->ptyName[0] = 'a';
    CmdOnRecvMessage(testServer, (uint8_t *)(&cmdMsg), 0);
    CmdServiceProcessDelClient(0);
    CmdServiceProcessDelClient(0);
    free(cmdMsg);
}

HWTEST_F(InnerkitsUnitTest, TestHoldFd, TestSize.Level1)
{
    int fds1[] = {1, 0};
    ServiceSaveFd("testServiceName", fds1, ARRAY_LENGTH(fds1));
    ServiceSaveFd(nullptr, fds1, ARRAY_LENGTH(fds1));
    ServiceSaveFdWithPoll("testServiceName", fds1, 0);
    ServiceSaveFdWithPoll(nullptr, fds1, 0);
    ServiceSaveFdWithPoll("testServiceName", fds1, ARRAY_LENGTH(fds1));
    EXPECT_EQ(setenv("OHOS_FD_HOLD_testServiceName", "1 0", 0), 0);

    size_t fdCount = 0;
    int *fds = nullptr;
    ServiceGetFd("testService", nullptr);
    ServiceGetFd("testService", &fdCount);
    char *wrongName = (char *)malloc(MAX_FD_HOLDER_BUFFER + 1);
    ASSERT_NE(wrongName, nullptr);
    EXPECT_EQ(memset_s(wrongName, MAX_FD_HOLDER_BUFFER + 1, 1, MAX_FD_HOLDER_BUFFER + 1), 0);
    ServiceGetFd(wrongName, &fdCount);
    BuildSendData(wrongName, 1, "testService", 0, 1);
    BuildSendData(wrongName, 1, "testService", 0, 0);
    BuildSendData(nullptr, 1, "testService", 0, 0);
    free(wrongName);

    fds = ServiceGetFd("testServiceName", &fdCount);
    EXPECT_NE(fds, nullptr);
    struct msghdr msghdr;
    BuildControlMessage(nullptr, nullptr, 1, 0);
    BuildControlMessage(&msghdr, nullptr, 1, 0);
    BuildControlMessage(&msghdr, fds, -1, 0);
    BuildControlMessage(&msghdr, fds, -1, 1);

    if (fds != nullptr)
    {
        free(fds);
        fds = nullptr;
    }
}

HWTEST_F(InnerkitsUnitTest, TestHoldFd2, TestSize.Level1)
{
    size_t fdCount = 0;
    int *fds = nullptr;
    char buffer[MAX_FD_HOLDER_BUFFER + 1] = {};
    pid_t requestPid = -1;
    struct msghdr msghdr;
    GetFdsFromMsg(&fdCount, &requestPid, msghdr);
    msghdr.msg_flags = MSG_TRUNC;
    GetFdsFromMsg(&fdCount, &requestPid, msghdr);
    struct iovec iovec = {
        .iov_base = buffer,
        .iov_len = MAX_FD_HOLDER_BUFFER,
    };
    ReceiveFds(0, iovec, &fdCount, false, &requestPid);
    fds = ReceiveFds(0, iovec, &fdCount, true, &requestPid);
    if (fds != nullptr)
    {
        free(fds);
        fds = nullptr;
    }
    if (msghdr.msg_control != nullptr) {
        free(msghdr.msg_control);
    }
}

} // namespace init_ut
