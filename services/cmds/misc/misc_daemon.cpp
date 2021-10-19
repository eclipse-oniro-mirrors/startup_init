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
#include <cstdint>
#include <fcntl.h>
#include <getopt.h>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include "fs_manager/fs_manager.h"
#include "misc_info.h"
#include "param_wrapper.h"

constexpr int PARTITION_INFO_POS = 1144;
constexpr int PARTITION_INFO_MAX_LENGTH = 256;
constexpr int BLOCK_SZIE_1 = 512;
constexpr uint64_t LOGO_MAGIC = 0XABCABCAB;

struct option g_options[] = {
    { "write_logo", required_argument, nullptr, 0 },
    { nullptr, 0, nullptr, 0 },
};

static std::string GetMiscDevicePath()
{
    std::string miscDev {};
    // Get misc device path from fstab
    std::string hardwareVal {};
    int ret = OHOS::system::GetStringParameter("ohos.boot.hardware", hardwareVal, "");
    if (ret != 0) {
        std::cout << "get ohos.boot.hardware failed\n";
        return "";
    }
    std::string fstabFileName = std::string("fstab.") + hardwareVal;
    std::string fstabFile = std::string("/vendor/etc/") + fstabFileName;
    Fstab *fstab = ReadFstabFromFile(fstabFile.c_str(), false);
    if (fstab == nullptr) {
        std::cout << "Failed to read fstab\n";
        return miscDev;
    }

    FstabItem *misc = FindFstabItemForMountPoint(*fstab, "/misc");
    if (misc == nullptr) {
        std::cout << "Cannot find misc partition from fstab\n";
        return miscDev;
    }
    miscDev = misc->deviceName;
    ReleaseFstab(fstab);
    return miscDev;
}


static void ClearLogo(int fd)
{
    if (fd < 0) {
        return;
    }
    char buffer[8] = {0}; // logo magic number + logo size is 8
    int addrOffset = (PARTITION_INFO_POS + PARTITION_INFO_MAX_LENGTH + BLOCK_SZIE_1 -1) / BLOCK_SZIE_1;
    if (lseek(fd, addrOffset * BLOCK_SZIE_1, SEEK_SET) < 0) {
        std::cout << "Failed to clean file\n";
        return;
    }
    if (write(fd, &buffer, sizeof(buffer)) != sizeof(buffer)) {
        std::cout << "clean misc failed\n";
        return;
    }
}

static void WriteLogoContent(int fd, const std::string &logoPath, uint32_t size)
{
    if (fd < 0 || logoPath.empty() || size <= 0) {
        std::cout << "path is null or size illegal\n";
        return;
    }
    FILE* rgbFile = fopen(logoPath.c_str(), "rb");
    if (rgbFile == nullptr) {
        std::cout << "cannot find pic file\n";
        return;
    }

    char* buffer = (char*)malloc(size);
    if (buffer == nullptr) {
        return;
    }
    uint32_t ret = fread(buffer, 1, size, rgbFile);
    if (ret < 0) {
        return;
    }
    ret = write(fd, buffer, size);
    if (ret != size) {
        return;
    }

    if (buffer != nullptr) {
        free(buffer);
        buffer = nullptr;
    }
    (void)fclose(rgbFile);
}

static int WriteLogo(int fd, const std::string &logoPath)
{
    if (fd < 0 || logoPath.empty()) {
        std::cout << "Invalid arguments\n";
        return -1;
    }
    int addrOffset = (PARTITION_INFO_POS + PARTITION_INFO_MAX_LENGTH + BLOCK_SZIE_1 - 1) / BLOCK_SZIE_1;
    if (lseek(fd, addrOffset * BLOCK_SZIE_1, SEEK_SET) < 0) {
        std::cout << "Failed to seek file\n";
        return -1;
    }

    uint32_t magic = 0;
    if (read(fd, &magic, sizeof(uint32_t)) != sizeof(uint32_t)) {
        std::cout << "Failed to read magic number\n";
        return -1;
    }

    if (magic == LOGO_MAGIC) {
        std::cout << "Get matched magic number, logo already written\n";
        return 0;
    }
    struct stat st {};
    magic = LOGO_MAGIC;
    lseek(fd, addrOffset * BLOCK_SZIE_1, SEEK_SET);
    if (write(fd, &magic, sizeof(magic)) != sizeof(magic)) {
        std::cout << "Write magic number failed\n";
        return -1;
    }

    if (stat(logoPath.c_str(), &st) < 0) {
        if (errno == ENOENT) {
            std::cout << logoPath << " is not exist\n";
        } else {
            std::cout << "Failed to get " << logoPath << " stat\n";
        }
        ClearLogo(fd);
        return -1;
    }

    if (st.st_size < 0 || st.st_size > updater::MAX_LOGO_SIZE) {
        std::cout << "Invalid logo file with size \n";
        ClearLogo(fd);
        return -1;
    }

    uint32_t logoSize =  static_cast<uint32_t>(st.st_size);
    if (write(fd, &logoSize, sizeof(logoSize)) != sizeof(logoSize)) {
        std::cout << "Write logo size failed\n";
        ClearLogo(fd);
        return -1;
    }

    WriteLogoContent(fd, logoPath, logoSize);
    return 0;
}

static void WriteLogoToMisc(const std::string &logoPath)
{
    if (logoPath.empty()) {
        std::cout << "logo path is empty\n";
        return;
    }
    std::string miscDev = GetMiscDevicePath();
    if (miscDev.empty()) {
        return;
    }

    int fd = open(miscDev.c_str(), O_RDWR | O_CLOEXEC, 0644);
    if (fd < 0) {
        std::cout << "Failed to open " << miscDev << ", err = " << errno << std::endl;
        return;
    }

    if (WriteLogo(fd, logoPath) < 0) {
        std::cout << "Write logo to " << miscDev << " failed" << std::endl;
    }
    close(fd);
    int addrOffset = (PARTITION_INFO_POS + PARTITION_INFO_MAX_LENGTH + BLOCK_SZIE_1 - 1) / BLOCK_SZIE_1;
    int fd1 = open(miscDev.c_str(), O_RDWR | O_CLOEXEC, 0644);
    if (lseek(fd1, addrOffset * BLOCK_SZIE_1, SEEK_SET) < 0) {
        std::cout << "Failed to seek file\n";
        return;
    }

    uint32_t magic = 0;
    uint32_t size = 0;
    if (read(fd1, &magic, sizeof(uint32_t)) != sizeof(uint32_t)) {
        std::cout << "Failed to read magic number\n";
        return;
    }
    if (read(fd1, &size, sizeof(uint32_t)) != sizeof(uint32_t)) {
        std::cout << "Failed to read magic number\n";
        return;
    }

    close(fd1);
}

int main(int argc, char **argv)
{
    int rc = -1;
    int optIndex = -1;
    while ((rc = getopt_long(argc, argv, "",  g_options, &optIndex)) != -1) {
        switch (rc) {
            case 0: {
                std::string optionName = g_options[optIndex].name;
                if (optionName == "write_logo") {
                    std::string logoPath = optarg;
                    WriteLogoToMisc(logoPath);
                }
                break;
            }
            case '?':
                std::cout << "Invalid arugment\n";
                break;
            default:
                std::cout << "Invalid arugment\n";
                break;
        }
    }
}
