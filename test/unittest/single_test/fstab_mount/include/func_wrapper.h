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
#ifndef TEST_WRAPPER_H
#define TEST_WRAPPER_H
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

// for wrapper strdup;
char* __real_strdup(const char* string);
typedef char* (*StrdupFunc)(const char* string);
void UpdateStrdupFunc(StrdupFunc func);

// for wrapper malloc;
void* __real_malloc(size_t size);
typedef void* (*MallocFunc)(size_t size);
void UpdateMallocFunc(MallocFunc func);

// for wrapper strncat_s;
int __real_strncat_s(char *strDest, size_t destMax, const char *strSrc, size_t count);
typedef int (*StrncatSFunc)(char *strDest, size_t destMax, const char *strSrc, size_t count);
void UpdateStrncatSFunc(StrncatSFunc func);

// for wrapper mkdir;
int __real_mkdir(const char *path, mode_t mode);
typedef int (*MkdirFunc)(const char *path, mode_t mode);
void UpdateMkdirFunc(MkdirFunc func);

// for wrapper mount;
int __real_mount(const char *source, const char *target, const char *fsType, unsigned long flags, const void *data);
typedef int (*MountFunc)(const char *source, const char *target,
    const char *fsType, unsigned long flags, const void *data);
void UpdateMountFunc(MountFunc func);

// for wrapper stat;
int __real_stat(const char *pathname, struct stat *buf);
typedef int (*StatFunc)(const char *pathname, struct stat *buf);
void UpdateStatFunc(StatFunc func);

// for wrapper snprintf_s;
size_t __real_snprintf_s(char *strDest, size_t destMax, size_t count, const char *format, ...);
typedef size_t (*SnprintfSFunc)(char *strDest, size_t destMax, size_t count, const char *format, ...);
void UpdateSnprintfSFunc(SnprintfSFunc func);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif // TEST_WRAPPER_H