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
#ifndef MKDIR_TEST_WRAPPER_H
#define MKDIR_TEST_WRAPPER_H

#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#ifdef __cplusplus
extern "C" {
#endif

int RealLstat(const char *pathname, struct stat *buf);
int RealFstatat(int dirfd, const char *pathname, struct stat *buf, int flags);

typedef int (*LstatFunc)(const char *pathname, struct stat *buf);
void UpdateLstatFunc(LstatFunc func);

typedef int (*FchmodatFunc)(int dirfd, const char *pathname, mode_t mode, int flags);
void UpdateFchmodatFunc(FchmodatFunc func);

typedef int (*LchownFunc)(const char *pathname, uid_t owner, gid_t group);
void UpdateLchownFunc(LchownFunc func);

typedef int (*FstatatFunc)(int dirfd, const char *pathname, struct stat *buf, int flags);
void UpdateFstatatFunc(FstatatFunc func);

typedef void (*PluginExecCmdFunc)(const char *name, const char *cmdContent);
void UpdatePluginExecCmdFunc(PluginExecCmdFunc func);

typedef int (*SetFileCryptPolicyFunc)(const char *dir);
void UpdateSetFileCryptPolicyFunc(SetFileCryptPolicyFunc func);

typedef int (*ChownFunc)(const char *pathname, uid_t owner, gid_t group);
void UpdateChownFunc(ChownFunc func);

typedef struct passwd *(*GetPwNamFunc)(const char *name);
void UpdateGetPwNamFunc(GetPwNamFunc func);

typedef struct group *(*GetGrNamFunc)(const char *name);
void UpdateGetGrNamFunc(GetGrNamFunc func);

typedef struct group *(*GetGRentFunc)(void);
void UpdateGetGRentFunc(GetGRentFunc func);

void ResetMkdirTestWrappers(void);

#ifdef __cplusplus
}
#endif
#endif
