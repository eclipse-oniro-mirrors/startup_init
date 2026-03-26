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
#include <libgen.h>

#include "param_stub.h"
#include "ueventd.h"
#include "ueventd_device_handler.h"
#include "ueventd_socket.h"

using namespace testing::ext;

namespace UeventdUt {
namespace {
    std::string g_testRoot{"/data/ueventd_handler"};
}

class UeventdDeviceHandlerUnitTest : public testing::Test {
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

bool IsFileExist(const std::string &file)
{
    struct stat st{};
    if (file.empty()) {
        return false;
    }

    if (stat(file.c_str(), &st) < 0) {
        return false;
    }
    return true;
}

HWTEST_F(UeventdDeviceHandlerUnitTest, Init_UeventdDeviceHandlerTest_HandleBlockDevice001, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "block",
        .syspath = "/devices/platform/test/block",
        .deviceName = "block_test",
        .partitionName = "test",
        .firmware = "",
        .action = ACTION_ADD,
        .partitionNum = 1,
        .major = 7,
        .minor = 0,
        .ug = {
            .uid = 0,
            .gid = 0,
        },
        .busNum = 0,
        .devNum = 0,
    };

    HandleBlockDeviceEvent(&uevent);
    auto exist = IsFileExist("/dev/block/block_test");
    EXPECT_TRUE(exist);
}

HWTEST_F(UeventdDeviceHandlerUnitTest, Init_UeventdDeviceHandlerTest_HandleBlockDevice002, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "block",
        .syspath = "/devices/platform/test/block",
        .deviceName = "block_test2",
        .partitionName = "test",
        .firmware = "",
        .action = ACTION_REMOVE,
        .partitionNum = 1,
        .major = 7,
        .minor = 1,
        .ug = {
            .uid = 0,
            .gid = 0,
        },
        .busNum = 0,
        .devNum = 0,
    };

    HandleBlockDeviceEvent(&uevent);
}

HWTEST_F(UeventdDeviceHandlerUnitTest, Init_UeventdDeviceHandlerTest_HandleOtherDevice001, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "input",
        .syspath = "/devices/platform/test/input",
        .deviceName = "event0",
        .major = 13,
        .minor = 64,
    };

    HandleOtherDeviceEvent(&uevent);
    auto exist = IsFileExist("/dev/input/event0");
    EXPECT_TRUE(exist);
}

HWTEST_F(UeventdDeviceHandlerUnitTest, Init_UeventdDeviceHandlerTest_HandleOtherDevice002, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "drm",
        .syspath = "/devices/platform/test/drm",
        .deviceName = "card0",
        .major = 226,
        .minor = 0,
    };

    HandleOtherDeviceEvent(&uevent);
    auto exist = IsFileExist("/dev/dri/card0");
    EXPECT_TRUE(exist);
}

HWTEST_F(UeventdDeviceHandlerUnitTest, Init_UeventdDeviceHandlerTest_HandleOtherDevice003, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "sound",
        .syspath = "/devices/platform/test/sound",
        .deviceName = "pcmC0D0c",
        .major = 116,
        .minor = 0,
    };

    HandleOtherDeviceEvent(&uevent);
    auto exist = IsFileExist("/dev/snd/pcmC0D0c");
    EXPECT_TRUE(exist);
}

HWTEST_F(UeventdDeviceHandlerUnitTest, Init_UeventdDeviceHandlerTest_HandleOtherDevice004, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "graphics",
        .syspath = "/devices/platform/test/graphics",
        .deviceName = "fb0",
        .major = 29,
        .minor = 0,
    };

    HandleOtherDeviceEvent(&uevent);
    auto exist = IsFileExist("/dev/graphics/fb0");
    EXPECT_TRUE(exist);
}

HWTEST_F(UeventdDeviceHandlerUnitTest, Init_UeventdDeviceHandlerTest_HandleOtherDevice005, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "functionfs",
        .syspath = "/devices/platform/test/functionfs",
        .deviceName = "ep1",
        .major = 10,
        .minor = 200,
    };

    HandleOtherDeviceEvent(&uevent);
    auto exist = IsFileExist("/dev/functionfs/ep1");
    EXPECT_TRUE(exist);
}

HWTEST_F(UeventdDeviceHandlerUnitTest, Init_UeventdDeviceHandlerTest_HandleOtherDevice006, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "dma_heap",
        .syspath = "/devices/platform/test/dma_heap",
        .deviceName = "system",
        .major = 10,
        .minor = 201,
    };

    HandleOtherDeviceEvent(&uevent);
    auto exist = IsFileExist("/dev/dma_heap/system");
    EXPECT_TRUE(exist);
}

HWTEST_F(UeventdDeviceHandlerUnitTest, Init_UeventdDeviceHandlerTest_HandleOtherDevice007, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "usb",
        .syspath = "/devices/usb1/1-1",
        .deviceName = "usb-device",
        .major = 189,
        .minor = 0,
    };

    HandleOtherDeviceEvent(&uevent);
    auto exist = IsFileExist("/dev/usb-device");
    EXPECT_TRUE(exist);
}

HWTEST_F(UeventdDeviceHandlerUnitTest, Init_UeventdDeviceHandlerTest_HandleOtherDevice008, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "usb",
        .syspath = "/devices/usb1/1-1",
        .major = 189,
        .minor = 1,
        .busNum = 1,
        .devNum = 2,
    };

    HandleOtherDeviceEvent(&uevent);
    auto exist = IsFileExist("/dev/bus/usb/001/002");
    EXPECT_TRUE(exist);
}

} // namespace UeventdUt