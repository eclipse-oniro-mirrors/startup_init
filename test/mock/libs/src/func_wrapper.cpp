/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include <sys/ioctl.h>
#include "func_wrapper.h"
#include "securec.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

// start wrap strdup
static StrdupFunc g_strdup = NULL;
void UpdateStrdupFunc(StrdupFunc func)
{
    g_strdup = func;
}

char* __wrap_strdup(const char* string)
{
    if (g_strdup) {
        return g_strdup(string);
    } else {
        return __real_strdup(string);
    }
}

// start wrap malloc
static MallocFunc g_malloc = NULL;
void UpdateMallocFunc(MallocFunc func)
{
    g_malloc = func;
}

void* __wrap_malloc(size_t size)
{
    if (g_malloc) {
        return g_malloc(size);
    } else {
        return __real_malloc(size);
    }
}

// start wrap strncat_s
static StrncatSFunc g_strncat_s = NULL;
void UpdateStrncatSFunc(StrncatSFunc func)
{
    g_strncat_s = func;
}

int __wrap_strncat_s(char *strDest, size_t destMax, const char *strSrc, size_t count)
{
    if (g_strncat_s) {
        return g_strncat_s(strDest, destMax, strSrc, count);
    } else {
        return __real_strncat_s(strDest, destMax, strSrc, count);
    }
}

// start wrap mkdir
static MkdirFunc g_mkdir = NULL;
void UpdateMkdirFunc(MkdirFunc func)
{
    g_mkdir = func;
}

int __wrap_mkdir(const char *path, mode_t mode)
{
    if (g_mkdir) {
        return g_mkdir(path, mode);
    } else {
        return __real_mkdir(path, mode);
    }
}

// start wrap mount
static MountFunc g_mount = NULL;
void UpdateMountFunc(MountFunc func)
{
    g_mount = func;
}

int __wrap_mount(const char *source, const char *target,
    const char *fsType, unsigned long flags, const void *data)
{
    if (g_mount) {
        return g_mount(source, target, fsType, flags, data);
    } else {
        return __real_mount(source, target, fsType, flags, data);
    }
}

// start wrap stat
static StatFunc g_stat = NULL;
void UpdateStatFunc(StatFunc func)
{
    g_stat = func;
}

int __wrap_stat(const char *pathname, struct stat *buf)
{
    if (g_stat) {
        return g_stat(pathname, buf);
    } else {
        return __real_stat(pathname, buf);
    }
}

// start wrap snprintf_s
static SnprintfSFunc g_snprintf_s = NULL;
void UpdateSnprintfSFunc(SnprintfSFunc func)
{
    g_snprintf_s = func;
}

size_t __wrap_snprintf_s(char *strDest, size_t destMax, size_t count, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    size_t rc;
    if (g_snprintf_s) {
        rc = g_snprintf_s(strDest, destMax, count, format, args);
    } else {
        rc = vsnprintf_s(strDest, destMax, count, format, args);
    }
    va_end(args);
    return rc;
}

// start wrap open
static OpenFunc g_open = NULL;
void UpdateOpenFunc(OpenFunc func)
{
    g_open = func;
}

static int CallRealOpen(const char *pathname, int flag, va_list args)
{
    if ((flag & O_CREAT) != 0) {
        return RealOpen(pathname, flag, va_arg(args, mode_t));
    }
    return RealOpen(pathname, flag);
}

int WrapOpen(const char *pathname, int flag, ...) __asm__("__wrap_open");
int WrapOpen(const char *pathname, int flag, ...)
{
    va_list args;
    va_start(args, flag);
    int ret;
    if (g_open) {
        ret = g_open(pathname, flag);
    } else {
        ret = CallRealOpen(pathname, flag, args);
    }
    va_end(args);
    return ret;
}

// start wrap close
static CloseFunc g_close = NULL;
void UpdateCloseFunc(CloseFunc func)
{
    g_close = func;
}

int __wrap_close(int fd)
{
    if (g_close) {
        return g_close(fd);
    } else {
        return __real_close(fd);
    }
}

// start wrap strcpy_s
static StrcpySFunc g_strcpy_s = NULL;
void UpdateStrcpySFunc(StrcpySFunc func)
{
    g_strcpy_s = func;
}

int __wrap_strcpy_s(char *dest, size_t destMax, const char *src)
{
    if (g_strcpy_s) {
        return g_strcpy_s(dest, destMax, src);
    } else {
        return __real_strcpy_s(dest, destMax, src);
    }
}

// start wrap ioctl
static IoctlFunc g_ioctl = NULL;
void UpdateIoctlFunc(IoctlFunc func)
{
    g_ioctl = func;
}

int WrapIoctl(int fd, unsigned long req, ...) __asm__("__wrap_ioctl");
int WrapIoctl(int fd, unsigned long req, ...)
{
    va_list args;
    va_start(args, req);
    int rc;
    if (g_ioctl) {
        rc = g_ioctl(fd, static_cast<int>(req), args);
    } else if (_IOC_DIR(req) == _IOC_NONE && _IOC_SIZE(req) == 0) {
        rc = RealIoctl(fd, req);
    } else if (req == _IOWR('d', 0x09, int)) {
        rc = RealIoctl(fd, req, nullptr);
    } else {
        void *arg = va_arg(args, void *);
        rc = RealIoctl(fd, req, arg);
    }
    va_end(args);
    return rc;
}

// start wrap calloc
static CallocFunc g_calloc = NULL;
void UpdateCallocFunc(CallocFunc func)
{
    g_calloc = func;
}

void* __wrap_calloc(size_t m, size_t n)
{
    if (g_calloc) {
        return g_calloc(m, n);
    } else {
        return __real_calloc(m, n);
    }
}

// start wrap minor
static MinorFunc g_minor = NULL;
void UpdateMinorFunc(MinorFunc func)
{
    g_minor = func;
}

int __wrap_minor(dev_t dev)
{
    if (g_minor) {
        return g_minor(dev);
    } else {
        return __real_minor(dev);
    }
}

// start wrap memset_s
static MemsetSFunc g_memset_s = NULL;
void UpdateMemsetSFunc(MemsetSFunc func)
{
    g_memset_s = func;
}

int __wrap_memset_s(void *dest, size_t destMax, int c, size_t count)
{
    if (g_memset_s) {
        return g_memset_s(dest, destMax, c, count);
    } else {
        return __real_memset_s(dest, destMax, c, count);
    }
}

// start wrap memcpy_s
static MemcpySFunc g_memcpy_s = NULL;
void UpdateMemcpySFunc(MemcpySFunc func)
{
    g_memcpy_s = func;
}

int __wrap_memcpy_s(void *dest, size_t destMax, const void *src, size_t count)
{
    if (g_memcpy_s) {
        return g_memcpy_s(dest, destMax, src, count);
    } else {
        return __real_memcpy_s(dest, destMax, src, count);
    }
}

// start wrap read
static ReadFunc g_read = NULL;

void UpdateReadFunc(ReadFunc func)
{
    g_read = func;
}

ssize_t __wrap_read(int fd, void *buf, size_t count)
{
    if (g_read) {
        return g_read(fd, buf, count);
    } else {
        return __real_read(fd, buf, count);
    }
}

// start wrap access
static AccessFunc g_access = NULL;

void UpdateAccessFunc(AccessFunc func)
{
    g_access = func;
}

int __wrap_access(const char *pathname, int mode)
{
    if (g_access) {
        return g_access(pathname, mode);
    } else {
        return __real_access(pathname, mode);
    }
}

// start wrap NeedDoAllResize
static NeedDoAllResizeFunc g_needDoAllResize = NULL;
void UpdateNeedDoAllResizeFunc(NeedDoAllResizeFunc func)
{
    g_needDoAllResize = func;
}
 
int __wrap_NeedDoAllResize(const unsigned int fsManagerFlags)
{
    if (g_needDoAllResize) {
        return g_needDoAllResize(fsManagerFlags);
    } else {
        return __real_NeedDoAllResize(fsManagerFlags);
    }
}

// start wrap fork
static ForkFunc g_fork = NULL;
void UpdateForkFunc(ForkFunc func)
{
    g_fork = func;
}

pid_t __wrap_fork(void)
{
    if (g_fork) {
        return g_fork();
    } else {
        return __real_fork();
    }
}

// start wrap getenv
static GetenvFunc g_getenv = NULL;
void UpdateGetenvFunc(GetenvFunc func)
{
    g_getenv = func;
}

char *__wrap_getenv(const char *name)
{
    if (g_getenv) {
        return g_getenv(name);
    } else {
        return __real_getenv(name);
    }
}

// start wrap memfd_create
static MemfdCreateFunc g_memfd_create = NULL;
void UpdateMemfdCreateFunc(MemfdCreateFunc func)
{
    g_memfd_create = func;
}

int __wrap_memfd_create(const char *name, unsigned flags)
{
    if (g_memfd_create) {
        return g_memfd_create(name, flags);
    } else {
        return __real_memfd_create(name, flags);
    }
}

// start wrap fcntl
static FcntlFunc g_fcntl = NULL;
static int g_fcntlFlagFilter = -1;
void UpdateFcntlFunc(FcntlFunc func, int flagFilter)
{
    g_fcntl = func;
    g_fcntlFlagFilter = flagFilter;
}

int __wrap_fcntl(int fd, int flag, unsigned long arg)
{
    int result;
    if (!g_fcntl || flag == g_fcntlFlagFilter) {
        result = __real_fcntl(fd, flag, arg);
    } else {
        result = g_fcntl(fd, flag, arg);
    }
    return result;
}

// start wrap waitpid
static WaitpidFunc g_waitpid = NULL;
void UpdateWaitpidFunc(WaitpidFunc func)
{
    g_waitpid = func;
}

pid_t __wrap_waitpid(pid_t pid, int *status, int options)
{
    if (g_waitpid) {
        return g_waitpid(pid, status, options);
    } else {
        return __real_waitpid(pid, status, options);
    }
}

// start wrap pread
static PreadFunc g_pread = NULL;
void UpdatePreadFunc(PreadFunc func)
{
    g_pread = func;
}

ssize_t __wrap_pread(int fd, void* const buf, size_t count, off_t offset)
{
    if (g_pread) {
        return g_pread(fd, buf, count, offset);
    } else {
        return __real_pread(fd, buf, count, offset);
    }
}

// start wrap getuid
static GetuidFunc g_getuid = NULL;
void UpdateGetuidFunc(GetuidFunc func)
{
    g_getuid = func;
}

uid_t __wrap_getuid(void)
{
    if (g_getuid) {
        return g_getuid();
    } else {
        return __real_getuid();
    }
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
