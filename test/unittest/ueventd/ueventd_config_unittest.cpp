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
#include <cerrno>
#include <dirent.h>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "init_unittest.h"
#include "init_utils.h"
#include "ueventd_read_cfg.h"
#include "ueventd_parameter.h"

using namespace std;
using namespace testing::ext;

const char *TEST_PATH = "/data/ueventd_ut";
namespace ueventd_ut {
static bool FileExists(const std::string &file)
{
    bool isExist = false;
    if (!file.empty()) {
        struct stat st = {};
        if (stat(file.c_str(), &st) == 0) {
            isExist = true;
        }
    }
    return isExist;
}

static bool CheckNeedToWrite(const std::string &section, const std::string &configFile)
{
    ifstream cfg(configFile);
    if (!cfg) {
        cout << "open configFile stream failed." << endl;
        return false;
    }
    std::string lastLineContent;
    while (std::getline(cfg, lastLineContent)) {
    if (strcmp(lastLineContent.c_str(), section.c_str()) == 0) {
            return false;
        }
    }
    return true;
}

static bool IsDir(const std::string &path)
{
    struct stat st {};
    if (stat(path.c_str(), &st) < 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

static bool DeleteDir(const std::string &path)
{
    auto pDir = std::unique_ptr<DIR, decltype(&closedir)>(opendir(path.c_str()), closedir);
    if (pDir == nullptr) {
        cout << "Can not open dir : " << path << ". " << errno << endl;
        return false;
    }

    struct dirent *dp = nullptr;
    if (pDir != nullptr) {
        while ((dp = readdir(pDir.get())) != nullptr) {
            std::string currentName(dp->d_name);
            if (currentName[0] != '.') {
                std::string tmpName(path);
                tmpName.append("/" + currentName);
                if (IsDir(tmpName)) {
                    DeleteDir(tmpName);
                }
                remove(tmpName.c_str());
            }
        }
        remove(path.c_str());
    }
    return true;
}

void GenerateConfigFiles(const std::string &section, const std::string &contents,
    const std::string &configFile)
{
    if (configFile.empty()) {
        return;
    }
    int flags = 0;
    // check if config file exists, if not exists create one
    if (FileExists(configFile)) {
        flags = O_RDWR | O_APPEND | O_CLOEXEC;
    } else {
        flags = O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC;
    }
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP; // 0640
    int fd = open(configFile.c_str(), flags, mode);
    if (fd < 0) {
        cout << "Open file " << configFile << " failed. " << errno << endl;
        return;
    }
    size_t rc = 0;
    if (!section.empty() && CheckNeedToWrite(section, configFile)) {
        rc = WriteAll(fd, section.c_str(), section.size());
        if (rc != section.size()) {
            cout << "Failed to write section " << section << " to file " << configFile << endl;
        }
    }
    if (!contents.empty()) {
        rc = WriteAll(fd, contents.c_str(), contents.size());
        if (rc != section.size()) {
            cout << "Failed to write section " << contents << " to file " << configFile << endl;
        }
    }
    close(fd);
    fd = -1;
}

class UeventdConfigUnitTest : public testing::Test {
public:
static void SetUpTestCase(void)
{
    int rc = mkdir(TEST_PATH, S_IRWXU | S_IRWXG | S_IRWXO);
    if (rc < 0) {
        if (errno != ENOENT) {
            cout << "Failed to prepare test directory." << errno << endl;
        } else {
            cout << " " << TEST_PATH << " already exists." << endl;
        }
    }
}

static void TearDownTestCase(void)
{
    DeleteDir(TEST_PATH);
}

void SetUp() {};
void TearDown() {};
};

HWTEST_F(UeventdConfigUnitTest, TestSectionConfigParse, TestSize.Level0)
{
    GenerateConfigFiles("[device]\n", "/dev/test 0666 1000 1000\n", "/data/ueventd_ut/valid.config");
    GenerateConfigFiles("[device]\n", "/dev/test1 0666 1000\n", "/data/ueventd_ut/valid.config");
    GenerateConfigFiles("[device]\n", "/dev/test2 0666 1000 1000 1000 1000\n", "/data/ueventd_ut/valid.config");
    GenerateConfigFiles("[sysfs]\n", "/dir/to/nothing attr_nowhere 0666 1000 1000\n",
        TEST_PATH "/valid.config");
    GenerateConfigFiles("[firmware]\n", "/etc\n", TEST_PATH"/valid.config");
    ParseUeventdConfigFile(TEST_PATH"/valid.config");
    uid_t uid = 0;
    gid_t gid = 0;
    mode_t mode = 0;
    GetDeviceNodePermissions("/dev/test", &uid, &gid, &mode);
    EXPECT_EQ(uid, 1000);
    EXPECT_EQ(gid, 1000);
    EXPECT_EQ(mode, 0666);
    uid = 999;
    gid = 999;
    mode = 0777;
    // /dev/test1 is not a valid item, miss gid
    // UGO will be default value.
    GetDeviceNodePermissions("/dev/test1", &uid, &gid, &mode);
    EXPECT_EQ(uid, 999);
    EXPECT_EQ(gid, 999);
    EXPECT_EQ(mode, 0777);
    // /dev/test2 is not a valid item, too much items
    // UGO will be default value.
    GetDeviceNodePermissions("/dev/test2", &uid, &gid, &mode);
    EXPECT_EQ(uid, 999);
    EXPECT_EQ(gid, 999);
    EXPECT_EQ(mode, 0777);
    ChangeSysAttributePermissions("/dev/test2");
}

HWTEST_F(UeventdConfigUnitTest, TestConfigEntry, TestSize.Level0)
{
    string file = "[device";
    int rc = ParseUeventConfig(const_cast<char*>(file.c_str())); // Invalid section
    EXPECT_EQ(rc, -1);
    file = "[unknown]";
    rc = ParseUeventConfig(const_cast<char*>(file.c_str()));  // Unknown section
    EXPECT_EQ(rc, -1);
    file = "[device]";
    rc = ParseUeventConfig(const_cast<char*>(file.c_str())); // valid section
    EXPECT_EQ(rc, 0);
}

HWTEST_F(UeventdConfigUnitTest, TestParameter, TestSize.Level0)
{
    GenerateConfigFiles("[device]\n", "/dev/testbinder1 0666 1000 1000 ohos.dev.binder\n", TEST_PATH"/valid.config");
    ParseUeventdConfigFile(TEST_PATH"/valid.config");
    GenerateConfigFiles("[device]\n", "/dev/testbinder2 0666 1000 1000 ohos.dev.binder\n", TEST_PATH"/valid.config");
    ParseUeventdConfigFile(TEST_PATH"/valid.config");
    GenerateConfigFiles("[device]\n", "/dev/testbinder3 0666 1000 1000 ohos.dev.binder\n", TEST_PATH"/valid.config");
    ParseUeventdConfigFile(TEST_PATH"/valid.config");
    SetUeventDeviceParameter("/dev/testbinder1", 0);
    SetUeventDeviceParameter("/dev/testbinder2", 0);
    SetUeventDeviceParameter("/dev/testbinder3", 0);
}
} // namespace ueventd_ut
