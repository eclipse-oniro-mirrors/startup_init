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
#include "func_wrapper.h"

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
        rc = __real_snprintf_s(strDest, destMax, count, format, args);
    }
    va_end(args);
    return rc;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
