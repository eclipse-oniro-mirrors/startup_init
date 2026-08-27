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

#include <cerrno>
#include <cstdint>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include "func_wrapper.h"
#include "init_cmds.h"
#include "init_param.h"
#include "init_group_manager.h"
#include "param_stub.h"
#include "init_utils.h"
#include "trigger_manager.h"

using namespace testing::ext;
using namespace std;
#define ESWAP_ENABLE_PATH "/proc/sys/kernel/hyperhold/enable"
#define HP_ENABLE_BUFFER_SIZE 30
#define DISABLE_ESWAP "disable"
#define CLOSE_HP_WAIT_TIME 500000
#define CLOSE_HP_INTERVAL_WAIT 10000
#define DMA_DEVICE_FILE "/dev/dma_reclaim"
#define GPU_RECLAIM_IMPL_SO "libgpu_reclaim.so"
#define ENTERPRISE_SPACE_TEST_FD 101

static constexpr char ENTERPRISE_SPACE_TEST_DEV[] = "/dev/access_token_id";
static constexpr unsigned long ENTERPRISE_SPACE_TEST_GET_REQUEST = _IOR('A', 29, std::uint8_t);
static constexpr unsigned long ENTERPRISE_SPACE_TEST_SET_REQUEST = _IOW('A', 30, std::uint8_t);

static int g_enterpriseSpaceOpenResult = ENTERPRISE_SPACE_TEST_FD;
static int g_enterpriseSpaceSetResult = 0;
static int g_enterpriseSpaceGetResult = 0;
static int g_enterpriseSpaceOpenCount = 0;
static int g_enterpriseSpaceIoctlCount = 0;
static int g_enterpriseSpaceCloseCount = 0;
static int g_enterpriseSpaceLastFlags = 0;
static int g_enterpriseSpaceLastFd = -1;
static bool g_enterpriseSpaceSetFlag = false;
static bool g_enterpriseSpaceReadbackFlag = false;
static bool g_enterpriseSpaceUseCustomReadback = false;
static unsigned long g_enterpriseSpaceRequests[2] = { 0 };

static int EnterpriseSpaceOpenMock(const char *path, int flags)
{
    EXPECT_STREQ(path, ENTERPRISE_SPACE_TEST_DEV);
    g_enterpriseSpaceOpenCount++;
    g_enterpriseSpaceLastFlags = flags;
    if (g_enterpriseSpaceOpenResult < 0) {
        errno = ENOENT;
    }
    return g_enterpriseSpaceOpenResult;
}

static int EnterpriseSpaceIoctlMock(int fd, int request, va_list args)
{
    int requestIndex = g_enterpriseSpaceIoctlCount++;
    g_enterpriseSpaceLastFd = fd;
    unsigned long requestCode = static_cast<unsigned int>(request);
    if (requestIndex < static_cast<int>(ARRAY_LENGTH(g_enterpriseSpaceRequests))) {
        g_enterpriseSpaceRequests[requestIndex] = requestCode;
    }
    if (requestCode == ENTERPRISE_SPACE_TEST_SET_REQUEST) {
        bool *flag = va_arg(args, bool *);
        EXPECT_NE(flag, nullptr);
        if (g_enterpriseSpaceSetResult != 0) {
            errno = ENOTTY;
            return g_enterpriseSpaceSetResult;
        }
        g_enterpriseSpaceSetFlag = *flag;
        return 0;
    }
    if (requestCode == ENTERPRISE_SPACE_TEST_GET_REQUEST) {
        bool *flag = va_arg(args, bool *);
        EXPECT_NE(flag, nullptr);
        if (g_enterpriseSpaceGetResult != 0) {
            errno = ENOTTY;
            return g_enterpriseSpaceGetResult;
        }
        *flag = g_enterpriseSpaceUseCustomReadback ? g_enterpriseSpaceReadbackFlag : g_enterpriseSpaceSetFlag;
        return 0;
    }
    errno = EINVAL;
    return -1;
}

static int EnterpriseSpaceCloseMock(int fd)
{
    g_enterpriseSpaceCloseCount++;
    g_enterpriseSpaceLastFd = fd;
    return 0;
}

static void ResetEnterpriseSpaceIoMock()
{
    g_enterpriseSpaceOpenResult = ENTERPRISE_SPACE_TEST_FD;
    g_enterpriseSpaceSetResult = 0;
    g_enterpriseSpaceGetResult = 0;
    g_enterpriseSpaceOpenCount = 0;
    g_enterpriseSpaceIoctlCount = 0;
    g_enterpriseSpaceCloseCount = 0;
    g_enterpriseSpaceLastFlags = 0;
    g_enterpriseSpaceLastFd = -1;
    g_enterpriseSpaceSetFlag = false;
    g_enterpriseSpaceReadbackFlag = false;
    g_enterpriseSpaceUseCustomReadback = false;
    g_enterpriseSpaceRequests[0] = 0;
    g_enterpriseSpaceRequests[1] = 0;
}

static void EnableEnterpriseSpaceIoMock()
{
    UpdateOpenFunc(EnterpriseSpaceOpenMock);
    UpdateIoctlFunc(EnterpriseSpaceIoctlMock);
    UpdateCloseFunc(EnterpriseSpaceCloseMock);
}

static void ExecuteSetSpaceFlagCommand()
{
    int cmdIndex = -1;
    ASSERT_NE(GetMatchCmd("SetSpaceFlag ", &cmdIndex), nullptr);
    DoCmdByIndex(cmdIndex, "true", nullptr);
}

static int DoCmdByName(const char *name, const char *cmdContent)
{
    int cmdIndex = 0;
    (void)GetMatchCmd(name, &cmdIndex);
    DoCmdByIndex(cmdIndex, cmdContent, nullptr);
    return 0;
}

char *LoadStringFromFile(const char *filePath)
{
    FILE *file = fopen(filePath, "r");
    if (!file) {
    return nullptr;
    }

    static char buffer[HP_ENABLE_BUFFER_SIZE] = {0};
    if (fgets(buffer, sizeof(buffer) - 1, file) == NULL) {
    (void)fclose(file);
    return nullptr;
    }

    (void)fclose(file);
    size_t index = strcspn(buffer, "\n");
    if (index < sizeof(buffer)) {
    buffer[index] = '\0';
    }

    return strdup(buffer);
}

namespace init_ut {
class CmdsUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() override
    {
        ResetEnterpriseSpaceIoMock();
    }
    void TearDown() override
    {
        UpdateOpenFunc(nullptr);
        UpdateIoctlFunc(nullptr);
        UpdateCloseFunc(nullptr);
    }
};

HWTEST_F(CmdsUnitTest, TestCmdExecByName1, TestSize.Level1)
{
    int ret = DoCmdByName("timer_start ", "media_service|5000");
    ret = DoCmdByName("timer_start ", "|5000");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("timer_stop ", "media_service");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("exec ", "media_service");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("syncexec ", "/system/bin/toybox");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("load_access_token_id ", "media_service");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("load_access_token_id ", "");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("stopAllServices ", "false");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("stopAllServices ", "true");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("umount ", "/2222222");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("mount ", "/2222222");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("mount ", "ext4 /2222222 /data wait filecrypt=555");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("umount ", "/2222222");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("init_global_key ", "/data");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("init_global_key ", "arg0 arg1");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("init_main_user ", "testUser");
    EXPECT_EQ(ret, 0);
}
HWTEST_F(CmdsUnitTest, TestCmdExecByName2, TestSize.Level1)
{
    int ret = DoCmdByName("init_main_user ", nullptr);
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("mkswap ", "/data/init_ut");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("swapon ", "/data/init_ut");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("sync ", "");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("restorecon ", "");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("restorecon ", "/data  /data");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("suspend ", "");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("wait ", "1");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("wait ", "aaa 1");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("mksandbox", "/sandbox");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("mount_fstab ", "/2222222");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("umount_fstab ", "/2222222");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("mknode  ", "node1 node1 node1 node1 node1");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("makedev ", "/device1 device2");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("symlink ", "/xxx/xxx/xxx1 /xxx/xxx/xxx2");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("load_param ", "aaa onlyadd");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("load_persist_params ", "");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("load_param ", "");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("setparam ", "bbb 0");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("ifup ", "aaa, bbb");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("insmod ", "a b");
    EXPECT_EQ(ret, 0);
    ret = DoCmdByName("insmod ", "/data /data");
}

/*
 * @tc.name: SetSpaceFlag_001
 * @tc.desc: Verify command registration and successful enterprise space SET/GET validation.
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, SetSpaceFlag_001, TestSize.Level1)
{
    int cmdIndex = -1;
    const char *cmd = GetMatchCmd("SetSpaceFlag ", &cmdIndex);
    ASSERT_NE(cmd, nullptr);
    EXPECT_STREQ(cmd, "SetSpaceFlag ");
    EXPECT_GE(cmdIndex, 0);

    EnableEnterpriseSpaceIoMock();
    DoCmdByIndex(cmdIndex, "true", nullptr);
    EXPECT_EQ(g_enterpriseSpaceOpenCount, 1);
    EXPECT_EQ(g_enterpriseSpaceIoctlCount, 2);
    EXPECT_EQ(g_enterpriseSpaceCloseCount, 1);
    EXPECT_EQ(g_enterpriseSpaceLastFlags, O_RDWR | O_CLOEXEC);
    EXPECT_EQ(g_enterpriseSpaceLastFd, ENTERPRISE_SPACE_TEST_FD);
    EXPECT_EQ(g_enterpriseSpaceRequests[0], ENTERPRISE_SPACE_TEST_SET_REQUEST);
    EXPECT_EQ(g_enterpriseSpaceRequests[1], ENTERPRISE_SPACE_TEST_GET_REQUEST);
    EXPECT_TRUE(g_enterpriseSpaceSetFlag);
}

/*
 * @tc.name: SetSpaceFlag_002
 * @tc.desc: Verify false clears the enterprise space flag and validates the readback value.
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, SetSpaceFlag_002, TestSize.Level2)
{
    int cmdIndex = -1;
    ASSERT_NE(GetMatchCmd("SetSpaceFlag ", &cmdIndex), nullptr);
    g_enterpriseSpaceSetFlag = true;
    EnableEnterpriseSpaceIoMock();
    DoCmdByIndex(cmdIndex, "false", nullptr);
    EXPECT_EQ(g_enterpriseSpaceOpenCount, 1);
    EXPECT_EQ(g_enterpriseSpaceIoctlCount, 2);
    EXPECT_EQ(g_enterpriseSpaceCloseCount, 1);
    EXPECT_EQ(g_enterpriseSpaceRequests[0], ENTERPRISE_SPACE_TEST_SET_REQUEST);
    EXPECT_EQ(g_enterpriseSpaceRequests[1], ENTERPRISE_SPACE_TEST_GET_REQUEST);
    EXPECT_FALSE(g_enterpriseSpaceSetFlag);
}

/*
 * @tc.name: SetSpaceFlag_003
 * @tc.desc: Verify invalid command values do not access the enterprise space device.
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, SetSpaceFlag_003, TestSize.Level2)
{
    int cmdIndex = -1;
    ASSERT_NE(GetMatchCmd("SetSpaceFlag ", &cmdIndex), nullptr);
    DoCmdByIndex(cmdIndex, "invalid", nullptr);
    EXPECT_EQ(g_enterpriseSpaceOpenCount, 0);
    EXPECT_EQ(g_enterpriseSpaceIoctlCount, 0);
    EXPECT_EQ(g_enterpriseSpaceCloseCount, 0);
}

/*
 * @tc.name: SetEnterpriseSpaceFlag_001
 * @tc.desc: Verify an open failure stops before ioctl and close.
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, SetEnterpriseSpaceFlag_001, TestSize.Level2)
{
    g_enterpriseSpaceOpenResult = -1;
    EnableEnterpriseSpaceIoMock();
    ExecuteSetSpaceFlagCommand();
    EXPECT_EQ(g_enterpriseSpaceOpenCount, 1);
    EXPECT_EQ(g_enterpriseSpaceIoctlCount, 0);
    EXPECT_EQ(g_enterpriseSpaceCloseCount, 0);
}

/*
 * @tc.name: SetEnterpriseSpaceFlag_002
 * @tc.desc: Verify a SET ioctl failure closes the device and returns failure.
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, SetEnterpriseSpaceFlag_002, TestSize.Level2)
{
    g_enterpriseSpaceSetResult = -1;
    EnableEnterpriseSpaceIoMock();
    ExecuteSetSpaceFlagCommand();
    EXPECT_EQ(g_enterpriseSpaceIoctlCount, 1);
    EXPECT_EQ(g_enterpriseSpaceCloseCount, 1);
}

/*
 * @tc.name: SetEnterpriseSpaceFlag_003
 * @tc.desc: Verify a GET ioctl failure closes the device and returns failure.
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, SetEnterpriseSpaceFlag_003, TestSize.Level2)
{
    g_enterpriseSpaceGetResult = -1;
    EnableEnterpriseSpaceIoMock();
    ExecuteSetSpaceFlagCommand();
    EXPECT_EQ(g_enterpriseSpaceIoctlCount, 2);
    EXPECT_EQ(g_enterpriseSpaceCloseCount, 1);
}

/*
 * @tc.name: SetEnterpriseSpaceFlag_004
 * @tc.desc: Verify a mismatched GET value closes the device and returns failure.
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, SetEnterpriseSpaceFlag_004, TestSize.Level2)
{
    g_enterpriseSpaceUseCustomReadback = true;
    g_enterpriseSpaceReadbackFlag = false;
    EnableEnterpriseSpaceIoMock();
    ExecuteSetSpaceFlagCommand();
    EXPECT_EQ(g_enterpriseSpaceIoctlCount, 2);
    EXPECT_EQ(g_enterpriseSpaceCloseCount, 1);
}

HWTEST_F(CmdsUnitTest, TestCommonMkdir, TestSize.Level1)
{
    auto checkMkdirCmd = [=](const char *mkdirFile, const char *cmdLine) {
        DoCmdByName("mkdir ", cmdLine);
        return access(mkdirFile, F_OK);
    };
    EXPECT_EQ(checkMkdirCmd("/data/init_ut/test_dir0", "/data/init_ut/test_dir0"), 0);
    EXPECT_EQ(checkMkdirCmd("/data/init_ut/test_dir1", "/data/init_ut/test_dir1 0755"), 0);
    EXPECT_EQ(checkMkdirCmd("/data/init_ut/test_dir2", "/data/init_ut/test_dir2 0755 system system"), 0);

    // abnormal
    EXPECT_NE(checkMkdirCmd("/data/init_ut/test_dir3", ""), 0);
    EXPECT_NE(checkMkdirCmd("/data/init_ut/test_dir4", "/data/init_ut/test_dir4 0755 system"), 0);
    EXPECT_EQ(checkMkdirCmd("/data/init_ut/test_dir5", "/data/init_ut/test_dir5 0755 error error"), 0);
}

HWTEST_F(CmdsUnitTest, TestCommonChown, TestSize.Level1)
{
    const char *testFile = "/data/init_ut/test_dir0";
    DoCmdByName("chown ", "system system /data/init_ut/test_dir0");
    struct stat info = {};
    stat(testFile, &info);
    const unsigned int systemUidGid = 1000;
    EXPECT_EQ(info.st_uid, systemUidGid);
    EXPECT_EQ(info.st_gid, systemUidGid);

    // abnormal
    DoCmdByName("chown ", "error error /data/init_ut/test_dir0");
    stat(testFile, &info);
    EXPECT_EQ(info.st_uid, systemUidGid);
    EXPECT_EQ(info.st_gid, systemUidGid);
}

HWTEST_F(CmdsUnitTest, TestCommonChmod, TestSize.Level1)
{
    const char *testFile = "/data/init_ut/test_dir0/test_file0";
    const mode_t testMode = S_IRWXU | S_IRWXG | S_IRWXO;
    int fd = open(testFile, O_CREAT | O_WRONLY, testMode);
    ASSERT_GE(fd, 0);
    DoCmdByName("chmod ", "777 /data/init_ut/test_dir0/test_file0");
    struct stat info;
    stat(testFile, &info);
    EXPECT_EQ(testMode, testMode & info.st_mode);

    // abnormal
    DoCmdByName("chmod ", "999 /data/init_ut/test_dir0/test_file0");
    stat(testFile, &info);
    EXPECT_EQ(testMode, testMode & info.st_mode);
    DoCmdByName("chmod ", "777 /data/init_ut/test_dir0/test_file001");

    close(fd);
}

HWTEST_F(CmdsUnitTest, TestCommonCopy, TestSize.Level1)
{
    const char *testFile1 = "/data/init_ut/test_dir0/test_file_copy1";
    DoCmdByName("copy ", "/data/init_ut/test_dir0/test_file0 /data/init_ut/test_dir0/test_file_copy1");
    int fd = open(testFile1, O_RDWR);
    ASSERT_GE(fd, 0);
    write(fd, "aaa", strlen("aaa"));

    const char *testFile2 = "/data/init_ut/test_dir0/test_file_copy2";
    DoCmdByName("copy ", "/data/init_ut/test_dir0/test_file_copy1 /data/init_ut/test_dir0/test_file_copy2");
    int ret = access(testFile2, F_OK);
    EXPECT_EQ(ret, 0);
    close(fd);
    // abnormal
    DoCmdByName("copy ", "/data/init_ut/test_dir0/test_file_copy1 /data/init_ut/test_dir0/test_file_copy1");
    DoCmdByName("copy ", "/data/init_ut/test_dir0/test_file_copy11 /data/init_ut/test_dir0/test_file_copy1");
    DoCmdByName("copy ", "a");

    DoCmdByName("chmod ", "111 /data/init_ut/test_dir0/test_file_copy1");
    DoCmdByName("copy ", "/data/init_ut/test_dir0/test_file_copy1 /data/init_ut/test_dir0/test_file_copy2");

    DoCmdByName("chmod ", "777 /data/init_ut/test_dir0/test_file_copy1");
    DoCmdByName("chmod ", "111 /data/init_ut/test_dir0/test_file_copy2");
    DoCmdByName("copy ", "/data/init_ut/test_dir0/test_file_copy1 /data/init_ut/test_dir0/test_file_copy2");
    DoCmdByName("chmod ", "777 /data/init_ut/test_dir0/test_file_copy2");
}

HWTEST_F(CmdsUnitTest, TestCommonWrite, TestSize.Level1)
{
    const char *testFile1 = "/data/init_ut/test_dir0/test_file_write1";
    int fd = open(testFile1, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    ASSERT_GE(fd, 0);

    DoCmdByName("write ", "/data/init_ut/test_dir0/test_file_write1 aaa");
    const int bufLen = 50;
    char buffer[bufLen];
    int length = read(fd, buffer, bufLen - 1);
    EXPECT_EQ(length, strlen("aaa"));
    close(fd);
    // abnormal
    DoCmdByName("write ", "/data/init_ut/test_dir0/test_file_write2 aaa 2");
}

HWTEST_F(CmdsUnitTest, TestCommonRm, TestSize.Level1)
{
    const char *testFile1 = "/data/init_ut/test_dir0/test_file_write1";
    DoCmdByName("rm ", testFile1);
    int ret = access(testFile1, F_OK);
    EXPECT_NE(ret, 0);

    testFile1 = "/data/init_ut/test_dir1";
    DoCmdByName("rmdir ", testFile1);
    ret = access(testFile1, F_OK);
    EXPECT_NE(ret, 0);

    // abnormal
    DoCmdByName("rmdir ", testFile1);
}

HWTEST_F(CmdsUnitTest, TestCommonExport, TestSize.Level1)
{
    DoCmdByName("export ", "TEST_INIT 1");
    EXPECT_STREQ("1", getenv("TEST_INIT"));
    unsetenv("TEST_INIT");
    EXPECT_STRNE("1", getenv("TEST_INIT"));
}

HWTEST_F(CmdsUnitTest, TestCommonMount, TestSize.Level1)
{
    DoCmdByName("mount ", "ext4 /dev/block/platform/soc/10100000.himci.eMMC/by-name/vendor "
        "/vendor wait rdonly barrier=1");
    struct statvfs64 vfs {};
    int ret = statvfs64("/vendor", &vfs);
    EXPECT_GE(ret, 0);
    EXPECT_GT(vfs.f_bsize, 0);
}

HWTEST_F(CmdsUnitTest, TestGetCmdKey, TestSize.Level1)
{
    const char *cmd1 = GetCmdKey(0);
    EXPECT_STREQ(cmd1, "start ");
}

HWTEST_F(CmdsUnitTest, TestDoCmdByIndex, TestSize.Level1)
{
    DoCmdByIndex(1, "/data/init_ut/test_cmd_dir0", nullptr);
    int ret = access("/data/init_ut/test_cmd_dir0", F_OK);
    EXPECT_EQ(ret, 0);

    const int execPos = 17;
    DoCmdByIndex(execPos, "sleep 1", nullptr);
    DoCmdByIndex(23, "test", nullptr); // 23 is cmd index
}

HWTEST_F(CmdsUnitTest, TestGetCmdLinesFromJson, TestSize.Level1)
{
    const char *jsonStr = "{\"jobs\":[{\"name\":\"init\",\"cmds\":[\"sleep 1\",100,\"test321 123\"]}]}";
    cJSON* jobItem = cJSON_Parse(jsonStr);
    ASSERT_NE(nullptr, jobItem);
    cJSON *cmdsItem = cJSON_GetObjectItem(jobItem, "jobs");
    ASSERT_NE(nullptr, cmdsItem);
    ASSERT_TRUE(cJSON_IsArray(cmdsItem));

    cJSON *cmdsItem1 = cJSON_GetArrayItem(cmdsItem, 0);
    ASSERT_NE(nullptr, cmdsItem1);
    CmdLines **cmdLines = (CmdLines **)calloc(1, sizeof(CmdLines *));
    ASSERT_NE(nullptr, cmdLines);
    int ret = GetCmdLinesFromJson(cmdsItem1, cmdLines);
    EXPECT_EQ(ret, -1);
    cJSON *cmdsItem2 = cJSON_GetObjectItem(cmdsItem1, "cmds");
    ASSERT_NE(nullptr, cmdsItem2);
    ret = GetCmdLinesFromJson(cmdsItem2, cmdLines);
    EXPECT_EQ(ret, 0);

    cJSON_Delete(jobItem);
    if (cmdLines[0] != nullptr) {
        free(cmdLines[0]);
        cmdLines[0] = nullptr;
    }
    free(cmdLines);
    cmdLines = nullptr;
}

HWTEST_F(CmdsUnitTest, TestInitCmdFunc, TestSize.Level1)
{
    int ret = GetBootModeFromMisc();
    EXPECT_EQ(ret, 0);
    ret = SetFileCryptPolicy(nullptr);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CmdsUnitTest, TestBuildStringFromCmdArg, TestSize.Level1)
{
    int strNum = 3;
    struct CmdArgs *ctx = (struct CmdArgs *)calloc(1, sizeof(struct CmdArgs) + sizeof(char *) * (strNum));
    ctx->argc = strNum;
    ctx->argv[0] = strdup("123456789012345678901234567890123456789012345678901234567890   \
            1234567890123456789012345678901234567890123456789012345678901234567");
    ctx->argv[1] = strdup("test");
    ctx->argv[2] = nullptr;
    char *options = BuildStringFromCmdArg(ctx, 0);
    EXPECT_EQ(options[0], '\0');
    free(options);

    options = BuildStringFromCmdArg(ctx, 1);
    EXPECT_STREQ(options, "test");
    free(options);
    FreeCmdArg(ctx);
}

HWTEST_F(CmdsUnitTest, TestInitDiffTime, TestSize.Level1)
{
    INIT_TIMING_STAT stat;
    stat.startTime.tv_sec = 2; // 2 is test sec
    stat.startTime.tv_nsec = 1000;  // 1000 is test nsec

    stat.endTime.tv_sec = 3;  // 3 is test sec
    stat.endTime.tv_nsec = 0;

    long long diff = InitDiffTime(&stat);
    EXPECT_TRUE(diff > 0);
}

HWTEST_F(CmdsUnitTest, TestDeInitEswapSpace, TestSize.Level1)
{
    bool ret = DeInitDmaEswapSpace();
    int fd = open(DMA_DEVICE_FILE, O_RDWR | O_CLOEXEC | O_NONBLOCK);
    if (fd <= 0) {
        EXPECT_EQ(ret, false);
    } else {
        close(fd);
        EXPECT_EQ(ret, true);
    }
    ret = DeInitGpuEswapSpace();
    void* libGpuKiaHandle = dlopen(GPU_RECLAIM_IMPL_SO, RTLD_NOW);
    if (!libGpuKiaHandle) {
        EXPECT_EQ(ret, false);
    } else {
        dlclose(libGpuKiaHandle);
        EXPECT_EQ(ret, true);
    }
}
} // namespace init_ut
