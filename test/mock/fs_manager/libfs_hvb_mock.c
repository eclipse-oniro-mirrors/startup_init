/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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


#include "libfs_hvb_mock.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "bootstage.h"
#include "fs_hvb_image_patch.h"
#include "fs_manager.h"
#include "hookmgr.h"
#include "securec.h"

static HvbMockFailure g_failure;
static unsigned int g_remaining;
static unsigned int g_hits;
static unsigned int g_opens;
static unsigned int g_closes;

void HvbMockReset(void)
{
    g_failure = HVB_FAIL_NONE;
    g_remaining = 0;
    g_hits = 0;
    g_opens = 0;
    g_closes = 0;
}

void HvbMockFailAt(HvbMockFailure function, unsigned int call)
{
    g_failure = function;
    g_remaining = call;
    g_hits = 0;
}

unsigned int HvbMockFailureHits(void) { return g_hits; }
unsigned int HvbMockOpenCount(void) { return g_opens; }
unsigned int HvbMockCloseCount(void) { return g_closes; }

static int ShouldFail(HvbMockFailure function)
{
    if (g_failure != function || g_remaining == 0 || --g_remaining != 0) {
        return 0;
    }
    g_failure = HVB_FAIL_NONE;
    ++g_hits;
    errno = EIO;
    return 1;
}

void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size)
{
    return ShouldFail(HVB_FAIL_MALLOC) ? NULL : __real_malloc(size);
}

void *__real_calloc(size_t count, size_t size);
void *__wrap_calloc(size_t count, size_t size)
{
    return ShouldFail(HVB_FAIL_CALLOC) ? NULL : __real_calloc(count, size);
}

errno_t __real_memcpy_s(void *dest, size_t destMax, const void *src, size_t count);
errno_t __wrap_memcpy_s(void *dest, size_t destMax, const void *src, size_t count)
{
    return ShouldFail(HVB_FAIL_MEMCPY) ? EINVAL : __real_memcpy_s(dest, destMax, src, count);
}

int __wrap_sprintf_s(char *dest, size_t destMax, const char *format, ...)
{
    if (ShouldFail(HVB_FAIL_SPRINTF)) {
        return -1;
    }
    va_list args;
    va_start(args, format);
    int result = vsprintf_s(dest, destMax, format, args);
    va_end(args);
    return result;
}

int __wrap_snprintf_s(char *dest, size_t destMax, size_t count, const char *format, ...)
{
    if (ShouldFail(HVB_FAIL_SNPRINTF)) {
        return -1;
    }
    va_list args;
    va_start(args, format);
    int result = vsnprintf_s(dest, destMax, count, format, args);
    va_end(args);
    return result;
}

/* All partition I/O stays in ordinary files, never on a real block device. */
static const char *MapTestPath(const char *path, char *buffer, size_t size)
{
    const char *suffix = NULL;
    if (strncmp(path, "/dev/block/", strlen("/dev/block/")) == 0) {
        suffix = path + strlen("/dev/block/");
    } else if (strcmp(path, "/data/module_update/active/ModuleTrain/module.img") == 0) {
        suffix = "module.img";
    }
    if (suffix == NULL) {
        return path;
    }
    if (snprintf(buffer, size, "%s/libfs_hvb/%s", STARTUP_INIT_UT_PATH, suffix) < 0) {
        return path;
    }
    return buffer;
}

int __real_open(const char *path, int flags, ...);
int __wrap_open(const char *path, int flags, ...)
{
    if (ShouldFail(HVB_FAIL_OPEN)) {
        return -1;
    }
    mode_t mode = 0;
    if ((flags & O_CREAT) != 0) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    char buffer[PATH_MAX];
    int fd = __real_open(MapTestPath(path, buffer, sizeof(buffer)), flags, mode);
    if (fd >= 0) {
        ++g_opens;
    }
    return fd;
}

int __real_access(const char *path, int mode);
int __wrap_access(const char *path, int mode)
{
    char buffer[PATH_MAX];
    return __real_access(MapTestPath(path, buffer, sizeof(buffer)), mode);
}

ssize_t __real_read(int fd, void *buffer, size_t size);
ssize_t __wrap_read(int fd, void *buffer, size_t size)
{
    return ShouldFail(HVB_FAIL_READ) ? -1 : __real_read(fd, buffer, size);
}

off64_t __real_lseek64(int fd, off64_t offset, int whence);
off64_t __wrap_lseek64(int fd, off64_t offset, int whence)
{
    return ShouldFail(HVB_FAIL_LSEEK64) ? -1 : __real_lseek64(fd, offset, whence);
}

/* musl may redirect lseek64 to lseek even in a 32-bit build. */
off_t __real_lseek(int fd, off_t offset, int whence);
off_t __wrap_lseek(int fd, off_t offset, int whence)
{
    return ShouldFail(HVB_FAIL_LSEEK64) ? -1 : __real_lseek(fd, offset, whence);
}

int __real_close(int fd);
int __wrap_close(int fd)
{
    int result = __real_close(fd);
    if (result == 0) {
        ++g_closes;
    }
    return result;
}

char *__real_basename(char *path);
char *__wrap_basename(char *path)
{
    return ShouldFail(HVB_FAIL_BASENAME) ? NULL : __real_basename(path);
}

HOOK_MGR *__wrap_GetBootStageHookMgr(void)
{
    return NULL;
}

int __wrap_HookMgrExecute(HOOK_MGR *mgr, int stage, void *cookie, const HOOK_EXEC_OPTIONS *options)
{
    (void)mgr;
    (void)options;
    if (stage != INIT_VAB_HVBCHECK || cookie == NULL) {
        return -1;
    }
    HvbDeviceParam *param = (HvbDeviceParam *)cookie;
    const char *value = getenv(param->reverse ? "HVB_SNAPSHOT_REVERSE" : "HVB_SNAPSHOT_FORWARD");
    return value == NULL ? -1 : strcpy_s(param->value, sizeof(param->value), value);
}

int __real_QuickfixIsImagePatch(const char *name, int mode);
int __wrap_QuickfixIsImagePatch(const char *name, int mode)
{
    const char *patch = getenv("HVB_QUICKFIX");
    if (patch != NULL && name != NULL && strcmp(name, patch) == 0 && mode == QUICKFIX_COMPARE_SNAPSHOT_NAME) {
        return 0;
    }
    return __real_QuickfixIsImagePatch(name, mode);
}

int __real_QuickfixFsHvbGetImageCert(struct hvb_cert *cert, char *name);
int __wrap_QuickfixFsHvbGetImageCert(struct hvb_cert *cert, char *name)
{
    const char *value = getenv("HVB_QUICKFIX_CERT");
    if (value == NULL) {
        return __real_QuickfixFsHvbGetImageCert(cert, name);
    }
    return strcmp(value, "Succeed") == 0 ? hvb_cert_parser(cert, NULL) : -1;
}
