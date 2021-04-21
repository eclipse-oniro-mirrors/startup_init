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

#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

void MountBasicFs()
{
    if (mount("tmpfs", "/dev", "tmpfs", MS_NOSUID, "mode=0755") != 0) {
        printf("Mount tmpfs failed. %s\n", strerror(errno));
    }
    if (mount("proc", "/proc", "proc", 0, "hidepid=2") != 0) {
        printf("Mount procfs failed. %s\n", strerror(errno));
    }
    if (mount("sysfs", "/sys", "sysfs", 0, NULL) != 0) {
        printf("Mount sysfs failed. %s\n", strerror(errno));
    }
}

void CreateDeviceNode()
{
    if (mknod("/dev/kmsg", S_IFCHR | 0600, makedev(1, 11)) != 0) {
        printf("Create /dev/kmsg device node failed. %s\n", strerror(errno));
    }
    if (mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3)) != 0) {
        printf("Create /dev/null device node failed. %s\n", strerror(errno));
    }
    if (mknod("/dev/random", S_IFCHR | 0666, makedev(1, 8)) != 0) {
        printf("Create /dev/random device node failed. %s\n", strerror(errno));
    }

    if (mknod("/dev/urandom", S_IFCHR | 0666, makedev(1, 9)) != 0) {
        printf("Create /dev/urandom device node failed. %s\n", strerror(errno));
    }
}
