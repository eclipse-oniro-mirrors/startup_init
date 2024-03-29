/*
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd.
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
#include "device.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <linux/major.h>
#include "init_log.h"

#define DEFAULT_RW_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)
#define DEFAULT_NO_AUTHORITY_MODE (S_IWUSR | S_IRUSR)

static void MountBasicFs(void)
{
    if (access("dev/null", F_OK) != 0) {
        INIT_LOGI("mount dev tmpfs");
        if (mount("tmpfs", "/dev", "tmpfs", MS_NOSUID, "mode=0755") != 0) {
            INIT_LOGE("Mount tmpfs failed. %s", strerror(errno));
        }
    } else {
        INIT_LOGI("dev already mounted");
    }
    if (mount("tmpfs", "/mnt", "tmpfs", MS_NOSUID, "mode=0755") != 0) {
        INIT_LOGE("Mount tmpfs failed. %s", strerror(errno));
    }
    if (mount(NULL, "/mnt", NULL, MS_SLAVE, NULL) != 0) {
        INIT_LOGE("Mount tmpfs slave failed. %s", strerror(errno));
    }
    if (mkdir("/mnt/data", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
        INIT_LOGE("mkdir /mnt/data failed. %s", strerror(errno));
    }
    if (mount("tmpfs", "/mnt/data", "tmpfs", MS_NOSUID, "mode=0755") != 0) {
        INIT_LOGE("Mount tmpfs failed. %s", strerror(errno));
    }
    if (mount(NULL, "/mnt/data", NULL, MS_SHARED, NULL) != 0) {
        INIT_LOGE("Mount tmpfs shared failed. %s", strerror(errno));
    }
    if (mount("tmpfs", "/storage", "tmpfs", MS_NOEXEC | MS_NODEV| MS_NOSUID, "mode=0755") != 0) {
        INIT_LOGE("Mount storage failed. %s", strerror(errno));
    }
    struct stat st;
    if (!(stat("/dev/pts", &st) == 0 && S_ISDIR(st.st_mode))) {
        if (mkdir("/dev/pts", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
            INIT_LOGE("mkdir /dev/pts failed. %s", strerror(errno));
        }
        if (mount("devpts", "/dev/pts", "devpts", 0, NULL) != 0) {
            INIT_LOGE("Mount devpts failed. %s", strerror(errno));
        }
    }
    if (mount("proc", "/proc", "proc", 0, "gid=3009,hidepid=2") != 0) {
        INIT_LOGE("Mount procfs failed. %s", strerror(errno));
    }
    if (mount("sysfs", "/sys", "sysfs", 0, NULL) != 0) {
        INIT_LOGE("Mount sysfs failed. %s", strerror(errno));
    }
    if (mount("selinuxfs", "/sys/fs/selinux", "selinuxfs", 0, NULL) != 0) {
        INIT_LOGE("Mount selinuxfs failed. %s", strerror(errno));
    }
}

static void CreateDeviceNode(void)
{
    if (access("/dev/null", F_OK) != 0) {
        if (mknod("/dev/null", S_IFCHR | DEFAULT_RW_MODE, makedev(MEM_MAJOR, DEV_NULL_MINOR)) != 0) {
            INIT_LOGE("Create /dev/null device node failed. %s", strerror(errno));
        }
    }

    if (access("/dev/random", F_OK) != 0) {
        if (mknod("/dev/random", S_IFCHR | DEFAULT_RW_MODE, makedev(MEM_MAJOR, DEV_RANDOM_MINOR)) != 0) {
            INIT_LOGE("Create /dev/random device node failed. %s", strerror(errno));
        }
    }

    if (access("/dev/urandom", F_OK) != 0) {
        if (mknod("/dev/urandom", S_IFCHR | DEFAULT_RW_MODE, makedev(MEM_MAJOR, DEV_URANDOM_MINOR)) != 0) {
            INIT_LOGW("Create /dev/urandom device node failed. %s", strerror(errno));
        }
    }
}

static void EnableDevKmsg(void)
{
    /* printk_devkmsg default value is ratelimit, We need to set "on" and remove the restrictions */
    int fd = open("/proc/sys/kernel/printk_devkmsg", O_WRONLY | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd < 0) {
        return;
    }
    char *kmsgStatus = "on";
    write(fd, kmsgStatus, strlen(kmsgStatus) + 1);
    close(fd);
    fd = -1;
    return;
}

void CreateFsAndDeviceNode(void)
{
    MountBasicFs();
    CreateDeviceNode();
    EnableDevKmsg();
}
