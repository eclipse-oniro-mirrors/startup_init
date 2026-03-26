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

#include <cerrno>
#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include "securec.h"
#include "hookmgr.h"

#include "param_stub.h"
#include "ueventd.h"
#include "ueventd_firmware_handler.h"

using namespace testing::ext;

namespace UeventdUt {
namespace {
    std::string g_testRoot{"/data/ueventd_firmware"};
}

class UeventdFirmwareHandlerUnitTest : public testing::Test {
public:
static void SetUpTestCase(void)
{
    struct stat st{};
    bool isExist = true;

    if (stat(g_testRoot.c_str(), &st) < 0) {
        if (errno != ENOENT) {
            std::cout << "Cannot get stat of " << g_testRoot << std::endl;
            ASSERT_TRUE(false);
        }
        isExist = false;
    }

    if (isExist) {
        RemoveDir(g_testRoot);
    }
    int ret = mkdir(g_testRoot.c_str(), S_IRWXU | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (ret < 0) {
        std::cout << "Cannot create directory " << g_testRoot << " err = " << errno << std::endl;
        ASSERT_TRUE(0);
    }
}

static void TearDownTestCase(void)
{
    if (RemoveDir(g_testRoot) < 0) {
        std::cout << "Failed to remove " << g_testRoot << ", err = " << errno << std::endl;
    }
}

static int RemoveDir(const std::string &path)
{
    if (path.empty()) {
        return 0;
    }
    auto dir = std::unique_ptr<DIR, decltype(&closedir)>(opendir(path.c_str()), closedir);
    if (dir == nullptr) {
        return -1;
    }

    struct dirent *dp = nullptr;
    while ((dp = readdir(dir.get())) != nullptr) {
        if (dp->d_name[0] == '.') {
            continue;
        }
        bool endsWithSlash = (path.find_last_of("/") == path.size() - 1);
        std::string fullPath {};
        if (endsWithSlash) {
            fullPath = path + dp->d_name;
        } else {
            fullPath = path + "/" + dp->d_name;
        }
        struct stat st {};
        if (stat(fullPath.c_str(), &st) < 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            if (RemoveDir(fullPath) < 0) {
                return -1;
            }
        } else {
            if (unlink(fullPath.c_str()) < 0) {
                return -1;
            }
        }
    }
    return rmdir(path.c_str());
}

void SetUp() {};
void TearDown() {};
};

HWTEST_F(UeventdFirmwareHandlerUnitTest, Init_UeventdFirmwareHandlerTest_HandleFirmware001, TestSize.Level1)
{
    mkdir("/data/ueventd_firmware/firmware_test", 0755);
    mkdir("/data/ueventd_firmware/firmware_test/devices", 0755);
    
    char loadingPath[256] = {0};
    int ret = snprintf_s(loadingPath, sizeof(loadingPath),
        "/data/ueventd_firmware/firmware_test/devices/loading");
    EXPECT_EQ(ret, EOK);
    
    int fd = open(loadingPath, O_RDWR | O_CREAT, 0644);
    if (fd >= 0) {
        close(fd);
    }
    
    struct Uevent uevent = {
        .subsystem = "firmware",
        .syspath = "/data/ueventd_firmware/firmware_test/devices",
        .deviceName = "test_firmware",
        .partitionName = "",
        .firmware = "test.bin",
        .action = ACTION_ADD,
        .partitionNum = 0,
        .major = 0,
        .minor = 0,
        .ug = {
            .uid = 0,
            .gid = 0,
        },
        .busNum = 0,
        .devNum = 0,
    };
    
    HandleFimwareDeviceEvent(&uevent);
}

} // namespace UeventdUt