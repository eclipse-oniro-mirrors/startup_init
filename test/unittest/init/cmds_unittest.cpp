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
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <csignal>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>
#include <dirent.h>
#include <climits>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "func_wrapper.h"
#include "mkdir_test_wrapper.h"
#include "securec.h"
#include "init_cmds.h"
#include "init_module_engine.h"

extern "C" void SetTestFileCryptPolicyFunc(int (*func)(const char *path));
#include "init_log.h"
#include "init_param.h"
#include "init_group_manager.h"
#include "param_stub.h"
#include "init_utils.h"
#include "trigger_manager.h"

using namespace testing::ext;
using namespace std;

static constexpr mode_t DIR_PERMISSION_MASK = S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO;
static constexpr uid_t SYSTEM_UID_GID = 1000;
static constexpr mode_t TEST_PRIVATE_DIR_MODE = S_IRWXU;
static constexpr mode_t TEST_FILE_MODE = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
static constexpr mode_t TEST_SPECIAL_MODE = S_IRUSR | S_IWUSR;
static constexpr int MKDIR_RETRY_CALL_COUNT = 2;
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

static std::vector<std::string> g_mkdirLogs;
static int g_mkdirMockCalls = 0;
static int g_mkdirMockFirstRet = 0;
static int g_mkdirMockFirstErrno = 0;
static int g_mkdirMockSecondRet = 0;
static int g_mkdirMockSecondErrno = 0;
static int g_lstatMockCalls = 0;
static int g_lstatMockFailAt = 0;
static int g_lstatMockErrno = 0;
static bool g_lstatMockUseMode = false;
static mode_t g_lstatMockMode = 0;
static int g_fchmodMockRet = 0;
static int g_fchmodMockErrno = EPERM;
static int g_fchmodMockCalls = 0;
static int g_fchownMockRet = 0;
static int g_fchownMockErrno = EACCES;
static int g_fchownMockCalls = 0;
static int g_chownMockRet = 0;
static int g_chownMockErrno = EACCES;
static int g_chownMockCalls = 0;
static struct passwd g_systemPw = {};
static struct group g_systemGr = {};
static char g_systemName[] = "system";
static char g_systemPasswd[] = "x";
static char g_systemGecos[] = "system";
static char g_systemDir[] = "/";
static char g_systemShell[] = "/bin/sh";
static int g_fstatMockRet = 0;
static int g_fstatMockErrno = EIO;
static int g_fstatMockCalls = 0;
static bool g_fstatMockUseStat = false;
static uid_t g_fstatMockUid = 0;
static gid_t g_fstatMockGid = 0;
static int g_restoreCalls = 0;
static int g_fscryptCalls = 0;
static int g_fscryptMockRet = 0;
static int g_restoreExecutorId = -1;

static int TestRestoreExecutor(int id, const char *name, int argc, const char **argv)
{
    (void)id;
    (void)name;
    (void)argc;
    (void)argv;
    g_restoreCalls++;
    return 0;
}

static void RemoveTestPath(const char *path)
{
    struct stat st = { 0 };
    if (lstat(path, &st) != 0) {
        return;
    }
    if (S_ISDIR(st.st_mode)) {
        (void)rmdir(path);
    } else {
        (void)unlink(path);
    }
}

static void RemoveAll(const char *path)
{
    struct stat st = { 0 };
    if (lstat(path, &st) != 0) {
        return;
    }
    if (!S_ISDIR(st.st_mode)) {
        (void)unlink(path);
        return;
    }
    DIR *dir = opendir(path);
    if (dir != nullptr) {
        struct dirent *entry = nullptr;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            char child[PATH_MAX] = { 0 };
            int len = snprintf_s(child, sizeof(child), sizeof(child) - 1, "%s/%s", path, entry->d_name);
            if (len > 0 && len < static_cast<int>(sizeof(child))) {
                RemoveAll(child);
            }
        }
        (void)closedir(dir);
    }
    (void)rmdir(path);
}

static void CleanupMkdirTestPaths(void)
{
    const char *paths[] = {
        "/data/init_ut/mkdir_exist_dir",
        "/data/init_ut/mkdir_regular_file",
        "/data/init_ut/mkdir_link_file",
        "/data/init_ut/mkdir_link_dir",
        "/data/init_ut/mkdir_link_target",
        "/data/init_ut/mkdir_link_target_dir",
        "/data/init_ut/mkdir_nofollow_link",
        "/data/init_ut/mkdir_nofollow_target",
        "/data/init_ut/mkdir_snapshot",
        "/data/init_ut/mkdir_readback",
        "/data/init_ut/mkdir_precheck",
        "/data/init_ut/mkdir_precheck_file",
        "/data/init_ut/mkdir_precheck_dir",
        "/data/init_ut/mkdir_category",
        "/data/init_ut/mkdir_order",
        "/data/init_ut/mkdir_default_mode",
        "/data/init_ut/mkdir_owner_new",
        "/data/init_ut/mkdir_owner_exist",
        "/data/init_ut/mkdir_link_dangling",
        "/data/init_ut/mkdir_link_dangling_target",
        "/data/init_ut/mkdir_fifo",
        "/data/init_ut/mkdir_socket",
        "/data/init_ut/mkdir_type_mock",
        "/data/init_ut/mkdir_readback_owner",
        "/data/init_ut/mkdir_fchmod_fail",
        "/data/init_ut/mkdir_chown_fail",
        "/data/init_ut/mkdir_fstat_fail",
        "/data/init_ut/mkdir_restore_existing",
        "/data/init_ut/mkdir_lstat_fail",
        "/data/init_ut/mkdir_precheck_lstat_fail",
        "/data/init_ut/mkdir_precheck_type",
        "/data/init_ut/mkdir_fscrypt_dir",
        "/data/init_ut/chown_dir",
        "/data/init_ut/chown_target",
        "/data/init_ut/chown_link",
        "/data/init_ut/chown_mock_fail",
        "/data/init_ut/owner_guard",
    };
    for (const char *path : paths) {
        RemoveTestPath(path);
    }
}

static void CaptureMkdirLog(int logLevel, uint32_t domain, const char *tag, const char *fmt, va_list args)
{
    (void)logLevel;
    (void)domain;
    (void)tag;
    char buffer[512] = { 0 };
    if (vsnprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, fmt, args) < 0) {
        return;
    }
    g_mkdirLogs.emplace_back(buffer);
}

static void ResetMkdirLogCapture(void)
{
    g_mkdirLogs.clear();
    SetInitCommLog(CaptureMkdirLog);
}

static bool HasMkdirLog(const char *needle)
{
    for (const auto &line : g_mkdirLogs) {
        if (line.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

static int MkdirSequenceMock(const char *path, mode_t mode)
{
    (void)path;
    (void)mode;
    g_mkdirMockCalls++;
    if (g_mkdirMockCalls == 1) {
        if (g_mkdirMockFirstRet != 0) {
            errno = g_mkdirMockFirstErrno;
        }
        return g_mkdirMockFirstRet;
    }
    if (g_mkdirMockSecondRet != 0) {
        errno = g_mkdirMockSecondErrno;
    }
    return g_mkdirMockSecondRet;
}

static void SetMkdirSequence(int firstRet, int firstErrno, int secondRet, int secondErrno)
{
    g_mkdirMockCalls = 0;
    g_mkdirMockFirstRet = firstRet;
    g_mkdirMockFirstErrno = firstErrno;
    g_mkdirMockSecondRet = secondRet;
    g_mkdirMockSecondErrno = secondErrno;
    UpdateMkdirFunc(MkdirSequenceMock);
}

static int LstatMock(const char *pathname, struct stat *buf)
{
    g_lstatMockCalls++;
    if (g_lstatMockFailAt > 0 && g_lstatMockCalls == g_lstatMockFailAt) {
        errno = g_lstatMockErrno;
        return -1;
    }
    if (g_lstatMockUseMode) {
        (void)memset_s(buf, sizeof(*buf), 0, sizeof(*buf));
        buf->st_mode = g_lstatMockMode;
        return 0;
    }
    return RealLstat(pathname, buf);
}

static int FchmodatMock(int dirfd, const char *pathname, mode_t mode, int flags)
{
    (void)dirfd;
    (void)pathname;
    (void)mode;
    (void)flags;
    g_fchmodMockCalls++;
    if (g_fchmodMockRet != 0) {
        errno = g_fchmodMockErrno;
        return g_fchmodMockRet;
    }
    return 0;
}

static int LchownMock(const char *pathname, uid_t owner, gid_t group)
{
    (void)pathname;
    (void)owner;
    (void)group;
    g_fchownMockCalls++;
    if (g_fchownMockRet != 0) {
        errno = g_fchownMockErrno;
        return g_fchownMockRet;
    }
    return 0;
}

static void InitSystemAccountMocks(void)
{
    g_systemPw.pw_name = g_systemName;
    g_systemPw.pw_passwd = g_systemPasswd;
    g_systemPw.pw_uid = SYSTEM_UID_GID;
    g_systemPw.pw_gid = SYSTEM_UID_GID;
    g_systemPw.pw_gecos = g_systemGecos;
    g_systemPw.pw_dir = g_systemDir;
    g_systemPw.pw_shell = g_systemShell;
    g_systemGr.gr_name = g_systemName;
    g_systemGr.gr_passwd = g_systemPasswd;
    g_systemGr.gr_gid = SYSTEM_UID_GID;
    g_systemGr.gr_mem = nullptr;
}

static struct passwd *PwNamMock(const char *name)
{
    if (strcmp(name, g_systemName) == 0) {
        return &g_systemPw;
    }
    return nullptr;
}

static struct group *GrNamMock(const char *name)
{
    if (strcmp(name, g_systemName) == 0) {
        return &g_systemGr;
    }
    return nullptr;
}

static struct group *GetGRentMock(void)
{
    return nullptr;
}

static int ChownMock(const char *pathname, uid_t owner, gid_t group)
{
    (void)pathname;
    (void)owner;
    (void)group;
    g_chownMockCalls++;
    if (g_chownMockRet != 0) {
        errno = g_chownMockErrno;
        return g_chownMockRet;
    }
    return 0;
}

static int FstatatMock(int dirfd, const char *pathname, struct stat *buf, int flags)
{
    (void)dirfd;
    (void)flags;
    g_fstatMockCalls++;
    if (g_fstatMockRet != 0) {
        errno = g_fstatMockErrno;
        return g_fstatMockRet;
    }
    if (g_fstatMockUseStat) {
        (void)memset_s(buf, sizeof(*buf), 0, sizeof(*buf));
        buf->st_mode = S_IFDIR | DEFAULT_DIR_MODE;
        buf->st_uid = g_fstatMockUid;
        buf->st_gid = g_fstatMockGid;
        return 0;
    }
    return RealFstatat(dirfd, pathname, buf, flags);
}

static int FscryptMock(const char *dir)
{
    (void)dir;
    g_fscryptCalls++;
    if (g_fscryptMockRet != 0) {
        errno = EIO;
    }
    return g_fscryptMockRet;
}

static void ResetDoMkDirMocks(void)
{
    UpdateMkdirFunc(nullptr);
    UpdateStatFunc(nullptr);
    ResetMkdirTestWrappers();
    SetTestFileCryptPolicyFunc(nullptr);
    UpdateOpenFunc(nullptr);
    UpdateCloseFunc(nullptr);
    SetInitCommLog(nullptr);
    g_mkdirLogs.clear();
    g_mkdirMockCalls = 0;
    g_mkdirMockFirstRet = 0;
    g_mkdirMockFirstErrno = 0;
    g_mkdirMockSecondRet = 0;
    g_mkdirMockSecondErrno = 0;
    g_lstatMockCalls = 0;
    g_lstatMockFailAt = 0;
    g_lstatMockErrno = 0;
    g_lstatMockUseMode = false;
    g_lstatMockMode = 0;
    g_fchmodMockRet = 0;
    g_fchmodMockErrno = EPERM;
    g_fchmodMockCalls = 0;
    g_fchownMockRet = 0;
    g_fchownMockErrno = EACCES;
    g_fchownMockCalls = 0;
    g_chownMockRet = 0;
    g_chownMockErrno = EACCES;
    g_chownMockCalls = 0;
    g_fstatMockRet = 0;
    g_fstatMockErrno = EIO;
    g_fstatMockCalls = 0;
    g_fstatMockUseStat = false;
    g_fstatMockUid = 0;
    g_fstatMockGid = 0;
    g_restoreCalls = 0;
    g_fscryptCalls = 0;
    g_fscryptMockRet = 0;
}

static void ResetMkdirWrappersForAssert(void)
{
    UpdateMkdirFunc(nullptr);
    UpdateStatFunc(nullptr);
    ResetMkdirTestWrappers();
    SetTestFileCryptPolicyFunc(nullptr);
}

static void InstallDoMkDirMocks(void)
{
    UpdateFchmodatFunc(FchmodatMock);
}

static void InstallLstatMock(void)
{
    UpdateLstatFunc(LstatMock);
}

static void InstallLchownMock(void)
{
    UpdateLchownFunc(LchownMock);
}

static void InstallChownMock(void)
{
    UpdateChownFunc(ChownMock);
}

static void InstallAccountMocks(void)
{
    InitSystemAccountMocks();
    UpdateGetPwNamFunc(PwNamMock);
    UpdateGetGrNamFunc(GrNamMock);
    UpdateGetGRentFunc(GetGRentMock);
}

static void InstallFstatatMock(void)
{
    UpdateFstatatFunc(FstatatMock);
}

static void InstallFscryptMock(void)
{
    SetTestFileCryptPolicyFunc(FscryptMock);
}

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
    if (fgets(buffer, sizeof(buffer) - 1, file) == nullptr) {
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
    static void SetUpTestCase(void)
    {
        (void)mkdir("/data/init_ut", DEFAULT_DIR_MODE);
        g_restoreExecutorId = AddCmdExecutor("restoreContentRecurse", TestRestoreExecutor);
    }
    static void TearDownTestCase(void)
    {
        if (g_restoreExecutorId >= 0) {
            RemoveCmdExecutor("restoreContentRecurse", g_restoreExecutorId);
            g_restoreExecutorId = -1;
        }
        const char *legacyDirs[] = {
            "/data/init_ut/test_dir0", "/data/init_ut/test_dir1", "/data/init_ut/test_dir2",
            "/data/init_ut/test_dir3", "/data/init_ut/test_dir4", "/data/init_ut/test_dir5",
        };
        for (const char *dir : legacyDirs) {
            RemoveAll(dir);
        }
    }
    void SetUp() override
    {
        ResetEnterpriseSpaceIoMock();
        ResetDoMkDirMocks();
    }
    void TearDown() override
    {
        UpdateOpenFunc(nullptr);
        UpdateIoctlFunc(nullptr);
        UpdateCloseFunc(nullptr);
        ResetDoMkDirMocks();
        CleanupMkdirTestPaths();
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
    mode_t oldMask = umask(0);
    EXPECT_EQ(checkMkdirCmd("/data/init_ut/test_dir0", "/data/init_ut/test_dir0"), 0);
    EXPECT_EQ(checkMkdirCmd("/data/init_ut/test_dir1", "/data/init_ut/test_dir1 0755"), 0);
    EXPECT_EQ(checkMkdirCmd("/data/init_ut/test_dir2", "/data/init_ut/test_dir2 0755 system system"), 0);
    (void)umask(oldMask);

    struct stat st = { 0 };
    ASSERT_EQ(stat("/data/init_ut/test_dir0", &st), 0);
    EXPECT_TRUE(S_ISDIR(st.st_mode));
    EXPECT_EQ(st.st_mode & DIR_PERMISSION_MASK, DEFAULT_DIR_MODE);
    ASSERT_EQ(stat("/data/init_ut/test_dir1", &st), 0);
    EXPECT_EQ(st.st_mode & DIR_PERMISSION_MASK, 0755);
    ASSERT_EQ(stat("/data/init_ut/test_dir2", &st), 0);
    EXPECT_EQ(st.st_mode & DIR_PERMISSION_MASK, 0755);
    if (geteuid() == 0) {
        EXPECT_EQ(st.st_uid, SYSTEM_UID_GID);
        EXPECT_EQ(st.st_gid, SYSTEM_UID_GID);
    }

    // abnormal
    EXPECT_NE(checkMkdirCmd("/data/init_ut/test_dir3", ""), 0);
    EXPECT_NE(checkMkdirCmd("/data/init_ut/test_dir4", "/data/init_ut/test_dir4 0755 system"), 0);
    EXPECT_EQ(checkMkdirCmd("/data/init_ut/test_dir5", "/data/init_ut/test_dir5 0755 error error"), 0);
    ASSERT_EQ(stat("/data/init_ut/test_dir5", &st), 0);
    EXPECT_EQ(st.st_mode & DIR_PERMISSION_MASK, 0755);
}

/*
 * @tc.name: TestCommonMkdirExistingDir
 * @tc.desc: 验证 mkdir 遇到已存在目录时按 cfg mode 修正并保持目录类型
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirExistingDir, TestSize.Level1)
{
    const char *dir = "/data/init_ut/mkdir_exist_dir";
    (void)rmdir(dir);
    ASSERT_EQ(mkdir(dir, TEST_PRIVATE_DIR_MODE), 0);
    EXPECT_EQ(chmod(dir, TEST_PRIVATE_DIR_MODE), 0);
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_exist_dir 0755");
    struct stat st = { 0 };
    ASSERT_EQ(stat(dir, &st), 0);
    EXPECT_TRUE(S_ISDIR(st.st_mode));
    EXPECT_EQ(st.st_mode & DIR_PERMISSION_MASK, 0755);
    EXPECT_EQ(g_restoreCalls, 0);
    EXPECT_FALSE(HasMkdirLog("existing type="));
    EXPECT_FALSE(HasMkdirLog("readback mismatch"));
    (void)rmdir(dir);
}

/*
 * @tc.name: TestCommonMkdirEexistRegularFile
 * @tc.desc: 验证 EEXIST 普通文件被识别、记录类型且权限不被修改
 * @tc.type: SECU
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirEexistRegularFile, TestSize.Level1)
{
    const char *file = "/data/init_ut/mkdir_regular_file";
    (void)unlink(file);
    int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, TEST_FILE_MODE);
    ASSERT_GE(fd, 0);
    EXPECT_EQ(close(fd), 0);
    EXPECT_EQ(chmod(file, TEST_FILE_MODE), 0);
    if (geteuid() == 0) {
        ASSERT_EQ(chown(file, SYSTEM_UID_GID, SYSTEM_UID_GID), 0);
    }
    struct stat before = { 0 };
    ASSERT_EQ(lstat(file, &before), 0);
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_regular_file 0755");
    struct stat st = { 0 };
    ASSERT_EQ(lstat(file, &st), 0);
    EXPECT_TRUE(S_ISREG(st.st_mode));
    EXPECT_EQ(st.st_mode & DIR_PERMISSION_MASK, before.st_mode & DIR_PERMISSION_MASK);
    EXPECT_EQ(st.st_uid, before.st_uid);
    EXPECT_EQ(st.st_gid, before.st_gid);
    EXPECT_TRUE(HasMkdirLog(file));
    EXPECT_TRUE(HasMkdirLog("err=20"));
    EXPECT_TRUE(HasMkdirLog("category=path"));
    EXPECT_TRUE(HasMkdirLog("existing type=regular file"));
    (void)chown(file, 0, 0);
    (void)unlink(file);
}

static bool PrepareMkdirSymlinkTarget(const char *link, const char *target, bool targetIsDir)
{
    (void)unlink(link);
    if (targetIsDir) {
        (void)rmdir(target);
        if (mkdir(target, TEST_PRIVATE_DIR_MODE) != 0) {
            return false;
        }
        if (chmod(target, TEST_PRIVATE_DIR_MODE) != 0) {
            return false;
        }
    } else {
        (void)unlink(target);
        int fd = open(target, O_WRONLY | O_CREAT | O_TRUNC, TEST_FILE_MODE);
        if (fd < 0) {
            return false;
        }
        (void)close(fd);
        if (chmod(target, TEST_FILE_MODE) != 0) {
            return false;
        }
    }
    if (symlink(target, link) != 0) {
        return false;
    }
    if (geteuid() == 0) {
        (void)lchown(link, SYSTEM_UID_GID, SYSTEM_UID_GID);
        (void)chown(target, SYSTEM_UID_GID, SYSTEM_UID_GID);
    }
    return true;
}

static void VerifyMkdirSymlinkRejected(const char *link, const char *target)
{
    struct stat linkBefore = { 0 };
    struct stat targetBefore = { 0 };
    if (lstat(link, &linkBefore) != 0 || stat(target, &targetBefore) != 0) {
        return;
    }
    char cmd[PATH_MAX + 8] = { 0 };
    int ret = snprintf_s(cmd, sizeof(cmd), sizeof(cmd) - 1, "%s 0755", link);
    if (ret <= 0) {
        return;
    }
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", cmd);
    struct stat linkAfter = { 0 };
    struct stat targetAfter = { 0 };
    if (lstat(link, &linkAfter) != 0 || stat(target, &targetAfter) != 0) {
        return;
    }
    EXPECT_TRUE(S_ISLNK(linkAfter.st_mode));
    EXPECT_EQ(linkAfter.st_mode & DIR_PERMISSION_MASK, linkBefore.st_mode & DIR_PERMISSION_MASK);
    EXPECT_EQ(linkAfter.st_uid, linkBefore.st_uid);
    EXPECT_EQ(linkAfter.st_gid, linkBefore.st_gid);
    EXPECT_EQ(targetAfter.st_mode & DIR_PERMISSION_MASK, targetBefore.st_mode & DIR_PERMISSION_MASK);
    EXPECT_EQ(targetAfter.st_uid, targetBefore.st_uid);
    EXPECT_EQ(targetAfter.st_gid, targetBefore.st_gid);
    EXPECT_TRUE(HasMkdirLog(link));
    EXPECT_TRUE(HasMkdirLog("err=20"));
    EXPECT_TRUE(HasMkdirLog("category=path"));
    EXPECT_TRUE(HasMkdirLog("existing type=symbolic link"));
}

/*
 * @tc.name: TestCommonMkdirEexistSymlink
 * @tc.desc: 验证 EEXIST 软链接被拒绝，链接与目标属性均不变
 * @tc.type: SECU
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirEexistSymlink, TestSize.Level1)
{
    const char *targetFile = "/data/init_ut/mkdir_link_target";
    const char *linkFile = "/data/init_ut/mkdir_link_file";
    ASSERT_TRUE(PrepareMkdirSymlinkTarget(linkFile, targetFile, false));
    VerifyMkdirSymlinkRejected(linkFile, targetFile);
    (void)unlink(linkFile);
    (void)unlink(targetFile);

    const char *targetDir = "/data/init_ut/mkdir_link_target_dir";
    const char *linkDir = "/data/init_ut/mkdir_link_dir";
    ASSERT_TRUE(PrepareMkdirSymlinkTarget(linkDir, targetDir, true));
    VerifyMkdirSymlinkRejected(linkDir, targetDir);
    (void)unlink(linkDir);
    (void)rmdir(targetDir);
}

/*
 * @tc.name: TestCommonMkdirSymlinkNoFollow
 * @tc.desc: 验证属性设置不跟随软链接，链接目标目录权限不变
 * @tc.type: SECU
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirSymlinkNoFollow, TestSize.Level1)
{
    const char *targetDir = "/data/init_ut/mkdir_nofollow_target";
    const char *linkDir = "/data/init_ut/mkdir_nofollow_link";
    (void)unlink(linkDir);
    (void)rmdir(targetDir);
    ASSERT_EQ(mkdir(targetDir, TEST_PRIVATE_DIR_MODE), 0);
    EXPECT_EQ(chmod(targetDir, TEST_PRIVATE_DIR_MODE), 0);
    ASSERT_EQ(symlink(targetDir, linkDir), 0);
    if (geteuid() == 0) {
        ASSERT_EQ(lchown(linkDir, SYSTEM_UID_GID, SYSTEM_UID_GID), 0);
        ASSERT_EQ(chown(targetDir, SYSTEM_UID_GID, SYSTEM_UID_GID), 0);
    }
    struct stat linkBefore = { 0 };
    struct stat targetBefore = { 0 };
    ASSERT_EQ(lstat(linkDir, &linkBefore), 0);
    ASSERT_EQ(stat(targetDir, &targetBefore), 0);
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_nofollow_link 0755 0 0");
    struct stat linkStat = { 0 };
    struct stat dirStat = { 0 };
    ASSERT_EQ(lstat(linkDir, &linkStat), 0);
    ASSERT_EQ(stat(targetDir, &dirStat), 0);
    EXPECT_EQ(linkStat.st_mode & DIR_PERMISSION_MASK, linkBefore.st_mode & DIR_PERMISSION_MASK);
    EXPECT_EQ(linkStat.st_uid, linkBefore.st_uid);
    EXPECT_EQ(linkStat.st_gid, linkBefore.st_gid);
    EXPECT_EQ(dirStat.st_mode & DIR_PERMISSION_MASK, targetBefore.st_mode & DIR_PERMISSION_MASK);
    EXPECT_EQ(dirStat.st_uid, targetBefore.st_uid);
    EXPECT_EQ(dirStat.st_gid, targetBefore.st_gid);
    EXPECT_TRUE(HasMkdirLog(linkDir));
    EXPECT_TRUE(HasMkdirLog("category=path"));
    EXPECT_TRUE(HasMkdirLog("existing type=symbolic link"));
    (void)unlink(linkDir);
    (void)rmdir(targetDir);
}

/*
 * @tc.name: TestCommonMkdirDataServiceRetry
 * @tc.desc: 验证 /data/service 重试成功继续、失败记录原始错误码并返回
 * @tc.type: RELI
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirDataServiceRetry, TestSize.Level1)
{
    InstallDoMkDirMocks();
    SetMkdirSequence(-1, ENOTDIR, 0, 0);
    g_restoreCalls = 0;
    g_fchmodMockCalls = 0;
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/service 0755");
    EXPECT_EQ(g_mkdirMockCalls, MKDIR_RETRY_CALL_COUNT);
    EXPECT_EQ(g_fchmodMockCalls, 1);
    EXPECT_EQ(g_restoreCalls, 1);
    EXPECT_TRUE(HasMkdirLog("Retry create directory '/data/service'"));
    EXPECT_TRUE(HasMkdirLog("first err=20"));
    EXPECT_FALSE(HasMkdirLog("retry Create directory"));

    g_mkdirMockCalls = 0;
    g_fchmodMockCalls = 0;
    g_restoreCalls = 0;
    SetMkdirSequence(-1, ENOTDIR, -1, EACCES);
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/service 0755");
    EXPECT_EQ(g_mkdirMockCalls, MKDIR_RETRY_CALL_COUNT);
    EXPECT_EQ(g_fchmodMockCalls, 0);
    EXPECT_EQ(g_restoreCalls, 0);
    EXPECT_TRUE(HasMkdirLog("retry Create directory '/data/service' failed"));
    EXPECT_TRUE(HasMkdirLog("err=13"));
    EXPECT_TRUE(HasMkdirLog("category=permission"));
}

/*
 * @tc.name: TestCommonMkdirDataServiceRetryEnospc
 * @tc.desc: 验证 /data/service 重试失败上报 ENOSPC 分类并停止属性设置
 * @tc.type: RELI
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirDataServiceRetryEnospc, TestSize.Level1)
{
    InstallDoMkDirMocks();
    SetMkdirSequence(-1, ENOTDIR, -1, ENOSPC);
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/service 0755 0 0");
    EXPECT_EQ(g_mkdirMockCalls, MKDIR_RETRY_CALL_COUNT);
    EXPECT_EQ(g_fchmodMockCalls, 0);
    EXPECT_EQ(g_restoreCalls, 0);
    EXPECT_TRUE(HasMkdirLog("retry Create directory '/data/service' failed"));
    EXPECT_TRUE(HasMkdirLog("err=28"));
    EXPECT_TRUE(HasMkdirLog("category=filesystem"));
}

/*
 * @tc.name: TestCommonMkdirErrnoSnapshot
 * @tc.desc: 验证 lstat 改写 errno 后仍按 mkdir 快照判定已存在，不误触发 restore
 * @tc.type: RELI
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirErrnoSnapshot, TestSize.Level1)
{
    const char *path = "/data/init_ut/mkdir_snapshot";
    RemoveTestPath(path);
    ASSERT_EQ(mkdir(path, TEST_PRIVATE_DIR_MODE), 0);
    SetMkdirSequence(-1, EEXIST, 0, 0);
    g_restoreCalls = 0;
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_snapshot 0755");
    EXPECT_EQ(g_restoreCalls, 0);
    struct stat st = { 0 };
    ASSERT_EQ(stat(path, &st), 0);
    EXPECT_TRUE(S_ISDIR(st.st_mode));
    EXPECT_EQ(st.st_mode & DIR_PERMISSION_MASK, 0755);
    EXPECT_FALSE(HasMkdirLog("readback mismatch"));
    RemoveTestPath(path);
}

/*
 * @tc.name: TestCommonMkdirErrnoCategory
 * @tc.desc: 验证 mkdir 失败错误码按五类输出
 * @tc.type: RELI
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirErrnoCategory, TestSize.Level1)
{
    InstallDoMkDirMocks();
    const struct {
        int err;
        const char *category;
    } cases[] = {
        { EACCES, "permission" },
        { EPERM, "permission" },
        { EROFS, "permission" },
        { ENOENT, "path" },
        { ENOTDIR, "path" },
        { ELOOP, "path" },
        { ENAMETOOLONG, "path" },
        { EFAULT, "path" },
        { ENOSPC, "filesystem" },
        { EDQUOT, "filesystem" },
        { ENOMEM, "filesystem" },
        { EIO, "filesystem" },
        { EMFILE, "filesystem" },
        { ENFILE, "filesystem" },
        { EINVAL, "other" },
    };
    for (const auto &testCase : cases) {
        SetMkdirSequence(-1, testCase.err, 0, 0);
        ResetMkdirLogCapture();
        DoCmdByName("mkdir ", "/data/init_ut/mkdir_category 0755");
        char errLog[16] = { 0 };
        int ret = snprintf_s(errLog, sizeof(errLog), sizeof(errLog) - 1, "err=%d", testCase.err);
        EXPECT_GT(ret, 0);
        EXPECT_TRUE(HasMkdirLog(errLog));
        EXPECT_TRUE(HasMkdirLog(testCase.category));
    }
}

/*
 * @tc.name: TestCommonMkdirErrnoCategoryEexist
 * @tc.desc: 验证 EEXIST 错误码映射为 exists（防御性分支直测）
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirErrnoCategoryEexist, TestSize.Level1)
{
    EXPECT_STREQ(TestGetMkdirErrnoCategory(EEXIST), "exists");
    EXPECT_STREQ(TestGetMkdirErrnoCategory(ENOENT), "path");
}

/*
 * @tc.name: TestCommonMkdirInvalidGroup
 * @tc.desc: 验证合法 owner + 非法 group 时记录参数错误且不设置属主
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirInvalidGroup, TestSize.Level1)
{
    const char *path = "/data/init_ut/mkdir_invalid_group";
    RemoveTestPath(path);
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_invalid_group 0755 1000 error");
    EXPECT_TRUE(HasMkdirLog("DoMkDir invalid owner or group"));
    EXPECT_FALSE(HasMkdirLog("failed change owner"));
    struct stat st = { 0 };
    ASSERT_EQ(stat(path, &st), 0);
    EXPECT_TRUE(S_ISDIR(st.st_mode));
    EXPECT_EQ(st.st_mode & DIR_PERMISSION_MASK, DEFAULT_DIR_MODE);
    RemoveTestPath(path);
}

/*
 * @tc.name: TestCommonMkdirAttrReadback
 * @tc.desc: 验证 mode 设置后回读，不一致时输出期望/实际值
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirAttrReadback, TestSize.Level1)
{
    const char *path = "/data/init_ut/mkdir_readback";
    RemoveTestPath(path);
    ASSERT_EQ(mkdir(path, TEST_PRIVATE_DIR_MODE), 0);
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_readback 0755");
    struct stat st = { 0 };
    ASSERT_EQ(stat(path, &st), 0);
    EXPECT_EQ(st.st_mode & DIR_PERMISSION_MASK, 0755);
    EXPECT_FALSE(HasMkdirLog("readback mismatch"));

    ASSERT_EQ(chmod(path, TEST_PRIVATE_DIR_MODE), 0);
    InstallDoMkDirMocks();
    g_fchmodMockCalls = 0;
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_readback 0755");
    EXPECT_EQ(g_fchmodMockCalls, 1);
    EXPECT_TRUE(HasMkdirLog("readback mismatch"));
    EXPECT_TRUE(HasMkdirLog("expect mode=00755"));
    EXPECT_TRUE(HasMkdirLog("actual mode=00700"));
    ResetMkdirTestWrappers();
    RemoveTestPath(path);
}

/*
 * @tc.name: TestCommonMkdirRestorePrecheck
 * @tc.desc: 验证 restore/fscrypt 仅在真实目录上执行
 * @tc.type: SECU
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirRestorePrecheck, TestSize.Level1)
{
    const char *file = "/data/init_ut/mkdir_precheck_file";
    const char *dir = "/data/init_ut/mkdir_precheck_dir";
    RemoveTestPath(file);
    RemoveTestPath(dir);

    int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, TEST_FILE_MODE);
    ASSERT_GE(fd, 0);
    EXPECT_EQ(close(fd), 0);
    struct stat before = { 0 };
    ASSERT_EQ(stat(file, &before), 0);
    g_restoreCalls = 0;
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_precheck_file 0755");
    EXPECT_EQ(g_restoreCalls, 0);
    struct stat after = { 0 };
    ASSERT_EQ(stat(file, &after), 0);
    EXPECT_EQ(after.st_mode & DIR_PERMISSION_MASK, before.st_mode & DIR_PERMISSION_MASK);
    EXPECT_EQ(after.st_uid, before.st_uid);
    EXPECT_EQ(after.st_gid, before.st_gid);

    g_restoreCalls = 0;
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_precheck_dir 0755");
    EXPECT_EQ(g_restoreCalls, 1);
    struct stat dirStat = { 0 };
    ASSERT_EQ(stat(dir, &dirStat), 0);
    EXPECT_TRUE(S_ISDIR(dirStat.st_mode));
    EXPECT_EQ(dirStat.st_mode & DIR_PERMISSION_MASK, 0755);
    RemoveTestPath(file);
    RemoveTestPath(dir);
}

/*
 * @tc.name: TestCommonMkdirDefaultMode
 * @tc.desc: 验证未声明 mode 时使用 DEFAULT_DIR_MODE 创建目录
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirDefaultMode, TestSize.Level1)
{
    const char *dir = "/data/init_ut/mkdir_default_mode";
    RemoveTestPath(dir);
    g_restoreCalls = 0;
    mode_t oldMask = umask(0);
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_default_mode");
    (void)umask(oldMask);
    struct stat st = { 0 };
    ASSERT_EQ(stat(dir, &st), 0);
    EXPECT_TRUE(S_ISDIR(st.st_mode));
    EXPECT_EQ(st.st_mode & DIR_PERMISSION_MASK, DEFAULT_DIR_MODE);
    EXPECT_EQ(g_restoreCalls, 1);
    RemoveTestPath(dir);
}

/*
 * @tc.name: TestCommonMkdirOwnerApply
 * @tc.desc: 验证新建目录按 cfg 声明的 mode/owner 生效
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirOwnerApply, TestSize.Level1)
{
    if (geteuid() != 0) {
        GTEST_SKIP() << "mkdir owner requires root";
    }
    const char *dir = "/data/init_ut/mkdir_owner_new";
    RemoveTestPath(dir);
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_owner_new 0755 1000 1000");
    struct stat st = { 0 };
    ASSERT_EQ(stat(dir, &st), 0);
    EXPECT_TRUE(S_ISDIR(st.st_mode));
    EXPECT_EQ(st.st_mode & DIR_PERMISSION_MASK, 0755);
    EXPECT_EQ(st.st_uid, SYSTEM_UID_GID);
    EXPECT_EQ(st.st_gid, SYSTEM_UID_GID);
    EXPECT_FALSE(HasMkdirLog("readback mismatch"));
    EXPECT_FALSE(HasMkdirLog("failed change owner"));
    (void)chown(dir, 0, 0);
    RemoveTestPath(dir);
}

/*
 * @tc.name: TestCommonMkdirExistingDirOwner
 * @tc.desc: 验证已存在目录按 cfg 声明的 mode/owner 修正
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirExistingDirOwner, TestSize.Level1)
{
    if (geteuid() != 0) {
        GTEST_SKIP() << "mkdir owner requires root";
    }
    const char *dir = "/data/init_ut/mkdir_owner_exist";
    RemoveTestPath(dir);
    ASSERT_EQ(mkdir(dir, TEST_PRIVATE_DIR_MODE), 0);
    ASSERT_EQ(chown(dir, SYSTEM_UID_GID, SYSTEM_UID_GID), 0);
    g_restoreCalls = 0;
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_owner_exist 0755 1000 1000");
    struct stat st = { 0 };
    ASSERT_EQ(stat(dir, &st), 0);
    EXPECT_EQ(st.st_mode & DIR_PERMISSION_MASK, 0755);
    EXPECT_EQ(st.st_uid, SYSTEM_UID_GID);
    EXPECT_EQ(st.st_gid, SYSTEM_UID_GID);
    EXPECT_EQ(g_restoreCalls, 0);
    EXPECT_FALSE(HasMkdirLog("existing type="));
    EXPECT_FALSE(HasMkdirLog("readback mismatch"));
    (void)chown(dir, 0, 0);
    RemoveTestPath(dir);
}

/*
 * @tc.name: TestCommonMkdirEexistDanglingSymlink
 * @tc.desc: 验证 EEXIST 悬空软链接被拒绝，链接及不存在目标均不变
 * @tc.type: SECU
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirEexistDanglingSymlink, TestSize.Level1)
{
    const char *link = "/data/init_ut/mkdir_link_dangling";
    const char *target = "/data/init_ut/mkdir_link_dangling_target";
    (void)unlink(link);
    (void)unlink(target);
    ASSERT_EQ(symlink(target, link), 0);
    if (geteuid() == 0) {
        ASSERT_EQ(lchown(link, SYSTEM_UID_GID, SYSTEM_UID_GID), 0);
    }
    struct stat before = { 0 };
    ASSERT_EQ(lstat(link, &before), 0);
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_link_dangling 0755");
    struct stat after = { 0 };
    ASSERT_EQ(lstat(link, &after), 0);
    EXPECT_TRUE(S_ISLNK(after.st_mode));
    EXPECT_EQ(after.st_mode & DIR_PERMISSION_MASK, before.st_mode & DIR_PERMISSION_MASK);
    EXPECT_EQ(after.st_uid, before.st_uid);
    EXPECT_EQ(after.st_gid, before.st_gid);
    EXPECT_NE(lstat(target, &after), 0);
    EXPECT_TRUE(HasMkdirLog(link));
    EXPECT_TRUE(HasMkdirLog("err=20"));
    EXPECT_TRUE(HasMkdirLog("category=path"));
    EXPECT_TRUE(HasMkdirLog("existing type=symbolic link"));
    (void)unlink(link);
}

/*
 * @tc.name: TestCommonMkdirEexistFifo
 * @tc.desc: 验证 EEXIST FIFO 被识别且属性不被修改
 * @tc.type: SECU
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirEexistFifo, TestSize.Level1)
{
    const char *fifo = "/data/init_ut/mkdir_fifo";
    (void)unlink(fifo);
    ASSERT_EQ(mkfifo(fifo, TEST_SPECIAL_MODE), 0);
    ASSERT_EQ(chmod(fifo, TEST_SPECIAL_MODE), 0);
    struct stat before = { 0 };
    ASSERT_EQ(lstat(fifo, &before), 0);
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_fifo 0755");
    struct stat after = { 0 };
    ASSERT_EQ(lstat(fifo, &after), 0);
    EXPECT_TRUE(S_ISFIFO(after.st_mode));
    EXPECT_EQ(after.st_mode & DIR_PERMISSION_MASK, before.st_mode & DIR_PERMISSION_MASK);
    EXPECT_EQ(after.st_uid, before.st_uid);
    EXPECT_EQ(after.st_gid, before.st_gid);
    EXPECT_TRUE(HasMkdirLog("existing type=fifo"));
    (void)unlink(fifo);
}

/*
 * @tc.name: TestCommonMkdirEexistSocket
 * @tc.desc: 验证 EEXIST socket 文件被识别且属性不被修改
 * @tc.type: SECU
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirEexistSocket, TestSize.Level1)
{
    const char *sockPath = "/data/init_ut/mkdir_socket";
    (void)unlink(sockPath);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_GE(fd, 0);
    struct sockaddr_un addr = { 0 };
    addr.sun_family = AF_UNIX;
    ASSERT_EQ(strncpy_s(addr.sun_path, sizeof(addr.sun_path), sockPath, sizeof(addr.sun_path) - 1), EOK);
    int bindRet = ::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), static_cast<socklen_t>(sizeof(addr)));
    if (bindRet != 0) {
        (void)close(fd);
        (void)unlink(sockPath);
    }
    ASSERT_EQ(bindRet, 0);
    EXPECT_EQ(close(fd), 0);
    ASSERT_EQ(chmod(sockPath, TEST_SPECIAL_MODE), 0);
    struct stat before = { 0 };
    ASSERT_EQ(lstat(sockPath, &before), 0);
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_socket 0755");
    struct stat after = { 0 };
    ASSERT_EQ(lstat(sockPath, &after), 0);
    EXPECT_TRUE(S_ISSOCK(after.st_mode));
    EXPECT_EQ(after.st_mode & DIR_PERMISSION_MASK, before.st_mode & DIR_PERMISSION_MASK);
    EXPECT_EQ(after.st_uid, before.st_uid);
    EXPECT_EQ(after.st_gid, before.st_gid);
    EXPECT_TRUE(HasMkdirLog("existing type=socket"));
    (void)unlink(sockPath);
}

/*
 * @tc.name: TestCommonMkdirEexistSpecialTypeMock
 * @tc.desc: 验证 block/character/fifo/socket/unknown 对象类型日志
 * @tc.type: SECU
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirEexistSpecialTypeMock, TestSize.Level1)
{
    const char *file = "/data/init_ut/mkdir_type_mock";
    const struct {
        mode_t mode;
        const char *type;
    } cases[] = {
        { S_IFBLK | TEST_SPECIAL_MODE, "block device" },
        { S_IFCHR | TEST_SPECIAL_MODE, "character device" },
        { S_IFIFO | TEST_SPECIAL_MODE, "fifo" },
        { S_IFSOCK | TEST_SPECIAL_MODE, "socket" },
        { 0, "unknown" },
    };
    for (const auto &testCase : cases) {
        RemoveTestPath(file);
        int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, TEST_FILE_MODE);
        ASSERT_GE(fd, 0);
        EXPECT_EQ(close(fd), 0);
        g_lstatMockCalls = 0;
        g_lstatMockUseMode = true;
        g_lstatMockMode = testCase.mode;
        InstallLstatMock();
        ResetMkdirLogCapture();
        DoCmdByName("mkdir ", "/data/init_ut/mkdir_type_mock 0755");
        std::string typeLog = std::string("existing type=") + testCase.type;
        EXPECT_TRUE(HasMkdirLog(file));
        EXPECT_TRUE(HasMkdirLog("err=20"));
        EXPECT_TRUE(HasMkdirLog("category=path"));
        EXPECT_TRUE(HasMkdirLog(typeLog.c_str()));
        ResetMkdirWrappersForAssert();
    }
    RemoveTestPath(file);
}

/*
 * @tc.name: TestCommonMkdirReadbackOwnerMismatch
 * @tc.desc: 验证 owner 回读不一致时输出期望值与实际值
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirReadbackOwnerMismatch, TestSize.Level1)
{
    const char *path = "/data/init_ut/mkdir_readback_owner";
    RemoveTestPath(path);
    ASSERT_EQ(mkdir(path, TEST_PRIVATE_DIR_MODE), 0);
    struct stat before = { 0 };
    ASSERT_EQ(stat(path, &before), 0);
    uid_t expectOwner = (before.st_uid == SYSTEM_UID_GID) ? SYSTEM_UID_GID + 1 : SYSTEM_UID_GID;
    gid_t expectGroup = (before.st_gid == SYSTEM_UID_GID) ? SYSTEM_UID_GID + 1 : SYSTEM_UID_GID;
    InstallLchownMock();
    g_fchownMockRet = 0;
    ResetMkdirLogCapture();
    char cmd[PATH_MAX + 64] = { 0 };
    int ret = snprintf_s(cmd, sizeof(cmd), sizeof(cmd) - 1, "%s 0755 %u %u",
        path, static_cast<unsigned int>(expectOwner), static_cast<unsigned int>(expectGroup));
    ASSERT_GT(ret, 0);
    DoCmdByName("mkdir ", cmd);
    EXPECT_EQ(g_fchownMockCalls, 1);
    char expectLog[128] = { 0 };
    ret = snprintf_s(expectLog, sizeof(expectLog), sizeof(expectLog) - 1,
        "expect mode=00755 uid=%u gid=%u",
        static_cast<unsigned int>(expectOwner), static_cast<unsigned int>(expectGroup));
    ASSERT_GT(ret, 0);
    char actualLog[128] = { 0 };
    ret = snprintf_s(actualLog, sizeof(actualLog), sizeof(actualLog) - 1,
        "actual mode=00755 uid=%u gid=%u",
        static_cast<unsigned int>(before.st_uid), static_cast<unsigned int>(before.st_gid));
    ASSERT_GT(ret, 0);
    EXPECT_TRUE(HasMkdirLog("readback mismatch"));
    EXPECT_TRUE(HasMkdirLog(expectLog));
    EXPECT_TRUE(HasMkdirLog(actualLog));
    ResetMkdirWrappersForAssert();
    RemoveTestPath(path);
}

/*
 * @tc.name: TestCommonMkdirReadbackGidMismatch
 * @tc.desc: 验证 uid 一致、gid 不一致时输出回读不一致日志
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirReadbackGidMismatch, TestSize.Level1)
{
    const char *path = "/data/init_ut/mkdir_gid_mismatch";
    RemoveTestPath(path);
    InstallDoMkDirMocks();
    InstallLchownMock();
    InstallFstatatMock();
    InstallFscryptMock();
    g_fchownMockRet = 0;
    g_fscryptMockRet = 0;
    g_fstatMockUseStat = true;
    g_fstatMockUid = SYSTEM_UID_GID;
    g_fstatMockGid = 0;
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_gid_mismatch 0755 1000 1000");
    EXPECT_TRUE(HasMkdirLog("readback mismatch"));
    EXPECT_TRUE(HasMkdirLog("expect mode=00755 uid=1000 gid=1000"));
    EXPECT_TRUE(HasMkdirLog("actual mode=00755 uid=1000 gid=0"));
    ResetMkdirWrappersForAssert();
    RemoveTestPath(path);
}

/*
 * @tc.name: TestCommonMkdirFstatatReadbackFailure
 * @tc.desc: 验证回读失败仅记录日志且不中断启动
 * @tc.type: RELI
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirFstatatReadbackFailure, TestSize.Level1)
{
    const char *path = "/data/init_ut/mkdir_fstat_fail";
    RemoveTestPath(path);
    InstallFstatatMock();
    g_fstatMockRet = -1;
    g_fstatMockErrno = EIO;
    g_restoreCalls = 0;
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_fstat_fail 0755");
    EXPECT_EQ(g_fstatMockCalls, 1);
    EXPECT_TRUE(HasMkdirLog("readback fstatat"));
    EXPECT_TRUE(HasMkdirLog("err=5"));
    EXPECT_TRUE(HasMkdirLog("category=filesystem"));
    EXPECT_EQ(g_restoreCalls, 1);
    ResetMkdirWrappersForAssert();
    RemoveTestPath(path);
}

/*
 * @tc.name: TestCommonMkdirFchmodatFailure
 * @tc.desc: 验证 fchmodat 失败记录分类日志且继续回读
 * @tc.type: RELI
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirFchmodatFailure, TestSize.Level1)
{
    const char *path = "/data/init_ut/mkdir_fchmod_fail";
    RemoveTestPath(path);
    ASSERT_EQ(mkdir(path, TEST_PRIVATE_DIR_MODE), 0);
    InstallDoMkDirMocks();
    g_fchmodMockRet = -1;
    g_fchmodMockErrno = EPERM;
    g_restoreCalls = 0;
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_fchmod_fail 0755");
    EXPECT_EQ(g_fchmodMockCalls, 1);
    EXPECT_TRUE(HasMkdirLog("DoMkDir failed"));
    EXPECT_TRUE(HasMkdirLog("err=1"));
    EXPECT_TRUE(HasMkdirLog("category=permission"));
    EXPECT_TRUE(HasMkdirLog("readback mismatch"));
    EXPECT_EQ(g_restoreCalls, 0);
    ResetMkdirWrappersForAssert();
    RemoveTestPath(path);
}

/*
 * @tc.name: TestCommonMkdirSetOwnerFailure
 * @tc.desc: 验证 lchown 失败记录分类日志且继续回读
 * @tc.type: RELI
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirSetOwnerFailure, TestSize.Level1)
{
    const char *path = "/data/init_ut/mkdir_chown_fail";
    RemoveTestPath(path);
    ASSERT_EQ(mkdir(path, TEST_PRIVATE_DIR_MODE), 0);
    InstallLchownMock();
    g_fchownMockRet = -1;
    g_fchownMockErrno = EACCES;
    g_restoreCalls = 0;
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_chown_fail 0755 1000 1000");
    EXPECT_EQ(g_fchownMockCalls, 1);
    EXPECT_TRUE(HasMkdirLog("failed change owner"));
    EXPECT_TRUE(HasMkdirLog("err=13"));
    EXPECT_TRUE(HasMkdirLog("category=permission"));
    EXPECT_TRUE(HasMkdirLog("readback mismatch"));
    ResetMkdirWrappersForAssert();
    RemoveTestPath(path);
}

/*
 * @tc.name: TestCommonMkdirExistingPathLstatFailure
 * @tc.desc: 验证 EEXIST 后 lstat 失败记录原始错误码并停止后续流程
 * @tc.type: RELI
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirExistingPathLstatFailure, TestSize.Level1)
{
    const char *path = "/data/init_ut/mkdir_lstat_fail";
    RemoveTestPath(path);
    ASSERT_EQ(mkdir(path, TEST_PRIVATE_DIR_MODE), 0);
    g_lstatMockCalls = 0;
    g_lstatMockFailAt = 1;
    g_lstatMockErrno = EACCES;
    InstallLstatMock();
    g_restoreCalls = 0;
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_lstat_fail 0755");
    EXPECT_EQ(g_lstatMockCalls, 1);
    EXPECT_TRUE(HasMkdirLog("lstat failed"));
    EXPECT_TRUE(HasMkdirLog("err=13"));
    EXPECT_TRUE(HasMkdirLog("category=permission"));
    EXPECT_EQ(g_restoreCalls, 0);
    ResetMkdirWrappersForAssert();
    RemoveTestPath(path);
}

/*
 * @tc.name: TestCommonMkdirRestorePathRules
 * @tc.desc: 验证已存在目录的 restore 路径规则（/dev、/data、/data/service/el1、其他）
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirRestorePathRules, TestSize.Level1)
{
    const struct {
        const char *path;
        int expectRestore;
    } cases[] = {
        { "/dev/mkdir_restore_dev", 0 },
        { "/data/init_ut/mkdir_restore_existing", 0 },
        { "/data/service/el1", 1 },
        { "/tmp/mkdir_restore_tmp", 1 },
    };
    for (const auto &testCase : cases) {
        SetMkdirSequence(-1, EEXIST, 0, 0);
        g_lstatMockCalls = 0;
        g_lstatMockUseMode = true;
        g_lstatMockMode = S_IFDIR | 0755;
        InstallLstatMock();
        g_restoreCalls = 0;
        ResetMkdirLogCapture();
        DoCmdByName("mkdir ", testCase.path);
        EXPECT_EQ(g_mkdirMockCalls, 1) << testCase.path;
        EXPECT_EQ(g_restoreCalls, testCase.expectRestore) << testCase.path;
    }
    ResetMkdirWrappersForAssert();
}

/*
 * @tc.name: TestCommonMkdirFscryptPrecheck
 * @tc.desc: 验证非目录不设置 fscrypt、真实目录设置一次
 * @tc.type: SECU
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirFscryptPrecheck, TestSize.Level1)
{
    const char *file = "/data/init_ut/mkdir_precheck_file";
    const char *dir = "/data/init_ut/mkdir_fscrypt_dir";
    RemoveTestPath(file);
    RemoveTestPath(dir);
    int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, TEST_FILE_MODE);
    ASSERT_GE(fd, 0);
    EXPECT_EQ(close(fd), 0);
    InstallFscryptMock();
    g_fscryptMockRet = 0;
    g_fscryptCalls = 0;
    g_restoreCalls = 0;
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_precheck_file 0755 0 0");
    EXPECT_EQ(g_fscryptCalls, 0);
    EXPECT_EQ(g_restoreCalls, 0);

    g_fscryptCalls = 0;
    g_restoreCalls = 0;
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_fscrypt_dir 0755 0 0");
    EXPECT_EQ(g_fscryptCalls, 1);
    EXPECT_EQ(g_restoreCalls, 1);
    struct stat st = { 0 };
    ASSERT_EQ(stat(dir, &st), 0);
    EXPECT_TRUE(S_ISDIR(st.st_mode));
    EXPECT_EQ(st.st_mode & DIR_PERMISSION_MASK, 0755);
    EXPECT_FALSE(HasMkdirLog("failed set file fscrypt"));
    ResetMkdirWrappersForAssert();
    RemoveTestPath(file);
    RemoveTestPath(dir);
}

/*
 * @tc.name: TestCommonMkdirFscryptFailure
 * @tc.desc: 验证 fscrypt 设置失败仅记录日志且不中断启动
 * @tc.type: RELI
 */
HWTEST_F(CmdsUnitTest, TestCommonMkdirFscryptFailure, TestSize.Level1)
{
    const char *dir = "/data/init_ut/mkdir_fscrypt_dir";
    RemoveTestPath(dir);
    InstallFscryptMock();
    g_fscryptMockRet = -1;
    g_fscryptCalls = 0;
    ResetMkdirLogCapture();
    DoCmdByName("mkdir ", "/data/init_ut/mkdir_fscrypt_dir 0755 0 0");
    EXPECT_EQ(g_fscryptCalls, 1);
    EXPECT_TRUE(HasMkdirLog("failed set file fscrypt"));
    struct stat st = { 0 };
    ASSERT_EQ(stat(dir, &st), 0);
    EXPECT_TRUE(S_ISDIR(st.st_mode));
    ResetMkdirWrappersForAssert();
    RemoveTestPath(dir);
}

HWTEST_F(CmdsUnitTest, TestCommonChown, TestSize.Level1)
{
    if (geteuid() != 0) {
        GTEST_SKIP() << "chown to system requires root";
    }
    const char *dir = "/data/init_ut/chown_dir";
    RemoveTestPath(dir);
    ASSERT_EQ(mkdir(dir, DEFAULT_DIR_MODE), 0);
    DoCmdByName("chown ", "system system /data/init_ut/chown_dir");
    struct stat info = { 0 };
    ASSERT_EQ(stat(dir, &info), 0);
    EXPECT_EQ(info.st_uid, SYSTEM_UID_GID);
    EXPECT_EQ(info.st_gid, SYSTEM_UID_GID);

    DoCmdByName("chown ", "error error /data/init_ut/chown_dir");
    ASSERT_EQ(stat(dir, &info), 0);
    EXPECT_EQ(info.st_uid, SYSTEM_UID_GID);
    EXPECT_EQ(info.st_gid, SYSTEM_UID_GID);
    (void)chown(dir, 0, 0);
    RemoveTestPath(dir);
}

/*
 * @tc.name: TestCommonChownFollowSymlink
 * @tc.desc: 验证 chown 保持跟随软链接语义，修改目标对象属主，链接本身不变
 * @tc.type: SECU
 */
HWTEST_F(CmdsUnitTest, TestCommonChownFollowSymlink, TestSize.Level1)
{
    if (geteuid() != 0) {
        GTEST_SKIP() << "chown to system requires root";
    }
    const char *target = "/data/init_ut/chown_target";
    const char *link = "/data/init_ut/chown_link";
    RemoveTestPath(link);
    RemoveTestPath(target);
    int fd = open(target, O_WRONLY | O_CREAT | O_TRUNC, TEST_FILE_MODE);
    ASSERT_GE(fd, 0);
    EXPECT_EQ(close(fd), 0);
    ASSERT_EQ(symlink(target, link), 0);
    struct stat linkBefore = { 0 };
    ASSERT_EQ(lstat(link, &linkBefore), 0);
    DoCmdByName("chown ", "system system /data/init_ut/chown_link");
    struct stat targetAfter = { 0 };
    struct stat linkAfter = { 0 };
    ASSERT_EQ(stat(target, &targetAfter), 0);
    ASSERT_EQ(lstat(link, &linkAfter), 0);
    EXPECT_EQ(targetAfter.st_uid, SYSTEM_UID_GID);
    EXPECT_EQ(targetAfter.st_gid, SYSTEM_UID_GID);
    EXPECT_EQ(linkAfter.st_uid, linkBefore.st_uid);
    EXPECT_EQ(linkAfter.st_gid, linkBefore.st_gid);
    RemoveTestPath(link);
    RemoveTestPath(target);
}

/*
 * @tc.name: TestCommonChownInvalidOwner
 * @tc.desc: 验证 chown 非法 owner/group 记录失败日志且属性不变
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, TestCommonChownInvalidOwner, TestSize.Level1)
{
    InstallAccountMocks();
    ResetMkdirLogCapture();
    DoCmdByName("chown ", "error error /data/init_ut/chown_mock_fail");
    EXPECT_TRUE(HasMkdirLog("SetOwner invalid uid"));
    EXPECT_TRUE(HasMkdirLog("change owner for"));
    DoCmdByName("chown ", "system error /data/init_ut/chown_mock_fail");
    EXPECT_TRUE(HasMkdirLog("SetOwner invalid gid"));
    EXPECT_TRUE(HasMkdirLog("change owner for"));

    // Mocked chown success covers DoChown's non-error branch deterministically.
    InstallChownMock();
    g_chownMockRet = 0;
    ResetMkdirLogCapture();
    DoCmdByName("chown ", "system system /data/init_ut/chown_mock_fail");
    EXPECT_FALSE(HasMkdirLog("change owner for"));
    ResetMkdirWrappersForAssert();
}

/*
 * @tc.name: TestCommonChownFailure
 * @tc.desc: 验证 chown 失败记录原始错误码
 * @tc.type: RELI
 */
HWTEST_F(CmdsUnitTest, TestCommonChownFailure, TestSize.Level1)
{
    const char *path = "/data/init_ut/chown_mock_fail_path";
    RemoveTestPath(path);
    InstallAccountMocks();
    InstallChownMock();
    g_chownMockRet = -1;
    g_chownMockErrno = ENOENT;
    ResetMkdirLogCapture();
    DoCmdByName("chown ", "system system /data/init_ut/chown_mock_fail_path");
    // Wording-agnostic: see TestCommonChownInvalidOwner.
    EXPECT_TRUE(HasMkdirLog("change owner for"));
    EXPECT_TRUE(HasMkdirLog("err 2"));
    ResetMkdirWrappersForAssert();
}

/*
 * @tc.name: TestCommonOwnerGuardBranches
 * @tc.desc: 验证 SetOwner 字符串入口与 SetOwnerNoFollowById 的防御性参数分支
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, TestCommonOwnerGuardBranches, TestSize.Level1)
{
    const char *path = "/data/init_ut/owner_guard";
    RemoveTestPath(path);
    ASSERT_EQ(mkdir(path, TEST_PRIVATE_DIR_MODE), 0);
    EXPECT_EQ(TestSetOwner(nullptr, "system", "system"), -1);
    EXPECT_EQ(TestSetOwner(path, nullptr, "system"), -1);
    EXPECT_EQ(TestSetOwner(path, "system", nullptr), -1);
    EXPECT_EQ(TestSetOwner(path, "error", "system"), -1);
    EXPECT_EQ(TestSetOwner(path, "system", "error"), -1);
    EXPECT_EQ(TestSetOwnerNoFollowById(nullptr, geteuid(), getegid()), -1);
    RemoveTestPath(path);
}

/*
 * @tc.name: TestCommonGetFileTypeString
 * @tc.desc: 验证文件类型字符串映射覆盖目录与全部类型
 * @tc.type: FUNC
 */
HWTEST_F(CmdsUnitTest, TestCommonGetFileTypeString, TestSize.Level1)
{
    EXPECT_STREQ(TestGetFileTypeString(S_IFDIR), "directory");
    EXPECT_STREQ(TestGetFileTypeString(S_IFREG), "regular file");
    EXPECT_STREQ(TestGetFileTypeString(S_IFLNK), "symbolic link");
    EXPECT_STREQ(TestGetFileTypeString(S_IFIFO), "fifo");
    EXPECT_STREQ(TestGetFileTypeString(S_IFSOCK), "socket");
    EXPECT_STREQ(TestGetFileTypeString(S_IFBLK), "block device");
    EXPECT_STREQ(TestGetFileTypeString(S_IFCHR), "character device");
    EXPECT_STREQ(TestGetFileTypeString(0), "unknown");
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
    EXPECT_STREQ(GetCmdKey(1), "mkdir ");
    int cmdIndex = -1;
    ASSERT_NE(GetMatchCmd("mkdir ", &cmdIndex), nullptr);
    DoCmdByIndex(cmdIndex, "/data/init_ut/test_cmd_dir0", nullptr);
    int ret = access("/data/init_ut/test_cmd_dir0", F_OK);
    EXPECT_EQ(ret, 0);

    ASSERT_NE(GetMatchCmd("exec ", &cmdIndex), nullptr);
    DoCmdByIndex(cmdIndex, "sleep 1", nullptr);
    ASSERT_NE(GetMatchCmd("symlink ", &cmdIndex), nullptr);
    DoCmdByIndex(cmdIndex, "test", nullptr); // Missing second argument must be rejected.
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
    void *libGpuKiaHandle = dlopen(GPU_RECLAIM_IMPL_SO, RTLD_NOW);
    if (!libGpuKiaHandle) {
        EXPECT_EQ(ret, false);
    } else {
        dlclose(libGpuKiaHandle);
        EXPECT_EQ(ret, true);
    }
}

// Branch 1: child exits quickly → returns the child's exit status.
HWTEST_F(CmdsUnitTest, WaitPidTimeout_ChildExits, TestSize.Level1)
{
    constexpr int testExitCode = 42;
    constexpr int testWaitTimeoutMs = 3000;
    pid_t pid = fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        _exit(testExitCode);
    }
    int status = WaitPidTimeout(pid, testWaitTimeoutMs);
    EXPECT_NE(status, -1);
    EXPECT_EQ(WEXITSTATUS(status), testExitCode);
}

// Branch 2: invalid pid → waitpid returns -1 → returns -1 immediately.
HWTEST_F(CmdsUnitTest, WaitPidTimeout_InvalidPid, TestSize.Level1)
{
    constexpr pid_t invalidTestPid = 999999;
    constexpr int testShortTimeoutMs = 1000;
    int status = WaitPidTimeout(invalidTestPid, testShortTimeoutMs);
    EXPECT_EQ(status, -1);
}

// Branch 3: child hangs → timeout → SIGKILL + reap → returns -1.
HWTEST_F(CmdsUnitTest, WaitPidTimeout_TimeoutKill, TestSize.Level1)
{
    constexpr int testKillTimeoutMs = 300;
    pid_t pid = fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        for (;;) {}
    }
    int status = WaitPidTimeout(pid, testKillTimeoutMs);
    EXPECT_EQ(status, -1);
}
} // namespace init_ut
