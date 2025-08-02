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

#include "ueventd.h"
#include <dirent.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include "ueventd_device_handler.h"
#include "ueventd_firmware_handler.h"
#include "ueventd_read_cfg.h"
#include "ueventd_socket.h"
#include "ueventd_utils.h"
#include "securec.h"
#define INIT_LOG_TAG "ueventd"
#include "init_log.h"
#include "init_utils.h"

// buffer size refer to kernel kobject uevent
#define UEVENT_BUFFER_SIZE (2048 + 1)
char bootDevice[CMDLINE_VALUE_LEN_MAX] = { 0 };
#define WRITE_SIZE 4

static const char *actions[] = {
    [ACTION_ADD] = "add",
    [ACTION_REMOVE] = "remove",
    [ACTION_CHANGE] = "change",
    [ACTION_MOVE] = "move",
    [ACTION_ONLINE] = "online",
    [ACTION_OFFLINE] = "offline",
    [ACTION_BIND] = "bind",
    [ACTION_UNBIND] = "unbind",
    [ACTION_UNKNOWN] = "unknown",
};

static SUBSYSTEMTYPE GetSubsystemType(const char *subsystem)
{
    if (subsystem == NULL || *subsystem == '\0') {
        return SUBSYSTEM_EMPTY;
    }

    if (strcmp(subsystem, "block") == 0) {
        return SUBSYSTEM_BLOCK;
    } else if (strcmp(subsystem, "platform") == 0) {
        return SUBSYSTEM_PLATFORM;
    } else if (strcmp(subsystem, "firmware") == 0) {
        return SUBSYSTEM_FIRMWARE;
    } else {
        return SUBSYSTEM_OTHERS;
    }
}

const char *ActionString(ACTION action)
{
    return actions[action];
}

static ACTION GetUeventAction(const char *action)
{
    if (action == NULL || *action == '\0') {
        return ACTION_UNKNOWN;
    }

    if (STRINGEQUAL(action, "add")) {
        return ACTION_ADD;
    } else if (STRINGEQUAL(action, "remove")) {
        return ACTION_REMOVE;
    } else if (STRINGEQUAL(action, "change")) {
        return ACTION_CHANGE;
    } else if (STRINGEQUAL(action, "move")) {
        return ACTION_MOVE;
    } else if (STRINGEQUAL(action, "online")) {
        return ACTION_ONLINE;
    } else if (STRINGEQUAL(action, "offline")) {
        return ACTION_OFFLINE;
    } else if (STRINGEQUAL(action, "bind")) {
        return ACTION_BIND;
    } else if (STRINGEQUAL(action, "unbind")) {
        return ACTION_UNBIND;
    } else {
        return ACTION_UNKNOWN;
    }
}

STATIC void HandleUevent(const struct Uevent *uevent)
{
    if (uevent->action == ACTION_ADD || uevent->action == ACTION_CHANGE || uevent->action == ACTION_ONLINE) {
        ChangeSysAttributePermissions(uevent->syspath);
    }

    SUBSYSTEMTYPE type = GetSubsystemType(uevent->subsystem);
    switch (type) {
        case SUBSYSTEM_BLOCK:
            HandleBlockDeviceEvent(uevent);
            break;
        case SUBSYSTEM_FIRMWARE:
            HandleFimwareDeviceEvent(uevent);
            break;
        case SUBSYSTEM_OTHERS:
            HandleOtherDeviceEvent(uevent);
            break;
        default:
            break;
    }
}

#define DEFAULT_RW_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

typedef struct {
    const char *dev;
    mode_t mode;
} DYNAMIC_DEVICE_NODE;

#define DEV_NODE_PATH_PREFIX "/dev/"
#define DEV_NODE_PATH_PREFIX_LEN 5

static const DYNAMIC_DEVICE_NODE DYNAMIC_DEVICES[] = {
    { DEV_NODE_PATH_PREFIX"tty",              S_IFCHR | DEFAULT_RW_MODE },
    { DEV_NODE_PATH_PREFIX"binder",           S_IFCHR | DEFAULT_RW_MODE },
    { DEV_NODE_PATH_PREFIX"console",          S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP },
    { DEV_NODE_PATH_PREFIX"mapper/control",          S_IFCHR | DEFAULT_RW_MODE }
};

static void HandleRequiredDynamicDeviceNodes(const struct Uevent *uevent)
{
    mode_t mask;
    size_t idx = 0;

    if (uevent->deviceName == NULL || uevent->major < 0 || uevent->minor < 0) {
        return;
    }

    while (idx < sizeof(DYNAMIC_DEVICES) / sizeof(DYNAMIC_DEVICES[0])) {
        if (strcmp(uevent->deviceName, DYNAMIC_DEVICES[idx].dev + DEV_NODE_PATH_PREFIX_LEN) != 0) {
            idx++;
            continue;
        }

        if (strcmp(uevent->deviceName, "mapper/control") == 0) {
            HandleOtherDeviceEvent(uevent);
            return;
        }

        // Matched
        mask = umask(0);
        if (mknod(DYNAMIC_DEVICES[idx].dev, DYNAMIC_DEVICES[idx].mode,
            makedev((unsigned int)uevent->major, (unsigned int)uevent->minor)) != 0) {
            INIT_LOGE("Create device node %s failed. %s", DYNAMIC_DEVICES[idx].dev, strerror(errno));
        }
        // Restore umask
        umask(mask);
        break;
    }
}

static bool IsPatchPartitionName(const char *partitionName)
{
    return strcmp(partitionName, "patch_a") == 0 || strcmp(partitionName, "patch_b") == 0;
}

static void HandleRequiredBlockDeviceNodes(const struct Uevent *uevent, char **devices, int num)
{
    for (int i = 0; i < num; i++) {
        if (uevent->partitionName == NULL) {
            if (strstr(devices[i], uevent->deviceName) != NULL) {
                INIT_LOGI("%s match with required partition %s success, now handle it", devices[i], uevent->deviceName);
                HandleBlockDeviceEvent(uevent);
                return;
            }
        } else if (strstr(devices[i], uevent->partitionName) != NULL ||
            strstr(uevent->partitionName, "vendor") != NULL ||
            strstr(uevent->partitionName, "system") != NULL ||
            strstr(uevent->partitionName, "chipset") != NULL ||
            strstr(uevent->partitionName, "boot") != NULL ||
            strstr(uevent->partitionName, "ramdisk") != NULL ||
            strstr(uevent->partitionName, "rvt") != NULL ||
            strstr(uevent->partitionName, "dtbo") != NULL ||
            strstr(uevent->partitionName, "modem_driver") != NULL ||
            IsPatchPartitionName(uevent->partitionName)) {
            INIT_LOGI("Handle required partitionName %s", uevent->partitionName);
            HandleBlockDeviceEvent(uevent);
            return;
        }
    }
    INIT_LOGW("Not found device for partitionName %s ", uevent->partitionName);
}

static void HandleUeventRequired(const struct Uevent *uevent, char **devices, int num)
{
    INIT_ERROR_CHECK(devices != NULL && num > 0, return, "Fault parameters");
    if (uevent->action == ACTION_ADD) {
        ChangeSysAttributePermissions(uevent->syspath);
    }
    SUBSYSTEMTYPE type = GetSubsystemType(uevent->subsystem);
    if (type == SUBSYSTEM_BLOCK) {
        HandleRequiredBlockDeviceNodes(uevent, devices, num);
    } else if (type == SUBSYSTEM_OTHERS) {
        HandleRequiredDynamicDeviceNodes(uevent);
    } else {
        return;
    }
}

static void AddUevent(struct Uevent *uevent, const char *event, size_t len)
{
    if (STARTSWITH(event, "DEVPATH=")) {
        uevent->syspath = event + strlen("DEVPATH=");
    } else if (STARTSWITH(event, "SUBSYSTEM=")) {
        uevent->subsystem = event + strlen("SUBSYSTEM=");
    } else if (STARTSWITH(event, "ACTION=")) {
        uevent->action = GetUeventAction(event + strlen("ACTION="));
    } else if (STARTSWITH(event, "DEVNAME=")) {
        uevent->deviceName = event + strlen("DEVNAME=");
    } else if (STARTSWITH(event, "PARTNAME=")) {
        uevent->partitionName = event + strlen("PARTNAME=");
    } else if (STARTSWITH(event, "PARTN=")) {
        uevent->partitionNum = StringToInt(event + strlen("PARTN="), -1);
    } else if (STARTSWITH(event, "MAJOR=")) {
        uevent->major = StringToInt(event + strlen("MAJOR="), -1);
    } else if (STARTSWITH(event, "MINOR=")) {
        uevent->minor = StringToInt(event + strlen("MINOR="), -1);
    } else if (STARTSWITH(event, "DEVUID")) {
        uevent->ug.uid = (uid_t)StringToInt(event + strlen("DEVUID="), 0);
    } else if (STARTSWITH(event, "DEVGID")) {
        uevent->ug.gid = (gid_t)StringToInt(event + strlen("DEVGID="), 0);
    } else if (STARTSWITH(event, "FIRMWARE=")) {
        uevent->firmware = event + strlen("FIRMWARE=");
    } else if (STARTSWITH(event, "BUSNUM=")) {
        uevent->busNum = StringToInt(event + strlen("BUSNUM="), -1);
    } else if (STARTSWITH(event, "DEVNUM=")) {
        uevent->devNum = StringToInt(event + strlen("DEVNUM="), -1);
    }

    // Ignore other events
    INIT_LOGV("got uevent message:\n"
              "subsystem: %s\n"
              "parition: %s:%d\n"
              "action: %d\n"
              "devpath: %s\n"
              "devname: %s\n"
              "devnode: %d:%d\n"
              "id: %d:%d",
              uevent->subsystem,
              uevent->partitionName, uevent->partitionNum,
              uevent->action,
              uevent->syspath,
              uevent->deviceName,
              uevent->major, uevent->minor,
              uevent->ug.uid, uevent->ug.gid);
}

void ParseUeventMessage(const char *buffer, ssize_t length, struct Uevent *uevent)
{
    if (buffer == NULL || uevent == NULL || length == 0) {
        // Ignore invalid buffer
        return;
    }

    // reset partition number, major and minor.
    uevent->partitionName = NULL;
    uevent->partitionNum = -1;
    uevent->major = -1;
    uevent->minor = -1;
    uevent->busNum = -1;
    uevent->devNum = -1;
    ssize_t pos = 0;
    while (pos < length) {
        const char *event = buffer + pos;
        size_t len = strlen(event);
        if (len == 0) {
            break;
        }
        AddUevent(uevent, event, len);
        pos += (ssize_t)len + 1;
    }
}

void ProcessUevent(int sockFd, char **devices, int num, CompareUevent compare)
{
    // One more bytes for '\0'
    char ueventBuffer[UEVENT_BUFFER_SIZE] = {};
    ssize_t n = 0;
    struct Uevent uevent = {};
    while ((n = ReadUeventMessage(sockFd, ueventBuffer, sizeof(ueventBuffer) - 1)) > 0) {
        ParseUeventMessage(ueventBuffer, n, &uevent);
        if (uevent.syspath == NULL) {
            INIT_LOGV("Ignore unexpected uevent");
            return;
        }
        if (compare != NULL) {
            INIT_LOGV("find compare and do it");
            int ret = compare(&uevent);
            INIT_CHECK(ret == 0, return);
        }
        if (devices != NULL && num > 0) {
            HandleUeventRequired(&uevent, devices, num);
        } else {
            HandleUevent(&uevent);
        }
    }
}

static void DoTrigger(const char *ueventPath, int sockFd, char **devices, int num, CompareUevent compare)
{
    if (ueventPath == NULL || ueventPath[0] == '\0') {
        return;
    }

    INIT_LOGV("------------------------\n"
              "\nTry to trigger \" %s \" now ...", ueventPath);
    int fd = open(ueventPath, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        INIT_LOGE("Open \" %s \" failed, err = %d", ueventPath, errno);
        return;
    }

    ssize_t n = write(fd, "add\n", WRITE_SIZE);
    close(fd);
    if (n < 0) {
        INIT_LOGE("Write \" %s \" failed, err = %d", ueventPath, errno);
        return;
    }

    // uevent triggered, now handle it.
    if (sockFd >= 0) {
        ProcessUevent(sockFd, devices, num, compare);
    }
}

static void Trigger(const char *path, int sockFd, char **devices, int num, CompareUevent compare)
{
    if (path == NULL) {
        return;
    }
    DIR *dir = opendir(path);
    if (dir == NULL) {
        return;
    }
    struct dirent *dirent = NULL;
    while ((dirent = readdir(dir)) != NULL) {
        if (dirent->d_name[0] == '.') {
            continue;
        }
        if (dirent->d_type == DT_DIR) {
            char pathBuffer[PATH_MAX];
            if (snprintf_s(pathBuffer, PATH_MAX, PATH_MAX - 1, "%s/%s", path, dirent->d_name) == -1) {
                continue;
            }
            Trigger(pathBuffer, sockFd, devices, num, compare);
        } else {
            if (strcmp(dirent->d_name, "uevent") != 0) {
                continue;
            }
            char ueventBuffer[PATH_MAX];
            if (snprintf_s(ueventBuffer, PATH_MAX, PATH_MAX - 1, "%s/%s", path, "uevent") == -1) {
                INIT_LOGW("Cannot build uevent path under %s", path);
                continue;
            }
            DoTrigger(ueventBuffer, sockFd, devices, num, compare);
        }
    }
    closedir(dir);
}

void RetriggerUeventByPath(int sockFd, char *path)
{
    Trigger(path, sockFd, NULL, 0, NULL);
}

void RetriggerDmUeventByPath(int sockFd, char *path, char **devices, int num)
{
    Trigger(path, sockFd, devices, num, NULL);
}

void RetriggerSpecialUevent(int sockFd, char *path, char **devices, int num, CompareUevent compare)
{
    Trigger(path, sockFd, devices, num, compare);
}

void RetriggerUevent(int sockFd, char **devices, int num)
{
    int ret = GetParameterFromCmdLine("default_boot_device", bootDevice, CMDLINE_VALUE_LEN_MAX);
    INIT_CHECK_ONLY_ELOG(ret == 0, "Failed get default_boot_device value from cmdline");
    Trigger("/sys/block", sockFd, devices, num, NULL);
    Trigger("/sys/class", sockFd, devices, num, NULL);
    Trigger("/sys/devices", sockFd, devices, num, NULL);
}
