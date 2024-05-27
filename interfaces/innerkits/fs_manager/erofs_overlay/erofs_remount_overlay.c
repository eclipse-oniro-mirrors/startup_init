/*
 * Copyright (c) 2024-2024 Huawei Device Co., Ltd.
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

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "securec.h"
#include "init_utils.h"
#include "fs_dm.h"
#include "fs_manager/fs_manager.h"
#include "erofs_remount_overlay.h"

#define MODEM_DRIVER_MNT_PATH "/vendor/modem/modem_driver"
#define MODEM_VENDOR_MNT_PATH "/vendor/modem/modem_vendor"
#define MODEM_FW_MNT_PATH "/vendor/modem/modem_fw"
#define MODEM_DRIVER_EXCHANGE_PATH "/mnt/driver_exchange"
#define MODEM_VENDOR_EXCHANGE_PATH "/mnt/vendor_exchange"
#define MODEM_FW_EXCHANGE_PATH "/mnt/fw_exchange"

static int Modem2Exchange(const char *modemPath, const char *exchangePath)
{
    int ret;
    ret = mkdir(exchangePath, MODE_MKDIR);
    if (ret) {
        BEGET_LOGE("mkdir %s failed.", exchangePath);
    } else {
        BEGET_LOGI("mkdir %s succeed.", exchangePath);
    }

    ret = mount(modemPath, exchangePath, NULL, MS_BIND | MS_RDONLY, NULL);
    if (ret) {
        BEGET_LOGE("bind %s to %s failed.", modemPath, exchangePath);
    } else {
        BEGET_LOGI("bind %s to %s succeed.", modemPath, exchangePath);
    }

    return ret;
}

static int Exchange2Modem(const char *modemPath, const char *exchangePath)
{
    int ret;
    struct stat statInfo;
    if (lstat(exchangePath, &statInfo)) {
        BEGET_LOGE("no  exchangePath %s.", exchangePath);
        return 0;
    }

    ret = mount(exchangePath, modemPath, NULL, MS_BIND | MS_RDONLY, NULL);
    if (ret) {
        BEGET_LOGE("bind %s to %s failed.", exchangePath, modemPath);
    } else {
        BEGET_LOGI("bind %s to %s succeed.", exchangePath, modemPath);
    }

    ret = umount(exchangePath);
    if (ret) {
        BEGET_LOGE("umount %s failed.", exchangePath);
    } else {
        BEGET_LOGI("umount %s succeed.", exchangePath);
    }

    ret = remove(exchangePath);
    if (ret) {
        BEGET_LOGE("remove %s failed.", exchangePath);
    } else {
        BEGET_LOGI("remove %s succeed.", exchangePath);
    }

    return ret;
}

void OverlayRemountVendorPre(void)
{
    struct stat statInfo;

    if (!stat(MODEM_DRIVER_MNT_PATH, &statInfo)) {
        Modem2Exchange(MODEM_DRIVER_MNT_PATH, MODEM_DRIVER_EXCHANGE_PATH);
    }

    if (!stat(MODEM_VENDOR_MNT_PATH, &statInfo)) {
        Modem2Exchange(MODEM_VENDOR_MNT_PATH, MODEM_VENDOR_EXCHANGE_PATH);
    }

    if (!stat(MODEM_FW_MNT_PATH, &statInfo)) {
        Modem2Exchange(MODEM_FW_MNT_PATH, MODEM_FW_EXCHANGE_PATH);
    }
}

void OverlayRemountVendorPost()
{
    Exchange2Modem(MODEM_DRIVER_MNT_PATH, MODEM_DRIVER_EXCHANGE_PATH);
    Exchange2Modem(MODEM_VENDOR_MNT_PATH, MODEM_VENDOR_EXCHANGE_PATH);
    Exchange2Modem(MODEM_FW_MNT_PATH, MODEM_FW_EXCHANGE_PATH);
}

int MountOverlayOne(const char *mnt)
{
    if ((strcmp(mnt, MODEM_DRIVER_MNT_PATH) == 0) || (strcmp(mnt, MODEM_VENDOR_MNT_PATH) == 0)) {
        return 0;
    }
    char dirLower[MAX_BUFFER_LEN] = {0};
    char dirUpper[MAX_BUFFER_LEN] = {0};
    char dirWork[MAX_BUFFER_LEN] = {0};
    char mntOpt[MAX_BUFFER_LEN] = {0};

    if (snprintf_s(dirLower, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, "%s%s", PREFIX_LOWER, mnt) < 0) {
        BEGET_LOGE("copy dirLower failed. errno %d", errno);
        return -1;
    }

    if (snprintf_s(dirUpper, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, "%s%s%s", PREFIX_OVERLAY, mnt, PREFIX_UPPER) < 0) {
        BEGET_LOGE("copy dirUpper failed. errno %d", errno);
        return -1;
    }

    if (snprintf_s(dirWork, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, "%s%s%s", PREFIX_OVERLAY, mnt, PREFIX_WORK) < 0) {
        BEGET_LOGE("copy dirWork failed. errno %d", errno);
        return -1;
    }

    if (snprintf_s(mntOpt, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1,
        "upperdir=%s,lowerdir=%s,workdir=%s,override_creds=off", dirUpper, dirLower, dirWork) < 0) {
        BEGET_LOGE("copy mntOpt failed. errno %d", errno);
        return -1;
    }

    if (strcmp(mnt, "/usr") == 0) {
        if (mount("overlay", "/system", "overlay", 0, mntOpt)) {
            BEGET_LOGE("mount system overlay failed. errno %d", errno);
            return -1;
        }
    } else {
        if (mount("overlay", mnt, "overlay", 0, mntOpt)) {
            BEGET_LOGE("mount overlay fs failed. errno %d", errno);
            return -1;
        }
    }
    BEGET_LOGE("mount overlay fs success on mnt:%s", mnt);
    return 0;
}

int RemountOverlay(void)
{
    char *remountPath[] = {"/usr", "/vendor", "/sys_prod", "/chip_prod", "/preload"};

    for (size_t i = 0; i < ARRAY_LENGTH(remountPath); i++) {
        struct stat statInfo;
        char dirMnt[MAX_BUFFER_LEN] = {0};
        if (snprintf_s(dirMnt, MAX_BUFFER_LEN, MAX_BUFFER_LEN - 1, "/mnt/overlay%s/upper", remountPath[i]) < 0) {
            BEGET_LOGE("copy dirMnt [%s] failed.", dirMnt);
            return -1;
        }

        if (lstat(dirMnt, &statInfo)) {
            BEGET_LOGE("dirMnt [%s] not exist.", dirMnt);
            continue;
        }

        if (strcmp(remountPath[i], "/vendor") == 0) {
            OverlayRemountVendorPre();
        }

        if (MountOverlayOne(remountPath[i])) {
            BEGET_LOGE("mount overlay failed on mnt [%s].", dirMnt);
            return -1;
        }

        if (strcmp(remountPath[i], "/vendor") == 0) {
            OverlayRemountVendorPost();
        }
    }

    return 0;
}