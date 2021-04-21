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

#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <dirent.h>
#include <linux/netlink.h>
#include "list.h"
#include "securec.h"

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

const char *TRIGGER = "/dev/.trigger_uevent";
static void HandleUevent();

static int UeventFD()
{
    return g_ueventFD;
}

static void DoTrigger(DIR *dir)
{
    struct dirent *de = NULL;
    int dfd = dirfd(dir);
    int fd = openat(dfd, "uevent", O_WRONLY);
    if (fd >= 0) {
        write(fd, "add\n", 4);
        close(fd);
        HandleUevent();
    }

    while ((de = readdir(dir)) != NULL) {
        DIR *dir2 = NULL;
        if (de->d_type != DT_DIR || de->d_name[0] == '.') {
            continue;
        }
        fd = openat(dfd, de->d_name, O_RDONLY | O_DIRECTORY);
        if (fd < 0) {
            continue;
        }

        dir2 = fdopendir(fd);
        if (dir2 == NULL) {
            close(fd);
        } else {
            DoTrigger(dir2);
            closedir(dir2);
        }
    }
}

void Trigger(const char *sysPath)
{
    DIR *dir = opendir(sysPath);
    if (dir) {
        DoTrigger(dir);
        closedir(dir);
    }
}

static void RetriggerUevent()
{
    if (access(TRIGGER, F_OK) == 0) {
        printf("Skip trigger uevent, alread done\n");
        return;
    }
    Trigger("/sys/class");
    Trigger("/sys/block");
    Trigger("/sys/devices");
    int fd = open(TRIGGER, O_WRONLY | O_CREAT | O_CLOEXEC, 0000);
    if (fd > 0) {
        close(fd);
    }
    printf("Re-trigger uevent done\n");
}

static void UeventSockInit()
{
    struct sockaddr_nl addr;
    int buffSize = 256 * 1024;
    int on = 1;

    if (memset_s(&addr, sizeof(addr), 0, sizeof(addr)) != 0) {
        return;
    }
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 0xffffffff;

    int sockfd = socket(PF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT);
    if (sockfd < 0) {
        printf("Create socket failed. %d\n", errno);
        return;
    }

    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUFFORCE, &buffSize, sizeof(buffSize));
    setsockopt(sockfd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));

    if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        printf("Bind socket failed. %d\n", errno);
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
    uid_t uid = -1;
    struct msghdr hdr = {
        &addr,
        sizeof(addr),
        &iov,
        1,
        control,
        sizeof(control),
        0,
    };
    ssize_t n = recvmsg(fd, &hdr, 0);
    if (n <= 0) {
        return n;
    }
    struct cmsghdr *cMsg = CMSG_FIRSTHDR(&hdr);
    if (cMsg == NULL || cMsg->cmsg_type != SCM_CREDENTIALS) {
        goto out;
    }
    struct ucred *cRed = (struct ucred *)CMSG_DATA(cMsg);
    uid = cRed->uid;
    if (uid != 0) {
        goto out;
    }
 
    if (addr.nl_groups == 0 || addr.nl_pid != 0) {
    /* ignoring non-kernel or unicast netlink message */
        goto out;
    }

    return n;
out:
    bzero(buf, len);
    errno = -EIO;
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
        if (strncmp(buf, "ACTION=", 7) == 0) {
            buf += 7;
            event->action = buf;
        } else if (strncmp(buf, "DEVPATH=", 8) == 0) {
            buf += 8;
            event->path = buf;
        } else if (strncmp(buf, "SUBSYSTEM=", 10) == 0) {
            buf += 10;
            event->subsystem = buf;
        } else if (strncmp(buf, "FIRMWARE=", 9) == 0) {
            buf += 9;
            event->firmware = buf;
        } else if (strncmp(buf, "MAJOR=", 6) == 0) {
            buf += 6;
            event->major = atoi(buf);
        } else if (strncmp(buf, "MINOR=", 6) == 0) {
            buf += 6;
            event->minor = atoi(buf);
        } else if (strncmp(buf, "PARTN=", 6) == 0) {
            buf += 6;
            event->partitionNum = atoi(buf);
        } else if (strncmp(buf, "PARTNAME=", 9) == 0) {
            buf += 9;
            event->partitionName = buf;
        } else if (strncmp(buf, "DEVNAME=", 8) == 0) {
            buf += 8;
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
        printf("Create %s failed. %d\n", path, errno);
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

static void Sanitize(char *s)
{
    const char* accept =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "_-.";

    if (!s) {
        return;
    }

    for (; *s; s++) {
        s += strspn(s, accept);
        if (*s) {
            *s = '_';
        }
    }
}

static char **ParsePlatformBlockDevice(const struct Uevent *uevent)
{
    const char *device;
    char *slash = NULL;
    const char *type;
    char linkPath[256];
    int linkNum = 0;
    char *p = NULL;

    struct PlatformNode *pDev = FindPlatformDevice(uevent->path);
    if (pDev) {
        device = pDev->name;
        type = "platform";
    } else {
        printf("Non platform device.\n");
        return NULL;
    }

    char **links = malloc(sizeof(char *) * 4);
    if (!links) {
        return NULL;
    }
    if (memset_s(links, sizeof(char *) * 4, 0, sizeof(char *) * 4) != 0) {
        return NULL;
    }
    printf("found %s device %s\n", type, device);
    if (snprintf_s(linkPath, sizeof(linkPath), sizeof(linkPath), "/dev/block/%s/%s", type, device) == -1) {
        return NULL;
    }
    if (uevent->partitionName) {
        p = strdup(uevent->partitionName);
        Sanitize(p);
        if (strcmp(uevent->partitionName, p)) {
            printf("Linking partition '%s' as '%s'\n", uevent->partitionName, p);
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

static void MakeDevice(const char *devpath, const char *path, int block, int major, int minor)
{
    /* Only for super user */
    gid_t gid = 0;
    dev_t dev;
    mode_t mode = 0600;
    mode |= (block ? S_IFBLK : S_IFCHR);
    dev = makedev(major, minor);
    setegid(gid);
    if (mknod(devpath, mode, dev) != 0) {
        if (errno != EEXIST) {
            printf("Make device node[%d, %d] failed. %d\n", major, minor, errno);
        }
    }
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
            printf("path too long for MkdirRecursive\n");
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
    char path[256];
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
    char buf[256];
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
    int ret = MkdirRecursive(buf, 0755);
    if (ret) {
        printf("Failed to create directory %s: %s (%d)\n", buf, strerror(errno), errno);
    }
    ret = symlink(oldPath, newPath);
    if (ret && errno != EEXIST) {
        printf("Failed to symlink %s to %s: %s (%d)\n", oldPath, newPath, strerror(errno), errno);
    }
}

static void HandleDevice(const char *action, const char *devpath, const char *path, int block, int major,
    int minor, char **links)
{
    int i;
    if (!strcmp(action, "add")) {
        MakeDevice(devpath, path, block, major, minor);
        if (links) {
            for (i = 0; links[i]; i++) {
                MakeLink(devpath, links[i]);
            }
        }
    }

    if (!strcmp(action, "remove")) {
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
    char devpath[96];
    char **links = NULL;

    if (event->major < 0 || event->minor < 0) {
        return;
    }
    const char *name = strrchr(event->path, '/');
    if (name == NULL) {
        return;
    }
    name++;
    if (strlen(name) > 64) { // too long
        return;
    }
    if (snprintf_s(devpath, sizeof(devpath), sizeof(devpath), "%s/%s", base, name) == -1) {
        return;
    }
    MakeDir(base, 0755);
    if (!strncmp(event->path, "/devices/", 9)) {
        links = ParsePlatformBlockDevice(event);
    }
    HandleDevice(event->action, devpath, event->path, 1, event->major, event->minor, links);
}

static void AddPlatformDevice(const char *path)
{
    size_t pathLen = strlen(path);
    const char *name = path;

    if (!strncmp(path, "/devices/", 9)) {
        name += 9;
        if (!strncmp(name, "platform/", 9)) {
            name += 9;
        }
    }
    printf("adding platform device %s (%s)\n", name, path);
    struct PlatformNode *bus = calloc(1, sizeof(struct PlatformNode));
    bus->path = strdup(path);
    bus->pathLen = pathLen;
    bus->name = bus->path + (name - path);
    ListAddTail(&g_platformNames, &bus->list);
}

static void RemovePlatformDevice(const char *path)
{
    struct ListNode *node = NULL;
    struct PlatformNode *bus = NULL;

    for (node = (&g_platformNames)->prev; node != &g_platformNames; node = node->prev) {
        bus = (struct PlatformNode *)(((char*)(node)) - offsetof(struct PlatformNode, list));
        if (!strcmp(path, bus->path)) {
            printf("removing platform device %s\n", bus->name);
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

    char **links = malloc(sizeof(char *) * 2);
    if (!links) {
        return NULL;
    }
    if (memset_s(links, sizeof(char *) * 2, 0, sizeof(char *) * 2) != 0) {
        return NULL;
    }

    /* skip "/devices/platform/<driver>" */
    const char *parent = strchr(uevent->path + pDev->pathLen, '/');
    if (!*parent) {
        goto err;
    }

    if (!strncmp(parent, "/usb", 4)) {
       /* skip root hub name and device. use device interface */
        while (*++parent && *parent != '/') {}
        if (*parent) {
            while (*++parent && *parent != '/') {}
        }
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
        mkdir("/dev/usb", 0755);
    } else {
        goto err;
    }
    return links;
err:
    free(links);
    return NULL;
}

static void HandleGenericDevice(struct Uevent *event)
{
    char *base = NULL;
    char devpath[96] = {0};
    char **links = NULL;

    const char *name = ParseDeviceName(event, 64);
    if (!name) {
        return;
    }
    if (!strncmp(event->subsystem, "usb", 3)) {
        if (!strcmp(event->subsystem, "usb")) {
            if (event->deviceName) {
                /*
                 * create device node provided by kernel if present
                 * see drivers/base/core.c
                 */
                char *p = devpath;
                if (snprintf_s(devpath, sizeof(devpath), sizeof(devpath), "/dev/%s", event->deviceName) == -1) {
                    return;
                }
                /* skip leading /dev/ */
                p += 5;
                /* build directories */
                while (*p) {
                    if (*p == '/') {
                        *p = 0;
                        MakeDir(devpath, 0755);
                        *p = '/';
                    }
                    p++;
                }
            } else {
                /* This imitates the file system that would be created
                 * if we were using devfs instead.
                 * Minors are broken up into groups of 128, starting at "001"
                 */
                int busId  = event->minor / 128 + 1;
                int deviceId = event->minor % 128 + 1;
                /* build directories */
                MakeDir("/dev/bus", 0755);
                MakeDir("/dev/bus/usb", 0755);
                if (snprintf_s(devpath, sizeof(devpath), sizeof(devpath), "/dev/bus/usb/%03d", busId) == -1) {
                    return;
                }
                MakeDir(devpath, 0755);
                if (snprintf_s(devpath, sizeof(devpath), sizeof(devpath), "/dev/bus/usb/%03d/%03d", busId,
                    deviceId) == -1) {
                    return;
                }
            }
        } else {
            /* ignore other USB events */
            return;
        }
    } else if (!strncmp(event->subsystem, "graphics", 8)) {
        base = "/dev/graphics/";
        MakeDir(base, 0755);
    } else if (!strncmp(event->subsystem, "drm", 3)) {
        base = "/dev/dri/";
        MakeDir(base, 0755);
    } else if (!strncmp(event->subsystem, "oncrpc", 6)) {
        base = "/dev/oncrpc/";
        MakeDir(base, 0755);
    } else if (!strncmp(event->subsystem, "adsp", 4)) {
        base = "/dev/adsp/";
        MakeDir(base, 0755);
    } else if (!strncmp(event->subsystem, "input", 5)) {
        base = "/dev/input/";
        MakeDir(base, 0755);
    } else if (!strncmp(event->subsystem, "mtd", 3)) {
        base = "/dev/mtd/";
        MakeDir(base, 0755);
    } else if (!strncmp(event->subsystem, "sound", 5)) {
        base = "/dev/snd/";
        MakeDir(base, 0755);
    } else if (!strncmp(event->subsystem, "misc", 4) && !strncmp(name, "log_", 4)) {
        base = "/dev/log/";
        MakeDir(base, 0755);
        name += 4;
    } else {
        base = "/dev/";
    }
    links = GetCharacterDeviceSymlinks(event);
    if (!devpath[0]) {
        if (snprintf_s(devpath, sizeof(devpath), sizeof(devpath), "%s%s", base, name) == -1) {
            return;
        }
    }
    HandleDevice(event->action, devpath, event->path, 0,
        event->major, event->minor, links);
}

static void HandleDeviceUevent(struct Uevent *event)
{
    if (strcmp(event->action, "add") == 0 || strcmp(event->action, "change") == 0) {
        /* Do nothing for now */
    }
    if (strncmp(event->subsystem, "block", 5) == 0) {
        HandleBlockDevice(event);
    } else if (strncmp(event->subsystem, "platform", 8) == 0) {
        HandlePlatformDevice(event);
    } else {
        HandleGenericDevice(event);
    }
}

static void HandleUevent()
{
    char buf[1024 + 2];
    int ret;
    struct Uevent event;
    while ((ret = ReadUevent(g_ueventFD, buf, 1024)) > 0) {
        if (ret >= 1024) {
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

int main(int argc, char **argv)
{
    printf("Uevent demo starting...\n");
    UeventInit();
    return 0;
}
