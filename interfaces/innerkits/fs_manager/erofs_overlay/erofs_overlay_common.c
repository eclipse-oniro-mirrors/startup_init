/*
 * Copyright (c) 2024-2024 Huawei Device Co., Ltd.
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

#include <sys/stat.h>
#include <sys/wait.h>
#include "securec.h"
#include "init_utils.h"

#include "erofs_overlay_common.h"

bool IsOverlayEnable(void)
{
    char oemMode[MAX_BUFFER_LEN] = {0};
    int ret = GetParameterFromCmdLine("oemMode", oemMode, MAX_BUFFER_LEN);
    if (ret) {
        BEGET_LOGE("Failed get oenmode from cmdline.");
        return false;
    }

    char buildvariant[MAX_BUFFER_LEN] = {0};
    ret = GetParameterFromCmdLine("buildvariant", buildvariant, MAX_BUFFER_LEN);
    if (ret) {
        BEGET_LOGE("Failed get buildvariant from cmdline.");
        return false;
    }

    if (strcmp(oemMode, "user") == 0 || strcmp(buildvariant, "eng") != 0) {
        BEGET_LOGI("not support overlay, oemMode:[%s] buildvariant:[%s]", oemMode, buildvariant);
        return false;
    }
    BEGET_LOGI("overlay enable.");
    return true;
}

bool CheckIsExt4(const char *dev, uint64_t offset)
{
    int fd = open(dev, O_RDONLY | O_LARGEFILE);
    if (fd < 0) {
        BEGET_LOGE("cannot open [dev]:%s", dev);
        return false;
    }

    if (lseek(fd, offset + EXT4_SUPER_BLOCK_START_POSITION, SEEK_SET) < 0) {
        BEGET_LOGE("cannot seek [dev]:%s", dev);
        close(fd);
        return false;
    }
    ext4_super_block superBlock;
    ssize_t nbytes = read(fd, &superBlock, sizeof(superBlock));
    if (nbytes != sizeof(superBlock)) {
        BEGET_LOGE("read ext4 super block fail");
        close(fd);
        return false;
    }

    if (superBlock.s_magic == EXT4_SUPER_MAGIC) {
        BEGET_LOGE("this [dev] %s  is ext4:[block cout]: %d, [size]: %d", dev,
            superBlock.s_blocks_count_lo, (superBlock.s_blocks_count_lo * BLOCK_SIZE_UNIT));
        close(fd);
        return true;
    }
    close(fd);
    return false;
}

bool CheckIsErofs(const char *dev)
{
    int fd = open(dev, O_RDONLY | O_LARGEFILE);
    if (fd < 0) {
        BEGET_LOGE("cannot open [dev]:%s", dev);
        return false;
    }

    if (lseek(fd, EROFS_SUPER_BLOCK_START_POSITION, SEEK_SET) < 0) {
        BEGET_LOGE("cannot seek [dev]:%s", dev);
        close(fd);
        return false;
    }
    struct erofs_super_block superBlock;
    ssize_t nbytes = read(fd, &superBlock, sizeof(superBlock));
    if (nbytes != sizeof(superBlock)) {
        BEGET_LOGE("read erofs super block fail");
        close(fd);
        return false;
    }

    BEGET_LOGI("the [dev] %s magic [%u]", dev, superBlock.magic);
    if (superBlock.magic == EROFS_SUPER_MAGIC) {
        BEGET_LOGI("this [dev] %s is erofs", dev);
        close(fd);
        return true;
    }
    close(fd);
    return false;
}