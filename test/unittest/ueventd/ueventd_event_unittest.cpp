/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include "ueventd.h"
#include "ueventd_device_handler.h"
#include "ueventd_socket.h"

using namespace testing::ext;
namespace UeventdUt {
namespace {
    std::string g_testRoot{"/data/ueventd"};
    int g_oldRootFd = -1;
}

class UeventdEventUnitTest : public testing::Test {
public:
static void SetUpTestCase(void)
{
    struct stat st{};
    bool isExist = true;

    if (stat(g_testRoot.c_str(), &st) < 0) {
        if (errno != ENOENT) {
            std::cout << "Cannot get stat of " << g_testRoot << std::endl;
            // If we cannot get root for ueventd tests
            // There is no reason to continue.
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
    if (SwitchRoot() < 0) {
        // If we cannot do this, there is not reason to continue
        ASSERT_TRUE(0);
    }
}

static void TearDownTestCase(void)
{
    // Switch back to old root
    if (g_oldRootFd < 0) {
        std::cout << "Old root directory is not valid\n";
        return;
    }

    if (fchdir(g_oldRootFd) < 0) {
        std::cout << "Failed to change working directory to '/', err = " << errno << std::endl;
    }

    if (chroot(".") < 0) {
        std::cout << "Failed to change root directory to '/', err = " << errno << std::endl;
    }
    close(g_oldRootFd);
    g_oldRootFd = -1;
    std::cout << "Change root back done\n";
    // Remove test data
    if (RemoveDir(g_testRoot) < 0) {
        std::cout << "Failed to remove " << g_testRoot << ", err = " << errno << std::endl;
    }
}

static int RemoveDir(const std::string &path)
{
    if (path.empty()) {
        // Treat it as OK
        return 0;
    }
    auto dir = std::unique_ptr<DIR, decltype(&closedir)>(opendir(path.c_str()), closedir);
    if (dir == nullptr) {
        std::cout << "Cannot open dir " << path << ", err = " << errno << std::endl;
        return -1;
    }

    struct dirent *dp = nullptr;
    while ((dp = readdir(dir.get())) != nullptr) {
        // Skip hidden files
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
            std::cout << "Failed to get stat of " << fullPath << std::endl;
            continue; // Should we continue?
        }
        if (S_ISDIR(st.st_mode)) {
            if (RemoveDir(fullPath) < 0) {
                std::cout << "Failed to remove directory " << fullPath << std::endl;
                return -1;
            }
        } else {
            if (unlink(fullPath.c_str()) < 0) {
                std::cout << "Failed to unlink file " << fullPath << std::endl;
                return -1;
            }
        }
    }
    return rmdir(path.c_str());
}

static int SwitchRoot()
{
    if (g_oldRootFd >= 0) {
        close(g_oldRootFd);
        g_oldRootFd = -1;
    }
    // Save old root
    DIR *dir = opendir("/");
    if (dir == nullptr) {
        std::cout << "Failed to open root directory\n";
        return -1;
    }
    g_oldRootFd = dirfd(dir);
    if (g_oldRootFd < 0) {
        std::cout << "Failed to pen root directory, err = " << errno << std::endl;
        return -1;
    }

    // Changing working directory to "/data/ueventd"
    if (chdir(g_testRoot.c_str()) < 0) {
        std::cout << "Failed to change working directory to " << g_testRoot << ", err = " << errno << std::endl;
        close(g_oldRootFd);
        g_oldRootFd = -1;
    }

    if (chroot(g_testRoot.c_str()) < 0) {
        std::cout << "Failed to change root directory to " << g_testRoot << ", err = " << errno << std::endl;
        close(g_oldRootFd);
        g_oldRootFd = -1;
    }
    std::cout << "Change root to " << g_testRoot << " done\n";
    return 0;
}

void SetUp() {};
void TearDown() {};
};

// Generate uevent buffer from struct uevent.
// extra data used to break uevent buffer to check
// if ueventd will handle this situation correctly
std::string GenerateUeventBuffer(struct Uevent &uevent, std::vector<std::string> &extraData)
{
    std::string ueventdBuffer{};
    if (uevent.syspath != nullptr) {
        ueventdBuffer.append(std::string("DEVPATH=") + uevent.syspath + '\000');
    }
    if (uevent.subsystem != nullptr) {
        ueventdBuffer.append(std::string("SUBSYSTEM=") + uevent.subsystem + '\000');
    }
    ueventdBuffer.append(std::string("ACTION=") + ActionString(uevent.action) + '\000');
    if (uevent.deviceName != nullptr) {
        ueventdBuffer.append(std::string("DEVNAME=") + uevent.deviceName + '\000');
    }
    if (uevent.partitionName != nullptr) {
        ueventdBuffer.append(std::string("PARTNAME=") + uevent.partitionName + '\000');
    }
    ueventdBuffer.append(std::string("PARTN=") + std::to_string(uevent.partitionNum) + '\000');
    ueventdBuffer.append(std::string("MAJOR=") + std::to_string(uevent.major) + '\000');
    ueventdBuffer.append(std::string("MINOR=") + std::to_string(uevent.minor) + '\000');
    ueventdBuffer.append(std::string("DEVUID=") + std::to_string(uevent.ug.uid) + '\000');
    ueventdBuffer.append(std::string("DEVGID=") + std::to_string(uevent.ug.gid) + '\000');
    if (uevent.firmware != nullptr) {
        ueventdBuffer.append(std::string("FIRMWARE=") + uevent.firmware + '\000');
    }
    ueventdBuffer.append(std::string("BUSNUM=") + std::to_string(uevent.busNum) + '\000');
    ueventdBuffer.append(std::string("DEVNUM=") + std::to_string(uevent.devNum) + '\000');

    if (!extraData.empty()) {
        for (const auto &data : extraData) {
            ueventdBuffer.append(data);
        }
    }
    return ueventdBuffer;
}

bool IsFileExist(const std::string &file)
{
    struct stat st{};
    if (file.empty()) {
        return false;
    }

    if (stat(file.c_str(), &st) < 0) {
        if (errno == ENOENT) {
            std::cout << "File " << file << " is not exist\n";
        }
        return false;
    }
    return true;
}

HWTEST_F(UeventdEventUnitTest, TestParseUeventdEvent, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "block",
        .syspath = "/block/mmc/test",
        .deviceName = "test",
        .partitionName = "userdata",
        .firmware = "",
        .action = ACTION_ADD,
        .partitionNum = 3,
        .major = 1,
        .minor = 2,
        .ug = {
            .uid = 0,
            .gid = 0,
        },
        .busNum = 1,
        .devNum = 2,
    };

    std::vector<std::string> extraData{};
    auto ueventBuffer = GenerateUeventBuffer(uevent, extraData);
    std::cout << "ueventBuffer = [" << ueventBuffer << "]. size = " << ueventBuffer.length() << std::endl;
    struct Uevent outEvent;
    ParseUeventMessage(ueventBuffer.data(), ueventBuffer.length(), &outEvent);
    EXPECT_EQ(outEvent.action, ACTION_ADD);
    EXPECT_EQ(outEvent.busNum, 1);
    EXPECT_STREQ(outEvent.subsystem, "block");
    EXPECT_STREQ(outEvent.deviceName, "test");
    EXPECT_EQ(outEvent.devNum, 2);
    EXPECT_EQ(outEvent.major, 1);
    EXPECT_EQ(outEvent.minor, 2);
    EXPECT_EQ(outEvent.partitionNum, 3);
    EXPECT_STREQ(outEvent.partitionName, "userdata");
    EXPECT_STREQ(outEvent.syspath, "/block/mmc/test");
    EXPECT_EQ(outEvent.ug.gid, 0);
    EXPECT_EQ(outEvent.ug.uid, 0);
}

HWTEST_F(UeventdEventUnitTest, TestUeventActions, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "block",
        .syspath = "/block/mmc/test",
        .deviceName = "test",
        .partitionName = "userdata",
        .action = ACTION_UNKNOWN,
    };
    std::vector<std::string> extraData {};
    auto ueventBuffer = GenerateUeventBuffer(uevent, extraData);
    std::cout << "ueventBuffer = [" << ueventBuffer << "]. size = " << ueventBuffer.length() << std::endl;
    struct Uevent outEvent;
    ParseUeventMessage(ueventBuffer.data(), ueventBuffer.length(), &outEvent);
    EXPECT_EQ(outEvent.action, ACTION_UNKNOWN);
}

HWTEST_F(UeventdEventUnitTest, TestUeventHandleBlockDevicesInvalidParameters, TestSize.Level1)
{
    HandleBlockDeviceEvent(nullptr);
    // Not block device
    struct Uevent noBlockUevent = {
        .subsystem = "char",
    };
    HandleBlockDeviceEvent(&noBlockUevent);

    struct Uevent invalidDevNoUevent = {
        .subsystem = "block",
        .major = -1,
        .minor = -1,
    };
    HandleBlockDeviceEvent(&invalidDevNoUevent);

    struct Uevent invalidSysPathUevent = {
        .subsystem = "block",
        .syspath = nullptr,
        .major = 1,
        .minor = 1,
    };
    HandleBlockDeviceEvent(&invalidSysPathUevent);
}

HWTEST_F(UeventdEventUnitTest, TestUeventHandleBlockDevicesValidParameters, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "block",
        .syspath = "/block/mmc/block_device_test",
        .deviceName = "block_device_test",
        .partitionName = "block_device_test",
        .firmware = "",
        .action = ACTION_ADD,
        .partitionNum = 3,
        .major = 5,
        .minor = 15,
        .ug = {
            .uid = 0,
            .gid = 0,
        },
        .busNum = 1,
        .devNum = 2,
    };

    HandleBlockDeviceEvent(&uevent);
    // Check results
    std::string blockDevice = "/dev/block/block_device_test";
    struct stat st{};
    int ret = stat(blockDevice.c_str(), &st);
    EXPECT_EQ(ret, 0);
    bool isBlock = S_ISBLK(st.st_mode);
    EXPECT_TRUE(isBlock);
}

HWTEST_F(UeventdEventUnitTest, TestUeventHandleBlockDevicesRemoved, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "block",
        .syspath = "/block/mmc/block_device_test",
        .deviceName = "block_device_test",
        .partitionName = "block_device_test",
        .firmware = "",
        .action = ACTION_REMOVE,
        .partitionNum = 3,
        .major = 5,
        .minor = 15,
        .ug = {
            .uid = 0,
            .gid = 0,
        },
        .busNum = 1,
        .devNum = 2,
    };
    std::string blockDevice = "/dev/block/block_device_test";
    struct stat st{};
    int ret = stat(blockDevice.c_str(), &st);
    if (ret < 0) {
        // This should not happen actually, because we've created the device node before.
        std::cout << "Warning. Block device " << blockDevice << " is not exist.\n";
    }
    HandleBlockDeviceEvent(&uevent);
    ret = stat(blockDevice.c_str(), &st);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(errno, ENOENT);
}

HWTEST_F(UeventdEventUnitTest, TestUeventHandleBlockDevicesChanged, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "block",
        .syspath = "/block/mmc/block_device_test",
        .deviceName = "block_device_test",
        .partitionName = "block_device_test",
        .firmware = "",
        .action = ACTION_REMOVE,
        .partitionNum = 3,
        .major = 5,
        .minor = 15,
        .ug = {
            .uid = 0,
            .gid = 0,
        },
        .busNum = 1,
        .devNum = 2,
    };
    HandleBlockDeviceEvent(&uevent);
}

HWTEST_F(UeventdEventUnitTest, TestUeventHandleOtherDevicesInvalidParameters, TestSize.Level1)
{
    HandleOtherDeviceEvent(nullptr);
    // Not Character device
    struct Uevent invalidDevNoUevent = {
        .subsystem = "test",
        .major = -1,
        .minor = -1,
    };
    HandleOtherDeviceEvent(&invalidDevNoUevent);

    struct Uevent invalidSysPathUevent = {
        .subsystem = "test",
        .syspath = nullptr,
        .major = 5,
        .minor = 9,
    };
    HandleOtherDeviceEvent(&invalidSysPathUevent);

    struct Uevent invalidSubsystemUevent = {
        .subsystem = "",
        .syspath = "/devices/test/char",
        .major = 5,
        .minor = 9,
    };
    HandleOtherDeviceEvent(&invalidSubsystemUevent);
}

HWTEST_F(UeventdEventUnitTest, TestUeventHandleOtherDevicesValidParameters, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "extcon3",
        .syspath = "/devices/platform/headset/extcon/extcon3",
        .deviceName = "extcon3-1",
        .major = 5,
        .minor = 9,
    };
    HandleOtherDeviceEvent(&uevent);
    auto exist = IsFileExist("/dev/extcon3-1");
    EXPECT_TRUE(exist);
    exist = IsFileExist("/dev/extcon3");
    EXPECT_FALSE(exist);
}

HWTEST_F(UeventdEventUnitTest, TestUeventHandleUsbDevicesWithDeviceName, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "usb",
        .syspath = "/devices/platform/headset/extcon/usb-dev",
        .deviceName = "usb-dev",
        .major = 8,
        .minor = 9,
    };
    HandleOtherDeviceEvent(&uevent);
    auto exist = IsFileExist("/dev/usb-dev");
    EXPECT_TRUE(exist);
}

HWTEST_F(UeventdEventUnitTest, TestUeventHandleInvalidUsbDevices, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "usb",
        .syspath = "/devices/platform/headset/extcon/usb-dev-1",
        .major = 8,
        .minor = 10,
        .busNum = -1,
        .devNum = -1,
    };
    HandleOtherDeviceEvent(&uevent);
    auto exist = IsFileExist("/dev/usb-dev-1");
    EXPECT_FALSE(exist);
}

HWTEST_F(UeventdEventUnitTest, TestUeventHandleUsbDevicesWithBusNo, TestSize.Level1)
{
    struct Uevent uevent = {
        .subsystem = "usb",
        .syspath = "/devices/platform/headset/extcon/usb-dev",
        .major = 8,
        .minor = 9,
        .busNum = 3,
        .devNum = 4,
    };
    HandleOtherDeviceEvent(&uevent);
    auto exist = IsFileExist("/dev/bus/usb/003/004");
    EXPECT_TRUE(exist);
}
} // UeventdUt