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

#include <sys/statvfs.h>
#include "init_cmds.h"
#include "init_param.h"
#include "init_group_manager.h"
#include "param_stub.h"
#include "init_utils.h"
#include "trigger_manager.h"

using namespace testing::ext;
using namespace std;

namespace init_ut {
class CmdsUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void)
    {
        PrepareInitUnitTestEnv();
    };
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(CmdsUnitTest, TestCmdExecByName, TestSize.Level1)
{
    DoCmdByName("load_param ", "     /system/etc/param            onlyadd");
    DoCmdByName("symlink ", "/proc/self/fd/0 /dev/stdin");
    DoCmdByName("symlink ", "/notfile ");
    DoCmdByName("insmod ",
        "libdemo.z.so anony=1 mmz_allocator=hisi mmz=anonymous,0,0xA8000000,384M || report_error");
    DoCmdByName("insmod ", "/vendor/modules/hi_irq.ko");
    DoCmdByName("insmod ", "/data/init_ut/test_insmod");

    DoCmdByName("setparam ", "sys.usb.config ${persist.sys.usb.config}");

    DoCmdByName("load_persist_params ", "");
    DoCmdByName("trigger ", "");
    DoCmdByName("domainname ", "localdomain");
    DoCmdByName("hostname ", "localhost");
    DoCmdByName("sleep ", "1");
    DoCmdByName("setrlimit ", "RLIMIT_NICE 40 40");
    DoCmdByName("setrlimit ", "RLIMIT_NICE2 40 40");
    DoCmdByName("start ", "init_ut");
    DoCmdByName("stop ", "init_ut");
    DoCmdByName("reset ", "init_ut");
    DoCmdByName("reboot ", "");
    DoCmdByName("ifup ", "lo");
    DoCmdByName("mknode ", "/dev/null b 0666 1 3");
    DoCmdByName("mknode ", "/dev/null C 0666 1 3");
    DoCmdByName("mknode ", "/dev/null F 0666 1 3");
    DoCmdByName("mknode ", "/dev/null A 0666 1 3");
    DoCmdByName("makedev ", "999 999");
    DoCmdByName("mount_fstab ", "");
    DoCmdByName("umount_fstab ", "");
    DoCmdByName("mksandbox ", "system chipset");
    DoCmdByName("mksandbox ", "system");
    DoCmdByName("mksandbox ", "notsupport");
    DoCmdByName("mksandbox ", "");
}
HWTEST_F(CmdsUnitTest, TestCmdExecByName1, TestSize.Level1)
{
    DoCmdByName("timer_start ", "media_service|5000");
    DoCmdByName("timer_stop ", "media_service");
    DoCmdByName("exec ", "media_service");
    DoCmdByName("syncexec ", "/system/bin/toybox");
    DoCmdByName("load_access_token_id ", "media_service");
    DoCmdByName("stopAllServices ", "true");
    DoCmdByName("umount ", "");
    DoCmdByName("mount ", "");
    DoCmdByName("mount ", "ext4 /dev/block/platform/soc/10100000.himci.eMMC/by-name/vendor "
        "/data wait filecrypt=555");
    DoCmdByName("init_global_key ", "/data");
    DoCmdByName("init_global_key ", "arg0 arg1");
    DoCmdByName("init_main_user ", "testUser");
    DoCmdByName("init_main_user ", nullptr);
    DoCmdByName("mkswap ", "");
    DoCmdByName("swapon ", "");
    DoCmdByName("sync ", "");
    DoCmdByName("restorecon ", "");
    DoCmdByName("suspend ", "");
    DoCmdByName("wait ", "1");
    DoCmdByName("wait ", "aaa 1");
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
    struct stat info;
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
    fd = -1;
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
    fd = -1;
    // abnormal
    DoCmdByName("copy ", "/data/init_ut/test_dir0/test_file_copy1 /data/init_ut/test_dir0/test_file_copy1");
    DoCmdByName("copy ", "/data/init_ut/test_dir0/test_file_copy11 /data/init_ut/test_dir0/test_file_copy1");
    DoCmdByName("copy ", "a");
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
    fd = -1;
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
    DoCmdByIndex(1, "/data/init_ut/test_cmd_dir0");
    int ret = access("/data/init_ut/test_cmd_dir0", F_OK);
    EXPECT_EQ(ret, 0);

    const int execPos = 17;
    DoCmdByIndex(execPos, "sleep 1");
    DoCmdByIndex(23, "test"); // 23 is cmd index
}

HWTEST_F(CmdsUnitTest, TestGetCmdLinesFromJson, TestSize.Level1)
{
    const char *jsonStr = "{\"jobs\":[{\"name\":\"init\",\"cmds\":[\"sleep 1\"]}]}";
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
}
} // namespace init_ut
