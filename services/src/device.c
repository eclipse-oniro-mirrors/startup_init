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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include "init_log.h"

#define DEFAULT_RW_MODE 0666
#define DEFAULT_NO_AUTHORITY_MODE 0600
#define DEVICE_ID_THIRD 3
#define DEVICE_ID_EIGHTH 8
#define DEVICE_ID_NINTH 9
#define DEVICE_ID_ELEVNTH 11

void MountBasicFs()
{
    if (mount("tmpfs", "/dev", "tmpfs", MS_NOSUID, "mode=0755") != 0) {
        INIT_LOGE("Mount tmpfs failed. %s", strerror(errno));
    }
#ifndef __LITEOS__
    if (mkdir("/dev/pts", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
        INIT_LOGE("mkdir /dev/pts failed. %s", strerror(errno));
    }
    if (mount("devpts", "/dev/pts", "devpts", 0, NULL) != 0) {
        INIT_LOGE("Mount devpts failed. %s", strerror(errno));
    }
#endif
    if (mount("proc", "/proc", "proc", 0, "hidepid=2") != 0) {
        INIT_LOGE("Mount procfs failed. %s", strerror(errno));
    }
    if (mount("sysfs", "/sys", "sysfs", 0, NULL) != 0) {
        INIT_LOGE("Mount sysfs failed. %s", strerror(errno));
    }
#ifndef __LITEOS__
    if (mount("selinuxfs", "/sys/fs/selinux", "selinuxfs", 0, NULL) != 0) {
        INIT_LOGE("Mount selinuxfs failed. %s", strerror(errno));
    }
#endif
}

void CreateDeviceNode()
{
    if (mknod("/dev/kmsg", S_IFCHR | DEFAULT_NO_AUTHORITY_MODE, makedev(1, DEVICE_ID_ELEVNTH)) != 0) {
        INIT_LOGE("Create /dev/kmsg device node failed. %s", strerror(errno));
    }
    if (mknod("/dev/null", S_IFCHR | DEFAULT_RW_MODE, makedev(1, DEVICE_ID_THIRD)) != 0) {
        INIT_LOGE("Create /dev/null device node failed. %s", strerror(errno));
    }
    if (mknod("/dev/random", S_IFCHR | DEFAULT_RW_MODE, makedev(1, DEVICE_ID_EIGHTH)) != 0) {
        INIT_LOGE("Create /dev/random device node failed. %s", strerror(errno));
    }

    if (mknod("/dev/urandom", S_IFCHR | DEFAULT_RW_MODE, makedev(1, DEVICE_ID_NINTH)) != 0) {
        INIT_LOGE("Create /dev/urandom device node failed. %s", strerror(errno));
    }
}

int MakeSocketDir(const char *path, mode_t mode)
{
    int rc = mkdir("/dev/unix/", mode);
    if (rc < 0 && errno != EEXIST) {
        INIT_LOGE("Create %s failed. %d", path, errno);
        return -1;
    }
    rc = mkdir("/dev/unix/socket/", mode);
    if (rc < 0 && errno != EEXIST) {
        INIT_LOGE("Create %s failed. %d", path, errno);
        return -1;
    }
    return rc;
}

