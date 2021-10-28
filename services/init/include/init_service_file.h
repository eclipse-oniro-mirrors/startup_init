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
#ifndef INIT_SERVICE_FILE_
#define INIT_SERVICE_FILE_
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define FILE_OPT_NUMS 5
#define SERVICE_FILE_NAME 0
#define SERVICE_FILE_FLAGS 1
#define SERVICE_FILE_PERM 2
#define SERVICE_FILE_UID 3
#define SERVICE_FILE_GID 4

typedef struct ServiceFile_ {
    struct ServiceFile_ *next;
    int flags;
    uid_t uid;     // uid
    gid_t gid;     // gid
    mode_t perm;   // Setting permissions
    int fd;
    char fileName[0];
} ServiceFile;

void CreateServiceFile(ServiceFile *fileOpt);
void CloseServiceFile(ServiceFile *fileOpt);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif
