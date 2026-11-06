/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#include <stddef.h>
#include <unistd.h>

#include "mkdir_test_wrapper.h"

/*
 * Test-only wrappers for DoMkDir. Every wrapper delegates to the real libc
 * symbol unless a test installs a mock through the corresponding Update*Func.
 */
extern int __real_lstat(const char *pathname, struct stat *buf);
extern int __real_fchmodat(int dirfd, const char *pathname, mode_t mode, int flags);
extern int __real_lchown(const char *pathname, uid_t owner, gid_t group);
extern int __real_fstatat(int dirfd, const char *pathname, struct stat *buf, int flags);
extern void __real_PluginExecCmdByName(const char *name, const char *cmdContent);
extern int __real_SetFileCryptPolicy(const char *dir);
extern int __real_chown(const char *pathname, uid_t owner, gid_t group);
extern struct passwd *__real_getpwnam(const char *name);
extern struct group *__real_getgrnam(const char *name);
extern struct group *__real_getgrent(void);

static LstatFunc g_lstat = NULL;

void UpdateLstatFunc(LstatFunc func)
{
    g_lstat = func;
}

int RealLstat(const char *pathname, struct stat *buf)
{
    return __real_lstat(pathname, buf);
}

int __wrap_lstat(const char *pathname, struct stat *buf)
{
    if (g_lstat != NULL) {
        return g_lstat(pathname, buf);
    }
    return RealLstat(pathname, buf);
}

static FchmodatFunc g_fchmodat = NULL;

void UpdateFchmodatFunc(FchmodatFunc func)
{
    g_fchmodat = func;
}

int __wrap_fchmodat(int dirfd, const char *pathname, mode_t mode, int flags)
{
    if (g_fchmodat != NULL) {
        return g_fchmodat(dirfd, pathname, mode, flags);
    }
    return __real_fchmodat(dirfd, pathname, mode, flags);
}

static LchownFunc g_lchown = NULL;

void UpdateLchownFunc(LchownFunc func)
{
    g_lchown = func;
}

int __wrap_lchown(const char *pathname, uid_t owner, gid_t group)
{
    if (g_lchown != NULL) {
        return g_lchown(pathname, owner, group);
    }
    return __real_lchown(pathname, owner, group);
}

static FstatatFunc g_fstatat = NULL;

void UpdateFstatatFunc(FstatatFunc func)
{
    g_fstatat = func;
}

int RealFstatat(int dirfd, const char *pathname, struct stat *buf, int flags)
{
    return __real_fstatat(dirfd, pathname, buf, flags);
}

int __wrap_fstatat(int dirfd, const char *pathname, struct stat *buf, int flags)
{
    if (g_fstatat != NULL) {
        return g_fstatat(dirfd, pathname, buf, flags);
    }
    return RealFstatat(dirfd, pathname, buf, flags);
}

static PluginExecCmdFunc g_pluginExecCmd = NULL;

void UpdatePluginExecCmdFunc(PluginExecCmdFunc func)
{
    g_pluginExecCmd = func;
}

void __wrap_PluginExecCmdByName(const char *name, const char *cmdContent)
{
    if (g_pluginExecCmd != NULL) {
        g_pluginExecCmd(name, cmdContent);
        return;
    }
    __real_PluginExecCmdByName(name, cmdContent);
}

static ChownFunc g_chown = NULL;

void UpdateChownFunc(ChownFunc func)
{
    g_chown = func;
}

int __wrap_chown(const char *pathname, uid_t owner, gid_t group)
{
    if (g_chown != NULL) {
        return g_chown(pathname, owner, group);
    }
    return __real_chown(pathname, owner, group);
}

static GetPwNamFunc g_getpwnam = NULL;

void UpdateGetPwNamFunc(GetPwNamFunc func)
{
    g_getpwnam = func;
}

struct passwd *__wrap_getpwnam(const char *name)
{
    if (g_getpwnam != NULL) {
        return g_getpwnam(name);
    }
    return __real_getpwnam(name);
}

static GetGrNamFunc g_getgrnam = NULL;

void UpdateGetGrNamFunc(GetGrNamFunc func)
{
    g_getgrnam = func;
}

struct group *__wrap_getgrnam(const char *name)
{
    if (g_getgrnam != NULL) {
        return g_getgrnam(name);
    }
    return __real_getgrnam(name);
}

static GetGRentFunc g_getgrent = NULL;

void UpdateGetGRentFunc(GetGRentFunc func)
{
    g_getgrent = func;
}

struct group *__wrap_getgrent(void)
{
    if (g_getgrent != NULL) {
        return g_getgrent();
    }
    return __real_getgrent();
}

static SetFileCryptPolicyFunc g_setFileCryptPolicy = NULL;

void UpdateSetFileCryptPolicyFunc(SetFileCryptPolicyFunc func)
{
    g_setFileCryptPolicy = func;
}

int __wrap_SetFileCryptPolicy(const char *dir)
{
    if (g_setFileCryptPolicy != NULL) {
        return g_setFileCryptPolicy(dir);
    }
    return __real_SetFileCryptPolicy(dir);
}


#if defined(__arm__)
extern int __real___lstat_time64(const char *pathname, struct stat *buf);
extern int __real___fstatat_time64(int dirfd, const char *pathname, struct stat *buf, int flags);

int __wrap___lstat_time64(const char *pathname, struct stat *buf)
{
    if (g_lstat != NULL) {
        return g_lstat(pathname, buf);
    }
    return __real___lstat_time64(pathname, buf);
}

int __wrap___fstatat_time64(int dirfd, const char *pathname, struct stat *buf, int flags)
{
    if (g_fstatat != NULL) {
        return g_fstatat(dirfd, pathname, buf, flags);
    }
    return __real___fstatat_time64(dirfd, pathname, buf, flags);
}
#endif

void ResetMkdirTestWrappers(void)
{
    g_lstat = NULL;
    g_fchmodat = NULL;
    g_lchown = NULL;
    g_fstatat = NULL;
    g_pluginExecCmd = NULL;
    g_setFileCryptPolicy = NULL;
    g_chown = NULL;
    g_getpwnam = NULL;
    g_getgrnam = NULL;
    g_getgrent = NULL;
}
