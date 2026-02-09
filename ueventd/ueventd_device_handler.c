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

#include "ueventd_device_handler.h"

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include "init_utils.h"
#include "ueventd.h"
#ifndef __RAMDISK__
#include "ueventd_parameter.h"
#endif
#include "ueventd_read_cfg.h"
#include "ueventd_utils.h"
#include "securec.h"
#define INIT_LOG_TAG "ueventd"
#include "init_log.h"
#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#include <policycoreutils.h>
#endif

static inline void AdjustDeviceNodePermissions(const char *deviceNode, uid_t uid, gid_t gid, mode_t mode)
{
    if (INVALIDSTRING(deviceNode)) {
        return;
    }
    if (chown(deviceNode, uid, gid) != 0) {
        INIT_LOGW("Failed to change \" %s \" owner, errno %d", deviceNode, errno);
    }

    if (chmod(deviceNode, mode) != 0) {
        INIT_LOGW("Failed to change \" %s \" mode, errno %d", deviceNode, errno);
    }
}

static void CreateSymbolLinks(const char *deviceNode, char **symLinks)
{
    if (INVALIDSTRING(deviceNode) || symLinks == NULL) {
        return;
    }

    uid_t uid = 0;
    gid_t gid = 0;
    mode_t mode = DEVMODE;

    for (int i = 0; symLinks[i] != NULL; i++) {
        const char *linkName = symLinks[i];
        char linkBuf[DEVICE_FILE_SIZE] = {};

        if (strstr(linkName, "/dev/block/by-name") != NULL) {
            int res = GetDeviceNodePermissions(linkName, &uid, &gid, &mode);
            INIT_CHECK(res != 0, AdjustDeviceNodePermissions(deviceNode, uid, gid, mode));
        }

        if (strncpy_s(linkBuf, DEVICE_FILE_SIZE - 1, linkName, strlen(linkName)) != EOK) {
            INIT_LOGE("Failed to copy link name");
            return;
        }
        const char *linkDir = dirname(linkBuf);
        if (MakeDirRecursive(linkDir, DIRMODE) < 0) {
            INIT_LOGE("[uevent] Failed to create dir \" %s \", err = %d", linkDir, errno);
            return;
        }

        errno = 0;
        INIT_LOGI("symlink %s->%s", deviceNode, linkName);
        int rc = symlink(deviceNode, linkName);
        if (rc != 0) {
            if (errno != EEXIST) {
                INIT_LOGE("Failed to link \" %s \" to \" %s \", err = %d", deviceNode, linkName, errno);
            }
        }
    }
}

#if defined(WITH_SELINUX) && !defined(__RAMDISK__)
static void SetDeviceLable(const char *path, char **symLinks)
{
    int rc = 0;
    char buffer[PATH_MAX] = {};
    const char *p = NULL;
    char *slash = NULL;

    p = path;
    slash = strchr(path, '/');
    while (slash != NULL) {
        int gap = slash - p;
        p = slash + 1;
        if (gap == 0) {
            slash = strchr(p, '/');
            continue;
        }
        if (gap < 0) { // end with '/'
            return;
        }

        if (memcpy_s(buffer, PATH_MAX, path, p - path - 1) != EOK) {
            INIT_LOGE("[uevent] Failed to memcpy path %s", path);
            return;
        }
        rc += Restorecon(buffer);
        slash = strchr(p, '/');
    }

    rc += Restorecon(path);
    if (rc != 0) {
        INIT_LOGE("[uevent] Failed to Restorecon \" %s \"", path);
    }

    INIT_CHECK_ONLY_RETURN(symLinks != NULL);
    char *context = NULL;
    for (int i = 0; symLinks[i] != NULL; i++) {
        const char *linkName = symLinks[i];
        const char *byNamePath = "/dev/block/by-name";
        if (strncmp(linkName, byNamePath, strlen(byNamePath)) == 0) {
            (void)Restorecon(linkName);
            lgetfilecon(linkName, &context);
            if (context != NULL) {
                setfilecon(path, context);
            }
            return;
        }
    }

    return;
}
#endif

static int CreateDeviceNodeWithPermissions(const struct Uevent *uevent, const char *deviceNode, bool isBlock)
{
    int major = uevent->major;
    int minor = uevent->minor;
    uid_t uid = uevent->ug.uid;
    gid_t gid = uevent->ug.gid;
    mode_t mode = DEVMODE;

    (void)GetDeviceNodePermissions(deviceNode, &uid, &gid, &mode);
    mode |= isBlock ? S_IFBLK : S_IFCHR;
    dev_t dev = makedev((unsigned int)major, (unsigned int)minor);

    (void)setegid(gid);
    mode_t originalMask = umask(000);
    int rc = mknod(deviceNode, mode, dev);
    (void)umask(originalMask);
    if (rc < 0 && errno != EEXIST) {
        INIT_LOGE("Create device node[%s %d, %d] failed. %d", deviceNode, major, minor, errno);
        return rc;
    }
    if (strncmp(deviceNode, "/dev/input", strlen("/dev/input")) == 0) {
        INIT_LOGI("HandleOtherDeviceEvent, deviceNode = %s", deviceNode);
    }
    AdjustDeviceNodePermissions(deviceNode, uid, gid, mode);
    (void)setegid(0);
    return 0;
}

static int CreateDeviceNode(const struct Uevent *uevent, const char *deviceNode, char **symLinks, bool isBlock)
{
    int rc = -1;

    if (deviceNode == NULL || *deviceNode == '\0') {
        INIT_LOGE("Invalid device file");
        return rc;
    }

    char deviceNodeBuffer[DEVICE_FILE_SIZE] = {};
    if (strncpy_s(deviceNodeBuffer, DEVICE_FILE_SIZE - 1, deviceNode, strlen(deviceNode)) != EOK) {
        INIT_LOGE("Failed to copy device node");
        return rc;
    }
    const char *devicePath = dirname(deviceNodeBuffer);
    // device node always installed in /dev, should not be other locations.
    if (STRINGEQUAL(devicePath, ".") || STRINGEQUAL(devicePath, "/")) {
        INIT_LOGE("device path is not valid. should be starts with /dev");
        return rc;
    }

    rc = MakeDirRecursive(devicePath, DIRMODE);
    if (rc < 0) {
        INIT_LOGE("Create path \" %s \" failed", devicePath);
        return rc;
    }

    rc = CreateDeviceNodeWithPermissions(uevent, deviceNode, isBlock);
    if (rc < 0) {
        return rc;
    }
    if (symLinks != NULL) {
        CreateSymbolLinks(deviceNode, symLinks);
    }
#if defined(WITH_SELINUX) && !defined(__RAMDISK__)
    SetDeviceLable(deviceNode, symLinks);
#endif
    // No matter what result the symbol links returns,
    // as long as create device node done, just returns success.
    rc = 0;
    return rc;
}

static int RemoveDeviceNode(const char *deviceNode, char **symLinks)
{
    if (INVALIDSTRING(deviceNode)) {
        INIT_LOGE("Invalid device node");
        return -1;
    }
    if (symLinks != NULL) {
        for (int i = 0; symLinks[i] != NULL; i++) {
            char realPath[DEVICE_FILE_SIZE] = {0};
            const char *linkName = symLinks[i];
            ssize_t ret = readlink(linkName, realPath, DEVICE_FILE_SIZE - 1);
            if (ret < 0) {
                continue;
            }
            if (STRINGEQUAL(deviceNode, realPath)) {
                INIT_LOGI("unlink %s", linkName);
                unlink(linkName);
            }
        }
    }
    INIT_LOGI("unlink %s", deviceNode);
    return unlink(deviceNode);
}

static char *FindPlatformDeviceName(char *path)
{
    if (INVALIDSTRING(path)) {
        return NULL;
    }

    if (STARTSWITH(path, "/sys/devices/platform/")) {
        path += strlen("/sys/devices/platform/");
        return path;
    }

    // Some platform devices may not be registered under platform device.
    if (STARTSWITH(path, "/sys/devices/")) {
        path += strlen("/sys/devices/");
        return path;
    }
    return NULL;
}

static int BuildDeviceSymbolLinks(char **links, int linkNum, const char *parent,
    const char *partitionName, const char *deviceName)
{
    int num = linkNum;
    links[num] = calloc(DEVICE_FILE_SIZE, sizeof(char));
    if (links[num] == NULL) {
        INIT_LOGE("Failed to allocate memory for link, err = %d", errno);
        return num;
    }

    // If a block device without partition name.
    // For now, we will not create symbol link for it.
    if (!INVALIDSTRING(partitionName)) {
        if (snprintf_s(links[num], DEVICE_FILE_SIZE, DEVICE_FILE_SIZE - 1,
            "/dev/block/platform/%s/by-name/%s", parent, partitionName) == -1) {
            INIT_LOGE("Failed to build link");
        }
        if (STRINGEQUAL(parent, bootDevice)) {
            num = linkNum + 1;
            links[num] = calloc(DEVICE_FILE_SIZE, sizeof(char));
            if (links[num] == NULL) {
                INIT_LOGE("Failed to allocate memory for link, err = %d", errno);
                return linkNum;
            }
            if (snprintf_s(links[num], DEVICE_FILE_SIZE, DEVICE_FILE_SIZE - 1,
                "/dev/block/by-name/%s", partitionName) == -1) {
                INIT_LOGE("Failed to build link");
            }
        } else {
            INIT_LOGI("%s and %s is not match", parent, bootDevice);
        }
    } else if (!INVALIDSTRING(deviceName)) {
        if (snprintf_s(links[num], DEVICE_FILE_SIZE, DEVICE_FILE_SIZE - 1,
            "/dev/block/platform/%s/%s", parent, deviceName) == -1) {
            INIT_LOGE("Failed to build link");
        }
    }

    return num;
}

static void FreeSymbolLinks(char **links, int length)
{
    if (links != NULL) {
        for (int i = 0; i < length && links[i] != NULL; i++) {
            free(links[i]);
            links[i] = NULL;
        }
        free(links);
        links = NULL;
    }
}

static char **GetBlockDeviceSymbolLinks(const struct Uevent *uevent)
{
    if (uevent == NULL || uevent->subsystem == NULL || STRINGEQUAL(uevent->subsystem, "block") == 0) {
        INIT_LOGW("Invalid arguments, Skip to get device symbol links.");
        return NULL;
    }

    // Only if current uevent is for real device.
    if (!STARTSWITH(uevent->syspath, "/devices")) {
        return NULL;
    }
    // For block device under one platform device.
    // check subsystem file under directory, see if it links to bus/platform.
    // For now, only support platform device.
    char sysPath[SYSPATH_SIZE] = {};
    if (snprintf_s(sysPath, SYSPATH_SIZE, SYSPATH_SIZE - 1, "/sys%s", uevent->syspath) == -1) {
        INIT_LOGE("Failed to build sys path for device %s", uevent->syspath);
        return NULL;
    }
    char **links = calloc(BLOCKDEVICE_LINKS, sizeof(char *));
    int linkNum = 0;
    if (links == NULL) {
        INIT_LOGE("Failed to allocate memory for links, err = %d", errno);
        return NULL;
    }

    // Reverse walk through sysPath, and check subsystem file under each directory.
    char *parent = dirname(sysPath);
    while (parent != NULL && !STRINGEQUAL(parent, "/") && !STRINGEQUAL(parent, ".")) {
        char subsystem[SYSPATH_SIZE];
        if (snprintf_s(subsystem, SYSPATH_SIZE, SYSPATH_SIZE - 1, "%s/subsystem", parent) == -1) {
            INIT_LOGE("Failed to build subsystem path for device \" %s \"", uevent->syspath);
            FreeSymbolLinks(links, BLOCKDEVICE_LINKS);
            return NULL;
        }
        char *bus = GetRealPath(subsystem);
        if (bus == NULL) {
            parent = dirname(parent);
            continue;
        }
        if (STRINGEQUAL(bus, "/sys/bus/platform")) {
            INIT_LOGV("Find a platform device: %s", parent);
            parent = FindPlatformDeviceName(parent);
            if (parent != NULL) {
                INIT_WARNING_CHECK(linkNum < BLOCKDEVICE_LINKS - 1, free(bus); links[linkNum] = NULL;
                    return links, "Too many links, ignore");
                linkNum = BuildDeviceSymbolLinks(links, linkNum, parent, uevent->partitionName, uevent->deviceName);
                linkNum++;
            }
        }
        free(bus);
        parent = dirname(parent);
    }

    links[linkNum] = NULL;
    return links;
}

static void HandleDeviceNode(const struct Uevent *uevent, const char *deviceNode, bool isBlock)
{
    ACTION action = uevent->action;
    char **symLinks = NULL;

    // Block device path and name maybe not human readable.
    // Consider to create symbol links for them.
    // Make block device more readable.
    if (isBlock) {
        symLinks = GetBlockDeviceSymbolLinks(uevent);
    }

    if (action == ACTION_ADD) {
        if (CreateDeviceNode(uevent, deviceNode, symLinks, isBlock) < 0) {
            INIT_LOGE("Create device \" %s \" failed", deviceNode);
        } else {
#ifndef __RAMDISK__
            if (SetUeventDeviceParameter(deviceNode, action) != 0) {
                INIT_LOGE("Set device parameter added failed");
            }
#endif
        }
    } else if (action == ACTION_REMOVE) {
        if (RemoveDeviceNode(deviceNode, symLinks) < 0) {
            INIT_LOGE("Remove device \" %s \" failed", deviceNode);
        } else {
#ifndef __RAMDISK__
            if (SetUeventDeviceParameter(deviceNode, action) != 0) {
                INIT_LOGE("Set device parameter removed failed");
            }
#endif
        }
    } else if (action == ACTION_CHANGE) {
        INIT_LOGV("Device %s changed", uevent->syspath);
    }
    // Ignore other actions
    FreeSymbolLinks(symLinks, BLOCKDEVICE_LINKS);
}

static const char *GetDeviceName(char *sysPath, const char *deviceName)
{
    const char *devName = NULL;
    if (INVALIDSTRING(sysPath)) {
        INIT_LOGE("Invalid sys path");
        return NULL;
    }
    if (deviceName != NULL && deviceName[0] != '\0') {
        // if device name reported by kernel includes '/', skip it.
        // use entire device name reported by kernel
        devName = basename((char *)deviceName);
        char *p = strrchr(deviceName, '/');
        if (p != NULL) { // device name includes slash
            p++;
            if (p == NULL || *p == '\0') {
                // device name ends with '/', which should never happen.
                // Get name from sys path.
                devName = basename(sysPath);
            } else {
                devName = p;
            }
        }
    } else {
        // kernel does not report DEVNAME, which is possible. use base name of syspath instead.
        devName = basename(sysPath);
    }
    return devName;
}

static const char *GetDeviceBasePath(const char *subsystem)
{
    char *devPath = NULL;
    if (INVALIDSTRING(subsystem)) {
        return devPath;
    }

    if (STRINGEQUAL(subsystem, "block")) {
        devPath = "/dev/block";
    } else if (STRINGEQUAL(subsystem, "input")) {
        devPath = "/dev/input";
    } else if (STRINGEQUAL(subsystem, "drm")) {
        devPath = "/dev/dri";
    } else if (STRINGEQUAL(subsystem, "graphics")) {
        devPath = "/dev/graphics";
    } else if (STRINGEQUAL(subsystem, "sound")) {
        devPath = "/dev/snd";
    } else if (STRINGEQUAL(subsystem, "functionfs")) {
        devPath = "/dev/functionfs";
    } else if (STRINGEQUAL(subsystem, "dma_heap")) {
        devPath = "/dev/dma_heap";
    } else {
        devPath = "/dev";
    }
    return devPath;
}

void HandleBlockDeviceEvent(const struct Uevent *uevent)
{
    // Sanity checks
    if (uevent == NULL || uevent->subsystem == NULL) {
        INIT_LOGE("Invalid uevent message received");
        return;
    }

    if (strcmp(uevent->subsystem, "block") != 0) {
        INIT_LOGE("Unexpected uevent subsystem \" %s \" received in block device handler", uevent->subsystem);
        return;
    }

    if (uevent->major < 0 || uevent->minor < 0) {
        return;
    }

    bool isBlock = true;
    // Block device always installed into /dev/block
    const char *devPath = GetDeviceBasePath(uevent->subsystem);
    char deviceNode[DEVICE_FILE_SIZE] = {};
    char sysPath[SYSPATH_SIZE] = {};

    if (uevent->syspath == NULL) {
        return;
    }
    if (strncpy_s(sysPath, SYSPATH_SIZE - 1, uevent->syspath, strlen(uevent->syspath) != EOK)) {
        INIT_LOGE("Failed to copy sys path");
        return;
    }
    const char *devName = GetDeviceName(sysPath, uevent->deviceName);

    if (devPath == NULL || devName == NULL) {
        INIT_LOGE("Cannot get device path or device name");
        return;
    }
    if (snprintf_s(deviceNode, DEVICE_FILE_SIZE, DEVICE_FILE_SIZE - 1, "%s/%s", devPath, devName) == -1) {
        INIT_LOGE("Make device file for device [%d : %d]", uevent->major, uevent->minor);
        return;
    }
    HandleDeviceNode(uevent, deviceNode, isBlock);
}

void HandleOtherDeviceEvent(const struct Uevent *uevent)
{
    if (uevent == NULL || uevent->subsystem == NULL || uevent->syspath == NULL) {
        INIT_LOGE("Invalid uevent received");
        return;
    }

    if (uevent->major < 0 || uevent->minor < 0) {
        return;
    }

    char deviceNode[DEVICE_FILE_SIZE] = {};
    char sysPath[SYSPATH_SIZE] = {};
    if (strncpy_s(sysPath, SYSPATH_SIZE - 1, uevent->syspath, strlen(uevent->syspath)) != EOK) {
        INIT_LOGE("Failed to copy sys path");
        return;
    }
    const char *devName = GetDeviceName(sysPath, uevent->deviceName);
    const char *devPath = GetDeviceBasePath(uevent->subsystem);

    if (devPath == NULL || devName == NULL) {
        INIT_LOGE("Cannot get device path or device name");
        return;
    }
    if (strcmp(devPath, "/dev/input") == 0) {
        INIT_LOGI("HandleOtherDeviceEvent, devPath = %s, devName = %s", devPath, devName);
    }
    INIT_LOGV("HandleOtherDeviceEvent, devPath = %s, devName = %s", devPath, devName);

    // For usb devices, should take care of it specially.
    // if usb devices report DEVNAME, just create device node.
    // otherwise, create deviceNode with bus number and device number.
    if (STRINGEQUAL(uevent->subsystem, "usb")) {
        if (uevent->deviceName != NULL) {
            if (snprintf_s(deviceNode, DEVICE_FILE_SIZE, DEVICE_FILE_SIZE - 1, "/dev/%s", uevent->deviceName) == -1) {
                INIT_LOGE("Make device file for device [%d : %d]", uevent->major, uevent->minor);
                return;
            }
        } else {
            if (uevent->busNum < 0 || uevent->devNum < 0) {
                // usb device should always report bus number and device number.
                INIT_LOGE("usb device with invalid bus number or device number");
                return;
            }
            if (snprintf_s(deviceNode, DEVICE_FILE_SIZE, DEVICE_FILE_SIZE - 1,
                "/dev/bus/usb/%03d/%03d", uevent->busNum, uevent->devNum) == -1) {
                INIT_LOGE("Make usb device node for device [%d : %d]", uevent->busNum, uevent->devNum);
            }
        }
    } else if (STARTSWITH(uevent->subsystem, "usb")) {
        // Other usb devies, do not handle it.
        return;
    } else {
        if (strcmp(uevent->deviceName, "mapper/control") == 0) {
            devName = "mapper/control";
        }
        if (snprintf_s(deviceNode, DEVICE_FILE_SIZE, DEVICE_FILE_SIZE - 1, "%s/%s", devPath, devName) == -1) {
            INIT_LOGE("Make device file for device [%d : %d]", uevent->major, uevent->minor);
            return;
        }
    }
    HandleDeviceNode(uevent, deviceNode, false);
}
