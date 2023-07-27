/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "init_eng.h"
#include "init_utils.h"
#include "param_stub.h"
#include "bootstage.h"
#include "securec.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
static const std::string SRC_FILE_PATH = STARTUP_INIT_UT_PATH"/eng/source/test.txt";
static const std::string TARGET_PATH = STARTUP_INIT_UT_PATH"/eng/link_name";
static const std::string ENG_ROOT_PATH = STARTUP_INIT_UT_PATH"/eng/";

static bool RemoveDir(const std::string &path)
{
    if (path.empty()) {
        return false;
    }
    std::string strPath = path;
    if (strPath.at(strPath.length() - 1) != '/') {
        strPath.append("/");
    }
    DIR *d = opendir(strPath.c_str());
    if (d != nullptr) {
        struct dirent *dt = nullptr;
        dt = readdir(d);
        while (dt != nullptr) {
            if (strcmp(dt->d_name, "..") == 0 || strcmp(dt->d_name, ".") == 0) {
                dt = readdir(d);
                continue;
            }
            struct stat st {};
            auto file_name = strPath + std::string(dt->d_name);
            stat(file_name.c_str(), &st);
            if (S_ISDIR(st.st_mode)) {
                RemoveDir(file_name);
            } else {
                remove(file_name.c_str());
            }
            dt = readdir(d);
        }
        closedir(d);
    }
    return rmdir(strPath.c_str()) == 0 ? true : false;
}

static bool IsFileExist(const std::string &path)
{
    if (path.empty()) {
        return false;
    }
    struct stat st {};
    if (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
        return true;
    }
    return false;
}

static bool IsDirExist(const std::string &path)
{
    if (path.empty()) {
        return false;
    }
    struct stat st {};
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    return false;
}

class EngUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(EngUnitTest, TestFilesOverlay, TestSize.Level1)
{
    bool isDel = false;
    bool isExist = IsDirExist(ENG_ROOT_PATH.c_str());
    if (isExist) {
        isDel = RemoveDir(ENG_ROOT_PATH.c_str());
        EXPECT_EQ(isDel, true);
    }
    isExist = IsDirExist(TARGET_PATH.c_str());
    if (isExist) {
        isDel = RemoveDir(TARGET_PATH.c_str());
        EXPECT_EQ(isDel, true);
    }
    DebugFilesOverlay(ENG_ROOT_PATH.c_str(), TARGET_PATH.c_str());

    CreateTestFile(SRC_FILE_PATH.c_str(), "test");
    isExist = IsFileExist(SRC_FILE_PATH.c_str());
    EXPECT_EQ(isExist, true);

    DebugFilesOverlay(SRC_FILE_PATH.c_str(), TARGET_PATH.c_str());
    isExist = IsFileExistWithType(SRC_FILE_PATH.c_str(), TYPE_REG);
    EXPECT_EQ(isExist, true);


    if (IsFileExistWithType(TARGET_PATH.c_str(), TYPE_LINK)) {
        if (unlink(TARGET_PATH.c_str()) < 0) {
            EXPECT_TRUE(false);
        }
    }
    int ret = symlink(ENG_ROOT_PATH.c_str(), TARGET_PATH.c_str());
    EXPECT_EQ(ret, 0);
    isExist = IsFileExistWithType(TARGET_PATH.c_str(), TYPE_LINK);
    EXPECT_EQ(isExist, true);
    DebugFilesOverlay(TARGET_PATH.c_str(), ENG_ROOT_PATH.c_str());
    EXPECT_EQ(ret, 0);
}

HWTEST_F(EngUnitTest, TestBindMountFile, TestSize.Level1)
{
    BindMountFile("data/init_ut", "");
    BindMountFile("data", "target");
    BindMountFile("/data/init_ut//", "/");
    BindMountFile("/data/init_ut", "/");
    BindMountFile("/data", "/");
    BindMountFile("/data/", "/");

    bool isExist = false;
    if (!IsFileExist(SRC_FILE_PATH.c_str())) {
        CreateTestFile(SRC_FILE_PATH.c_str(), "test reg file mount");
        isExist = IsFileExist(SRC_FILE_PATH.c_str());
        EXPECT_EQ(isExist, true);
        BindMountFile(SRC_FILE_PATH.c_str(), "/");
    }
    BindMountFile(SRC_FILE_PATH.c_str(), "/");

    if (IsFileExist(SRC_FILE_PATH.c_str())) {
        RemoveDir(STARTUP_INIT_UT_PATH"/eng/source");
        isExist = IsFileExist(SRC_FILE_PATH.c_str());
        EXPECT_EQ(isExist, false);
    }
    if (IsFileExistWithType(TARGET_PATH.c_str(), TYPE_LINK)) {
        if (unlink(TARGET_PATH.c_str()) < 0) {
            EXPECT_TRUE(false);
        }
    }

    bool isLinkFile = IsFileExist(TARGET_PATH.c_str());
    EXPECT_EQ(isLinkFile, false);
    BindMountFile(SRC_FILE_PATH.c_str(), TARGET_PATH.c_str());

    int ret = symlink(SRC_FILE_PATH.c_str(), TARGET_PATH.c_str());
    EXPECT_EQ(ret, 0);
    isLinkFile = IsFileExistWithType(TARGET_PATH.c_str(), TYPE_LINK);
    EXPECT_EQ(isLinkFile, true);
    BindMountFile(SRC_FILE_PATH.c_str(), TARGET_PATH.c_str());
    BindMountFile(TARGET_PATH.c_str(), SRC_FILE_PATH.c_str());
}

HWTEST_F(EngUnitTest, TestMountCmd, TestSize.Level1)
{
    char mountCmd[MOUNT_CMD_MAX_LEN] = {};
    MountEngPartitions();
    BuildMountCmd(mountCmd, MOUNT_CMD_MAX_LEN, "/eng/source", "/eng/target", "ext4");
    BuildMountCmd(mountCmd, 0, "/eng/source", "/eng/target", "ext4");
}

HWTEST_F(EngUnitTest, TestFileType, TestSize.Level1)
{
    std::string targetFile = "/data/init_ut/eng/target_file";
    std::string linkName = "/data/init_ut/eng/link_name_test";
    bool isExist = false;

    if (!IsFileExist(SRC_FILE_PATH.c_str())) {
        CreateTestFile(SRC_FILE_PATH.c_str(), "test");
        isExist = IsFileExist(SRC_FILE_PATH.c_str());
        EXPECT_EQ(isExist, true);
    }

    EXPECT_EQ(IsFileExistWithType(SRC_FILE_PATH.c_str(), TYPE_REG), true);
    EXPECT_EQ(IsFileExistWithType(STARTUP_INIT_UT_PATH"/eng", TYPE_DIR), true);

    EXPECT_EQ(IsFileExistWithType(STARTUP_INIT_UT_PATH"/eng", TYPE_LINK), false);
    EXPECT_EQ(IsFileExistWithType(STARTUP_INIT_UT_PATH"/eng", TYPE_REG), false);
    EXPECT_EQ(IsFileExistWithType(STARTUP_INIT_UT_PATH"/eng", TYPE_ANY), true);
    EXPECT_EQ(IsFileExistWithType(SRC_FILE_PATH.c_str(), TYPE_DIR), false);

    if (IsFileExist(targetFile)) {
        if (unlink(targetFile.c_str()) < 0) {
            std::cout << "Failed to unlink file " << targetFile << " err = " << errno << std::endl;
            EXPECT_TRUE(false);
        }
    }
    int fd = open(targetFile.c_str(), O_CREAT | O_CLOEXEC | O_WRONLY, 0644);
    if (fd < 0) {
        std::cout << "Failed to create file " << targetFile << " err = " << errno << std::endl;
        EXPECT_TRUE(false);
    } else {
        std::string buffer = "hello";
        write(fd, buffer.c_str(), buffer.size());
        close(fd); // avoid leak
    }

    if (IsFileExist(linkName)) {
        if (unlink(linkName.c_str()) < 0) {
            std::cout << "Failed to unlink file " << linkName << " err = " << errno << std::endl;
            EXPECT_TRUE(false);
        }
    }

    int ret = symlink(targetFile.c_str(), linkName.c_str());
    EXPECT_EQ(ret, 0);
    bool isFileExist = IsFileExistWithType(linkName.c_str(), TYPE_LINK);
    EXPECT_EQ(isFileExist, true);

    isFileExist = IsFileExistWithType("/eng/target", TYPE_LINK);
    EXPECT_EQ(isFileExist, false);

    isFileExist = IsFileExistWithType("/eng/target", TYPE_REG);
    EXPECT_EQ(isFileExist, false);
}

HWTEST_F(EngUnitTest, TestHook, TestSize.Level1)
{
    HookMgrExecute(GetBootStageHookMgr(), INIT_GLOBAL_INIT, nullptr, nullptr);
    PrepareCmdLineData();
    HookMgrExecute(GetBootStageHookMgr(), INIT_GLOBAL_INIT, nullptr, nullptr);
    const char *cmdLine = "ohos.boot.root_package=off ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
    HookMgrExecute(GetBootStageHookMgr(), INIT_GLOBAL_INIT, nullptr, nullptr);
}
} // namespace init_ut
