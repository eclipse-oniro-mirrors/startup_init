/*
 * Copyright (c) 2020 Huawei Device Co., Ltd.
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

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/un.h>
#include <unistd.h>
#include "init_log.h"
#include "list.h"
#include "securec.h"

#define LINK_NUMBER 4
#define DEFAULT_DIR_MODE (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)
#define TRIGGER_ADDR_SIZE 4
#define BASE_BUFFER_SIZE 1024
#define MAX_BUFFER 256
#define EVENT_MAX_BUFFER 1026
#define MAX_DEV_PATH 96
#define MINORS_GROUPS 128
#define SYS_LINK_NUMBER 2
#define MAX_DEVICE_LEN 64
#define DEFAULT_MODE 0000
#define DEVICE_DEFAULT_MODE (S_IRUSR | S_IWUSR | S_IRGRP)

int g_ueventFD = -1;

struct Uevent {
    const char *action;
    const char *path;
    const char *subsystem;
    const char *firmware;
    const char *partitionName;
    const char *deviceName;
    int partitionNum;
    int major;
    int minor;
};

struct PlatformNode {
    char *name;
    char *path;
    size_t pathLen;
    struct ListNode list;
};

static struct ListNode g_platformNames = {
    .next = &g_platformNames,
    .prev = &g_platformNames,
};

const char *g_trigger = "/dev/.trigger_uevent";
static void HandleUevent();

static int UeventFD()
{
    return g_ueventFD;
}

static void DoTrigger(DIR *dir, const char *path, int32_t pathLen)
{
    if (pathLen < 0) {
        return;
    }
    struct dirent *dirent = NULL;
    char ueventPath[MAX_BUFFER];
    if (snprintf_s(ueventPath, sizeof(ueventPath), sizeof(ueventPath) - 1, "%s/uevent", path) == -1) {
        return;
    }
    int fd = open(ueventPath, O_WRONLY);
    if (fd >= 0) {
        write(fd, "add\n", TRIGGER_ADDR_SIZE);
        close(fd);
        HandleUevent();
    }

    while ((dirent = readdir(dir)) != NULL) {
        if (dirent->d_name[0] == '.' || dirent->d_type != DT_DIR) {
            continue;
        }
        char tmpPath[MAX_BUFFER];
        if (snprintf_s(tmpPath, sizeof(tmpPath), sizeof(tmpPath) - 1, "%s/%s", path, dirent->d_name) == -1) {
            continue;
        }

        DIR *dir2 = opendir(tmpPath);
        if (dir2) {
            DoTrigger(dir2, tmpPath, strlen(tmpPath));
            closedir(dir2);
        }
    }
}

void Trigger(const char *sysPath)
{
    DIR *dir = opendir(sysPath);
    if (dir) {
        DoTrigger(dir, sysPath, strlen(sysPath));
        closedir(dir);
    }
}

static void RetriggerUevent()
{
    if (access(g_trigger, F_OK) == 0) {
        INIT_LOGI("Skip trigger uevent, alread done");
        return;
    }
    Trigger("/sys/class");
    Trigger("/sys/block");
    Trigger("/sys/devices");
    int fd = open(g_trigger, O_WRONLY | O_CREAT | O_CLOEXEC, DEFAULT_MODE);
    if (fd >= 0) {
        close(fd);
    }
    INIT_LOGI("Re-trigger uevent done");
}

static void UeventSockInit()
{
    struct sockaddr_nl addr;
    int buffSize = MAX_BUFFER * BASE_BUFFER_SIZE;
    int on = 1;

    if (memset_s(&addr, sizeof(addr), 0, sizeof(addr)) != 0) {
        return;
    }
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 0xffffffff;

    int sockfd = socket(PF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT);
    if (sockfd < 0) {
        INIT_LOGE("Create socket failed. %d", errno);
        return;
    }

    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUFFORCE, &buffSize, sizeof(buffSize));
    setsockopt(sockfd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));

    if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        INIT_LOGE("Bind socket failed. %d", errno);
        close(sockfd);
        return;
    }
    g_ueventFD = sockfd;
    fcntl(g_ueventFD, F_SETFD, FD_CLOEXEC);
    fcntl(g_ueventFD, F_SETFL, O_NONBLOCK);
    RetriggerUevent();
    return;
}

ssize_t ReadUevent(int fd, void *buf, size_t len)
{
    struct iovec iov = { buf, len };
    struct sockaddr_nl addr;
    char control[CMSG_SPACE(sizeof(struct ucred))];
    struct msghdr hdr;
    hdr.msg_name = &addr;
    hdr.msg_namelen = sizeof(addr);
    hdr.msg_iov = &iov;
    hdr.msg_iovlen  = 1;
    hdr.msg_control = control;
    hdr.msg_controllen = sizeof(control);
    hdr.msg_flags = 0;
    ssize_t n = recvmsg(fd, &hdr, 0);
    if (n <= 0) {
        return n;
    }
    struct cmsghdr *cMsg = CMSG_FIRSTHDR(&hdr);
    if (cMsg == NULL || cMsg->cmsg_type != SCM_CREDENTIALS) {
        bzero(buf, len);
        errno = -EIO;
        return n;
    }
    struct ucred *cRed = (struct ucred *)CMSG_DATA(cMsg);
    if (cRed->uid != 0) {
        bzero(buf, len);
        errno = -EIO;
        return n;
    }

    if (addr.nl_groups == 0 || addr.nl_pid != 0) {
    /* ignoring non-kernel or unicast netlink message */
        bzero(buf, len);
        errno = -EIO;
        return n;
    }

    return n;
}

static void InitUevent(struct Uevent *event)
{
    event->action = "";
    event->path = "";
    event->subsystem = "";
    event->firmware = "";
    event->partitionName = "";
    event->deviceName = "";
    event->partitionNum = -1;
}

static void ParseUevent(const char *buf, struct Uevent *event)
{
    InitUevent(event);
    while (*buf) {
        if (strncmp(buf, "ACTION=", strlen("ACTION=")) == 0) {
            buf += strlen("ACTION=");
            event->action = buf;
        } else if (strncmp(buf, "DEVPATH=", strlen("DEVPATH=")) == 0) {
            buf += strlen("DEVPATH=");
            event->path = buf;
        } else if (strncmp(buf, "SUBSYSTEM=", strlen("SUBSYSTEM=")) == 0) {
            buf += strlen("SUBSYSTEM=");
            event->subsystem = buf;
        } else if (strncmp(buf, "FIRMWARE=", strlen("FIRMWARE=")) == 0) {
            buf += strlen("FIRMWARE=");
            event->firmware = buf;
        } else if (strncmp(buf, "MAJOR=", strlen("MAJOR=")) == 0) {
            buf += strlen("MAJOR=");
            event->major = atoi(buf);
        } else if (strncmp(buf, "MINOR=", strlen("MINOR=")) == 0) {
            buf += strlen("MINOR=");
            event->minor = atoi(buf);
        } else if (strncmp(buf, "PARTN=", strlen("PARTN=")) == 0) {
            buf += strlen("PARTN=");
            event->partitionNum = atoi(buf);
        } else if (strncmp(buf, "PARTNAME=", strlen("PARTNAME=")) == 0) {
            buf += strlen("PARTNAME=");
            event->partitionName = buf;
        } else if (strncmp(buf, "DEVNAME=", strlen("DEVNAME=")) == 0) {
            buf += strlen("DEVNAME=");
            event->deviceName = buf;
        }
        // Drop reset.
        while (*buf++) {}
    }
}

static int MakeDir(const char *path, mode_t mode)
{
    int rc = mkdir(path, mode);
    if (rc < 0 && errno != EEXIST) {
        INIT_LOGE("Create %s failed. %d", path, errno);
    }
    return rc;
}

static struct PlatformNode *FindPlatformDevice(const char *path)
{
    size_t pathLen = strlen(path);
    struct ListNode *node = NULL;
    struct PlatformNode *bus = NULL;

    for (node = (&g_platformNames)->prev; node != &g_platformNames; node = node->prev) {
        bus = (struct PlatformNode *)(((char*)(node)) - offsetof(struct PlatformNode, list));
        if ((bus->pathLen < pathLen) && (path[bus->pathLen] == '/') && !strncmp(path, bus->path, bus->pathLen)) {
            return bus;
        }
    }
    return NULL;
}

static void CheckValidPartitionName(char *partitionName)
{
    const char* supportPartition = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_-";
    if (!partitionName) {
        return;
    }

    for (size_t i = 0; i < strlen(partitionName); i++) {
        int len = strspn(partitionName, supportPartition);
        partitionName += len;
        i = len;
        if (*partitionName) {
            *partitionName = '_';
        }
    }
}

static char **ParsePlatformBlockDevice(const struct Uevent *uevent)
{
    const char *device;
    char *slash = NULL;
    const char *type;
    char linkPath[MAX_BUFFER];
    int linkNum = 0;
    char *p = NULL;

    struct PlatformNode *pDev = FindPlatformDevice(uevent->path);
    if (!pDev) {
        return NULL;
    }
    device = pDev->name;
    type = "platform";
    char **links = calloc(sizeof(char *), LINK_NUMBER);
    if (!links) {
        return NULL;
    }
    if (snprintf_s(linkPath, sizeof(linkPath), sizeof(linkPath) - 1, "/dev/block/%s/%s", type, device) == -1) {
        free(links);
        return NULL;
    }
    if (uevent->partitionName) {
        p = strdup(uevent->partitionName);
        CheckValidPartitionName(p);
        if (strcmp(uevent->partitionName, p)) {
            INIT_LOGI("Linking partition '%s' as '%s'", uevent->partitionName, p);
        }
        if (asprintf(&links[linkNum], "%s/by-name/%s", linkPath, p) > 0) {
            linkNum++;
        } else {
            links[linkNum] = NULL;
        }
        free(p);
    }
    if (uevent->partitionNum >= 0) {
        if (asprintf(&links[linkNum], "%s/by-num/p%d", linkPath, uevent->partitionNum) > 0) {
            linkNum++;
        } else {
            links[linkNum] = NULL;
        }
    }
    slash = strrchr(uevent->path, '/');
    if (asprintf(&links[linkNum], "%s/%s", linkPath, slash + 1) > 0) {
        linkNum++;
    } else {
        links[linkNum] = NULL;
    }
    return links;
}

struct DevPermissionMapper {
    char *devName;
    mode_t devMode;
    uid_t uid;
    gid_t gid;
};

struct DevPermissionMapper DEV_MAPPER[] = {
    {"/dev/binder", 0666, 0, 0},
    {"/dev/input/event0", 0660, 0, 0},
    {"/dev/input/event1", 0660, 0, 1004},
    {"/dev/input/mice", 0660, 0, 1004},
    {"/dev/input/mouse0", 0660, 0, 0},
    {"/dev/snd/timer", 0660, 1000, 1005},
    {"/dev/zero", 0666, 0, 0},
    {"/dev/full", 0666, 0, 0},
    {"/dev/ptmx", 0666, 0, 0},
    {"/dev/tty", 0666, 0, 0},
    {"/dev/random", 0666, 0, 0},
    {"/dev/urandom", 0666, 0, 0},
    {"/dev/ashmem", 0666, 0, 0},
    {"/dev/pmsg0", 0222, 0, 1007},
    {"/dev/jpeg", 0666, 1000, 1003},
    {"/dev/vinput", 0660, 1000, 1004},
    {"/dev/mmz_userdev", 0644, 1000, 1005},
    {"/dev/graphics/fb0", 0660, 1000, 1003},
    {"/dev/mem", 0660, 1000, 1005},
    {"/dev/ion", 0666, 1000, 1000},
    {"/dev/btusb0", 0660, 1002, 1002},
    {"/dev/uhid", 0660, 3011, 3011},
    {"/dev/tc_ns_client", 0660, 1000, 1005},
    {"/dev/rtk_btusb", 0660, 1002, 0},
    {"/dev/sil9293", 0660, 1000, 1005},
    {"/dev/stpbt", 0660, 1002, 1001},
    {"/dev/avs", 0660, 1000, 1005},
    {"/dev/gdc", 0660, 1000, 1005},
    {"/dev/hdmi", 0660, 1000, 1005},
    {"/dev/hi_mipi", 0660, 1000, 1005},
    {"/dev/hi_mipi_tx", 0660, 1000, 1005},
    {"/dev/hi_tde", 0644, 1000, 1003},
    {"/dev/isp_dev", 0660, 1000, 1006},
    {"/dev/match", 0660, 1000, 1005},
    {"/dev/photo", 0660, 1000, 1005},
    {"/dev/rect", 0660, 1000, 1005},
    {"/dev/rgn", 0660, 1000, 1005},
    {"/dev/sys", 0660, 1000, 1005},
    {"/dev/vb", 0666, 1000, 1005},
    {"/dev/vdec", 0666, 1000, 1005},
    {"/dev/venc", 0666, 1000, 1005},
    {"/dev/vi", 0660, 1000, 1005},
    {"/dev/vo", 0660, 1000, 1005},
    {"/dev/vpss", 0660, 1000, 1005},
    {"/dev/i2c-0", 0660, 1000, 1006},
    {"/dev/i2c-1", 0660, 1000, 1006},
    {"/dev/i2c-2", 0660, 1000, 1006},
    {"/dev/i2c-3", 0660, 1000, 1006},
    {"/dev/i2c-4", 0660, 1000, 1006},
    {"/dev/i2c-5", 0660, 1000, 1006},
    {"/dev/i2c-6", 0660, 1000, 1006},
    {"/dev/i2c-7", 0660, 1000, 1006},
    {"/dev/vgs", 0666, 1000, 1005},
    {"/dev/dri/card0", 0666, 0, 1003},
    {"/dev/dri/card0-DSI-1", 0666, 0, 1003},
    {"/dev/dri/card0-HDMI-A-1", 0666, 0, 1003},
    {"/dev/dri/renderD128", 0666, 0, 1003},
    {"/dev/rtc0", 0640, 1000, 1000},
    {"/dev/tty0", 0660, 0, 1000},
    {"/dev/uinput", 0660, 3011, 3011}
};

static void AdjustDevicePermission(const char *devPath)
{
    for (unsigned int i = 0; i < sizeof(DEV_MAPPER) / sizeof(struct DevPermissionMapper); ++i) {
        if (strcmp(devPath, DEV_MAPPER[i].devName) == 0) {
            if (chmod(devPath, DEV_MAPPER[i].devMode) != 0) {
                INIT_LOGE("AdjustDevicePermission, failed for %s, err %d.", devPath, errno);
                return;
            }
            if (chown(devPath, DEV_MAPPER[i].uid, DEV_MAPPER[i].gid) != 0) {
                INIT_LOGE("AdjustDevicePermission, failed for %s, err %d.", devPath, errno);
                return;
            }
            INIT_LOGI("AdjustDevicePermission :%s success", devPath);
        }
    }
}

static void MakeDevice(const char *devPath, const char *path, int block, int major, int minor)
{
    /* Only for super user */
    gid_t gid = 0;
    dev_t dev;
    mode_t mode = DEVICE_DEFAULT_MODE;
    mode |= (block ? S_IFBLK : S_IFCHR);
    dev = makedev(major, minor);
    setegid(gid);
    if (mknod(devPath, mode, dev) != 0) {
        if (errno != EEXIST) {
            INIT_LOGE("Make device node[%d, %d] failed. %d", major, minor, errno);
        }
    }
    AdjustDevicePermission(devPath);
}

int MkdirRecursive(const char *pathName, mode_t mode)
{
    char buf[128];
    const char *slash;
    const char *p = pathName;
    struct stat info;

    while ((slash = strchr(p, '/')) != NULL) {
        int width = slash - pathName;
        p = slash + 1;
        if (width < 0) {
            break;
        }
        if (width == 0) {
            continue;
        }
        if ((unsigned int)width > sizeof(buf) - 1) {
            INIT_LOGE("path too long for MkdirRecursive");
            return -1;
        }
        if (memcpy_s(buf, width, pathName, width) != 0) {
            return -1;
        }
        buf[width] = 0;
        if (stat(buf, &info) != 0) {
            int ret = MakeDir(buf, mode);
            if (ret && errno != EEXIST) {
                return ret;
            }
        }
    }
    int ret = MakeDir(pathName, mode);
    if (ret && errno != EEXIST) {
        return ret;
    }
    return 0;
}

void RemoveLink(const char *oldpath, const char *newpath)
{
    char path[MAX_BUFFER];
    ssize_t ret = readlink(newpath, path, sizeof(path) - 1);
    if (ret <= 0) {
        return;
    }
    path[ret] = 0;
    if (!strcmp(path, oldpath)) {
        unlink(newpath);
    }
}

static void MakeLink(const char *oldPath, const char *newPath)
{
    char buf[MAX_BUFFER];
    char *slash = strrchr(newPath, '/');
    if (!slash) {
        return;
    }
    int width = slash - newPath;
    if (width <= 0 || width > (int)sizeof(buf) - 1) {
        return;
    }
    if (memcpy_s(buf, sizeof(buf), newPath, width) != 0) {
        return;
    }
    buf[width] = 0;
    int ret = MkdirRecursive(buf, DEFAULT_DIR_MODE);
    if (ret) {
        INIT_LOGE("Failed to create directory %s: %s (%d)", buf, strerror(errno), errno);
    }
    ret = symlink(oldPath, newPath);
    if (ret && errno != EEXIST) {
        INIT_LOGE("Failed to symlink %s to %s: %s (%d)", oldPath, newPath, strerror(errno), errno);
    }
}

static void HandleDevice(struct Uevent *event, const char *devpath, int block, char **links)
{
    int i;
    if (!strcmp(event->action, "add")) {
        MakeDevice(devpath, event->path, block, event->major, event->minor);
        if (links) {
            for (i = 0; links[i]; i++) {
                MakeLink(devpath, links[i]);
            }
        }
    }

    if (!strcmp(event->action, "remove")) {
        if (links) {
            for (i = 0; links[i]; i++) {
                RemoveLink(devpath, links[i]);
            }
        }
        unlink(devpath);
    }

    if (links) {
        for (i = 0; links[i]; i++) {
            free(links[i]);
        }
        free(links);
    }
}

static void HandleBlockDevice(struct Uevent *event)
{
    const char *base = "/dev/block";
    char devpath[MAX_DEV_PATH];
    char **links = NULL;

    if (event->major < 0 || event->minor < 0) {
        return;
    }
    const char *name = strrchr(event->path, '/');
    if (name == NULL) {
        return;
    }
    name++;
    if (strlen(name) > MAX_DEVICE_LEN) { // too long
        return;
    }
    if (snprintf_s(devpath, sizeof(devpath), sizeof(devpath) - 1, "%s/%s", base, name) == -1) {
        return;
    }
    MakeDir(base, DEFAULT_DIR_MODE);
    if (!strncmp(event->path, "/devices/", strlen("/devices/"))) {
        links = ParsePlatformBlockDevice(event);
    }
    HandleDevice(event, devpath, 1, links);
}

static void AddPlatformDevice(const char *path)
{
    size_t pathLen = strlen(path);
    const char *name = path;
    size_t deviceLength = strlen("/devices/");
    size_t platformLength = strlen("platform/");
    if (!strncmp(path, "/devices/", deviceLength)) {
        name += deviceLength;
        if (!strncmp(name, "platform/", platformLength)) {
            name += platformLength;
        }
    }
    INIT_LOGI("adding platform device %s (%s)", name, path);
    struct PlatformNode *bus = calloc(1, sizeof(struct PlatformNode));
    if (!bus) {
        return;
    }
    bus->path = strdup(path);
    bus->pathLen = pathLen;
    bus->name = bus->path + (name - path);
    ListAddTail(&g_platformNames, &bus->list);
    return;
}

static void RemovePlatformDevice(const char *path)
{
    struct ListNode *node = NULL;
    struct PlatformNode *bus = NULL;

    for (node = (&g_platformNames)->prev; node != &g_platformNames; node = node->prev) {
        bus = (struct PlatformNode *)(((char*)(node)) - offsetof(struct PlatformNode, list));
        if (!strcmp(path, bus->path)) {
            INIT_LOGI("removing platform device %s", bus->name);
            free(bus->path);
            ListRemove(node);
            free(bus);
            return;
        }
    }
}

static void HandlePlatformDevice(const struct Uevent *event)
{
    const char *path = event->path;
    if (strcmp(event->action, "add") == 0) {
        AddPlatformDevice(path);
    } else if (strcmp(event->action, "remove") == 0) {
        RemovePlatformDevice(path);
    }
}

static const char *ParseDeviceName(const struct Uevent *uevent, unsigned int len)
{
    /* if it's not a /dev device, nothing else to do */
    if ((uevent->major < 0) || (uevent->minor < 0)) {
        return NULL;
    }
    /* do we have a name? */
    const char *name = strrchr(uevent->path, '/');
    if (!name) {
        return NULL;
    }
    name++;
    /* too-long names would overrun our buffer */
    if (strlen(name) > len) {
        return NULL;
    }
    return name;
}

static char **GetCharacterDeviceSymlinks(const struct Uevent *uevent)
{
    char *slash = NULL;
    int linkNum = 0;
    int width;

    struct PlatformNode *pDev = FindPlatformDevice(uevent->path);
    if (!pDev) {
        return NULL;
    }
    char **links = calloc(sizeof(char *), SYS_LINK_NUMBER);
    if (!links) {
        return NULL;
    }

    /* skip "/devices/platform/<driver>" */
    const char *parent = strchr(uevent->path + pDev->pathLen, '/');
    if (!*parent) {
        goto err;
    }

    if (strncmp(parent, "/usb", strlen("/usb"))) {
        goto err;
    }
    /* skip root hub name and device. use device interface */
    if (!*parent) {
        goto err;
    }
    slash = strchr(++parent, '/');
    if (!slash) {
        goto err;
    }
    width = slash - parent;
    if (width <= 0) {
        goto err;
    }

    if (asprintf(&links[linkNum], "/dev/usb/%s%.*s", uevent->subsystem, width, parent) > 0) {
        linkNum++;
    } else {
        links[linkNum] = NULL;
    }
    mkdir("/dev/usb", DEFAULT_DIR_MODE);
    return links;
err:
    free(links);
    return NULL;
}

static int HandleUsbDevice(const struct Uevent *event, char *devpath, int len)
{
    if (event->deviceName) {
        /*
         * create device node provided by kernel if present
         * see drivers/base/core.c
         */
        char *p = devpath;
        if (snprintf_s(devpath, len, len - 1, "/dev/%s", event->deviceName) == -1) {
            return -1;
        }
        /* skip leading /dev/ */
        p += strlen("/dev/");
        /* build directories */
        while (*p) {
            if (*p == '/') {
                *p = 0;
                MakeDir(devpath, DEFAULT_DIR_MODE);
                *p = '/';
            }
            p++;
        }
    } else {
        /* This imitates the file system that would be created
         * if we were using devfs instead.
         * Minors are broken up into groups of 128, starting at "001"
         */
        int busId  = event->minor / MINORS_GROUPS + 1;
        int deviceId = event->minor % MINORS_GROUPS + 1;
        /* build directories */
        MakeDir("/dev/bus", DEFAULT_DIR_MODE);
        MakeDir("/dev/bus/usb", DEFAULT_DIR_MODE);
        if (snprintf_s(devpath, len, len - 1, "/dev/bus/usb/%03d", busId) == -1) {
            return -1;
        }
        MakeDir(devpath, DEFAULT_DIR_MODE);
        if (snprintf_s(devpath, len, len - 1, "/dev/bus/usb/%03d/%03d", busId,
            deviceId) == -1) {
            return -1;
        }
    }
    return 0;
}

static void HandleDeviceEvent(struct Uevent *event, char *devpath, int len, const char *base, const char *name)
{
    char **links = NULL;
    links = GetCharacterDeviceSymlinks(event);
    if (!devpath[0]) {
        if (snprintf_s(devpath, len, len - 1, "%s%s", base, name) == -1) {
            INIT_LOGE("snprintf_s err ");
            goto err;
        }
    }
    HandleDevice(event, devpath, 0, links);
    return;
err:
    if (links) {
        for (int i = 0; links[i]; i++) {
            free(links[i]);
        }
        free(links);
    }
    return;
}
static void HandleGenericDevice(struct Uevent *event)
{
    char *base = NULL;
    char devpath[MAX_DEV_PATH] = {0};
    const char *name = ParseDeviceName(event, MAX_DEVICE_LEN);
    if (!name) {
        return;
    }
    if (!strncmp(event->subsystem, "usb", strlen("usb"))) {
        if (!strcmp(event->subsystem, "usb")) {
            if (HandleUsbDevice(event, devpath, MAX_DEV_PATH) == -1) {
                return;
            }
        } else {
            /* ignore other USB events */
            return;
        }
    } else if (!strncmp(event->subsystem, "graphics", strlen("graphics"))) {
        base = "/dev/graphics/";
        MakeDir(base, DEFAULT_DIR_MODE);
    } else if (!strncmp(event->subsystem, "drm", strlen("drm"))) {
        base = "/dev/dri/";
        MakeDir(base, DEFAULT_DIR_MODE);
    } else if (!strncmp(event->subsystem, "oncrpc", strlen("oncrpc"))) {
        base = "/dev/oncrpc/";
        MakeDir(base, DEFAULT_DIR_MODE);
    } else if (!strncmp(event->subsystem, "adsp", strlen("adsp"))) {
        base = "/dev/adsp/";
        MakeDir(base, DEFAULT_DIR_MODE);
    } else if (!strncmp(event->subsystem, "input", strlen("input"))) {
        base = "/dev/input/";
        MakeDir(base, DEFAULT_DIR_MODE);
    } else if (!strncmp(event->subsystem, "mtd", strlen("mtd"))) {
        base = "/dev/mtd/";
        MakeDir(base, DEFAULT_DIR_MODE);
    } else if (!strncmp(event->subsystem, "sound", strlen("sound"))) {
        base = "/dev/snd/";
        MakeDir(base, DEFAULT_DIR_MODE);
    } else {
        base = "/dev/";
    }
    HandleDeviceEvent(event, devpath, MAX_DEV_PATH, base, name);
    return;
}

static void HandleDeviceUevent(struct Uevent *event)
{
    if (strcmp(event->action, "add") == 0 || strcmp(event->action, "change") == 0) {
        /* Do nothing for now */
    }
    if (strncmp(event->subsystem, "block", strlen("block")) == 0) {
        HandleBlockDevice(event);
    } else if (strncmp(event->subsystem, "platform", strlen("platform")) == 0) {
        HandlePlatformDevice(event);
    } else {
        HandleGenericDevice(event);
    }
}

static void HandleUevent()
{
    char buf[EVENT_MAX_BUFFER];
    int ret;
    struct Uevent event;
    while ((ret = ReadUevent(g_ueventFD, buf, BASE_BUFFER_SIZE)) > 0) {
        if (ret >= BASE_BUFFER_SIZE) {
            continue;
        }
        buf[ret] = '\0';
        buf[ret + 1] = '\0';
        ParseUevent(buf, &event);
        HandleDeviceUevent(&event);
    }
}

void UeventInit()
{
    struct pollfd ufd;
    UeventSockInit();
    ufd.events = POLLIN;
    ufd.fd = UeventFD();
    while (1) {
        ufd.revents = 0;
        int ret = poll(&ufd, 1, -1);
        if (ret <= 0) {
            continue;
        }
        if (ufd.revents & POLLIN) {
            HandleUevent();
        }
    }
    return;
}

int main(const int argc, const char **argv)
{
    INIT_LOGI("Uevent demo starting...");
    UeventInit();
    return 0;
}
