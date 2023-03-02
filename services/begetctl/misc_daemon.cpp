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
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "begetctl.h"
#include "fs_manager/fs_manager.h"
#include "shell.h"
#include "shell_utils.h"
#include "init_param.h"

constexpr int MAX_LOGO_SIZE = 1024 * 2038;
constexpr int PARTITION_INFO_POS = 1144;
constexpr int PARTITION_INFO_MAX_LENGTH = 256;
constexpr int BLOCK_SZIE_1 = 512;
constexpr uint64_t LOGO_MAGIC = 0XABCABCAB;

#define MISC_DEVICE_NODE "/dev/block/by-name/misc"

static void ClearLogo(int fd)
{
    if (fd < 0) {
        return;
    }
    char buffer[8] = {0}; // logo magic number + logo size is 8
    int addrOffset = (PARTITION_INFO_POS + PARTITION_INFO_MAX_LENGTH + BLOCK_SZIE_1 - 1) / BLOCK_SZIE_1;
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
    if (size == 0 || size > MAX_LOGO_SIZE) {
        BSH_LOGE("logo size is invalid!");
        return;
    }
    FILE *rgbFile = fopen(logoPath.c_str(), "rb");
    if (rgbFile == nullptr) {
        std::cout << "cannot find pic file\n";
        return;
    }
    char *buffer = reinterpret_cast<char *>(malloc(size));
    if (buffer == nullptr) {
        (void)fclose(rgbFile);
        return;
    }
    (void)fread(buffer, 1, size, rgbFile);
    if (ferror(rgbFile)) {
        (void)fclose(rgbFile);
        free(buffer);
        return;
    }
    ssize_t ret = write(fd, buffer, size);
    if (ret == -1 || ret != size) {
        (void)fclose(rgbFile);
        free(buffer);
        return;
    }
    free(buffer);
    (void)fclose(rgbFile);
}

static int WriteLogo(int fd, const std::string &logoPath)
{
    int addrOffset = (PARTITION_INFO_POS + PARTITION_INFO_MAX_LENGTH + BLOCK_SZIE_1 - 1) / BLOCK_SZIE_1;
    if (lseek(fd, addrOffset * BLOCK_SZIE_1, SEEK_SET) < 0) {
        BSH_LOGI("Failed lseek logoPath %s errno %d ", logoPath.c_str(), errno);
        return -1;
    }

    uint32_t magic = 0;
    if (read(fd, &magic, sizeof(uint32_t)) != sizeof(uint32_t)) {
        BSH_LOGI("Failed magic logoPath %s errno %d ", logoPath.c_str(), errno);
        return -1;
    }
#ifndef STARTUP_INIT_TEST
    if (magic == LOGO_MAGIC) {
        BSH_LOGI("Get matched magic number, logo already written\n");
        return 0;
    }
#endif
    struct stat st {};
    magic = LOGO_MAGIC;
    lseek(fd, addrOffset * BLOCK_SZIE_1, SEEK_SET);
    if (write(fd, &magic, sizeof(magic)) != sizeof(magic)) {
        BSH_LOGI("Write magic number failed %d", errno);
        return -1;
    }

    if (stat(logoPath.c_str(), &st) < 0) {
        if (errno == ENOENT) {
            BSH_LOGI("%s is not exist", logoPath.c_str());
        } else {
            BSH_LOGI("Failed to get %s stat", logoPath.c_str());
        }
        ClearLogo(fd);
        return -1;
    }

    if (st.st_size <= 0 || st.st_size > MAX_LOGO_SIZE) {
        BSH_LOGE("Invalid logo file with size ");
        ClearLogo(fd);
        return -1;
    }

    uint32_t logoSize =  static_cast<uint32_t>(st.st_size);
    if (write(fd, &logoSize, sizeof(logoSize)) != sizeof(logoSize)) {
        BSH_LOGE("Write logo size failed ");
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
    int fd = open(MISC_DEVICE_NODE, O_RDWR | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        BSH_LOGI("Failed to writeLogoToMisc errno %d ", errno);
        return;
    }

    if (WriteLogo(fd, logoPath) < 0) {
        BSH_LOGI("Failed WriteLogo errno %d ", errno);
    }
    close(fd);
    int addrOffset = (PARTITION_INFO_POS + PARTITION_INFO_MAX_LENGTH + BLOCK_SZIE_1 - 1) / BLOCK_SZIE_1;
    int fd1 = open(MISC_DEVICE_NODE, O_RDWR | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd1 < 0) {
        return;
    }
    if (lseek(fd1, addrOffset * BLOCK_SZIE_1, SEEK_SET) < 0) {
        BSH_LOGI("Failed lseek errno %d ", errno);
        close(fd1);
        return;
    }

    uint32_t magic = 0;
    uint32_t size = 0;
    if (read(fd1, &magic, sizeof(uint32_t)) != sizeof(uint32_t)) {
        BSH_LOGI("Failed read errno %d ", errno);
        close(fd1);
        return;
    }
    if (read(fd1, &size, sizeof(uint32_t)) != sizeof(uint32_t)) {
        BSH_LOGI("Failed read migic errno %d ", errno);
        close(fd1);
        return;
    }

    close(fd1);
}

static int main_cmd(BShellHandle shell, int argc, char **argv)
{
    if (argc >= 2 && strcmp(const_cast<char *>("--write_logo"), argv[0]) == 0) { // 2 min arg
        WriteLogoToMisc(argv[1]);
    } else {
        char *helpArgs[] = {const_cast<char *>("misc_daemon"), nullptr};
        BShellCmdHelp(shell, 1, helpArgs);
    }
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    const CmdInfo infos[] = {
        {
            const_cast<char *>("misc_daemon"), main_cmd, const_cast<char *>("write start logo"),
            const_cast<char *>("misc_daemon --write_logo xxx.rgb"), const_cast<char *>("misc_daemon --write_logo")
        }
    };
    for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
        BShellEnvRegisterCmd(GetShellHandle(), &infos[i]);
    }
}
