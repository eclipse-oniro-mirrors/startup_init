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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <unistd.h>

#include "init_log.h"
#include "bootevent.h"

uid_t __real_getuid();
uid_t __wrap_getuid()
{
    if (!IsBootCompleted()) {
        return __real_getuid();
    }

    INIT_LOGI("getuid begin");
    uid_t uid = __real_getuid();
    INIT_LOGI("getuid end");
    return uid;
}

int __real_mkdir(const char *pathname, mode_t mode);
int __wrap_mkdir(const char *pathname, mode_t mode)
{
    if (!IsBootCompleted()) {
        return __real_mkdir(pathname, mode);
    }

    INIT_LOGI("mkdir begin");
    int ret = __real_mkdir(pathname, mode);
    INIT_LOGI("mkdir end");
    return ret;
}

int __real_rmdir(const char *pathname);
int __wrap_rmdir(const char *pathname)
{
    if (!IsBootCompleted()) {
        return __real_rmdir(pathname);
    }

    INIT_LOGI("rmdir begin");
    int ret = __real_rmdir(pathname);
    INIT_LOGI("rmdir end");
    return ret;
}

pid_t __real_fork(void);
pid_t __wrap_fork(void)
{
    if (!IsBootCompleted()) {
        return __real_fork();
    }

    INIT_LOGI("fork begin");
    pid_t pid = __real_fork();
    INIT_LOGI("fork end");
    return pid;
}

int __real_mount(const char *source, const char *target,
                 const char *filesystemtype, unsigned long mountflags,
                 const void *data);
int __wrap_mount(const char *source, const char *target,
                 const char *filesystemtype, unsigned long mountflags,
                 const void *data)
{
    if (!IsBootCompleted()) {
        return __real_mount(source, target, filesystemtype, mountflags, data);
    }

    INIT_LOGI("mount begin");
    int ret = __real_mount(source, target, filesystemtype, mountflags, data);
    INIT_LOGI("mount end");
    return ret;
}

int __real_chown(const char *pathname, uid_t owner, gid_t group);
int __wrap_chown(const char *pathname, uid_t owner, gid_t group)
{
    if (!IsBootCompleted()) {
        return __real_chown(pathname, owner, group);
    }

    INIT_LOGI("chown begin");
    int ret = __real_chown(pathname, owner, group);
    INIT_LOGI("chown end");
    return ret;
}

int __real_chmod(const char *filename, int pmode);
int __wrap_chmod(const char *filename, int pmode)
{
    if (!IsBootCompleted()) {
        return __real_chmod(filename, pmode);
    }

    INIT_LOGI("chmod begin");
    int ret = __real_chmod(filename, pmode);
    INIT_LOGI("chmod end");
    return ret;
}

int __real_kill(pid_t pid, int sig);
int __wrap_kill(pid_t pid, int sig)
{
    if (!IsBootCompleted()) {
        return __real_kill(pid, sig);
    }

    INIT_LOGI("kill begin");
    int ret = __real_kill(pid, sig);
    INIT_LOGI("kill end");
    return ret;
}

FILE *__real_fopen(const char *filename, const char *mode);
FILE *__wrap_fopen(const char *filename, const char *mode)
{
    if (!IsBootCompleted()) {
        return __real_fopen(filename, mode);
    }

    INIT_LOGI("fopen begin");
    FILE *file = __real_fopen(filename, mode);
    INIT_LOGI("fopen end");
    return file;
}