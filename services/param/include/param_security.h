/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef BASE_STARTUP_PARAM_SECURITY_H
#define BASE_STARTUP_PARAM_SECURITY_H
#include <stdint.h>
#ifndef __LINUX__
#include <sys/socket.h>
#endif
#include <sys/types.h>
#ifdef PARAM_SUPPORT_SELINUX
#include "selinux_parameter.h"
#endif

#include "beget_ext.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define DAC_GROUP_START 3
#define DAC_OTHER_START 6
#define DAC_READ 0x0100  // 4
#define DAC_WRITE 0x0080 // 2
#define DAC_WATCH 0x0040 // 1
#define DAC_ALL_PERMISSION 0777

#define LABEL_ALL_PERMISSION 0x04
#define LABEL_CHECK_IN_ALL_PROCESS 0x02
#define LABEL_INIT_FOR_INIT 0x01

#define SELINUX_CONTENT_LEN 64
#define SYS_UID_INDEX      1000

#define DAC_RESULT_PERMISSION 0

typedef struct UserCred {
    pid_t pid;
    uid_t uid;
    gid_t gid;
} UserCred;

typedef enum {
    PARAM_SECURITY_DAC = 0,
#ifdef PARAM_SUPPORT_SELINUX
    PARAM_SECURITY_SELINUX,
#endif
    PARAM_SECURITY_MAX
} ParamSecurityType;

typedef struct {
    int32_t sockFd;
    UserCred cred;
    uint32_t flags[PARAM_SECURITY_MAX];
} ParamSecurityLabel;

typedef struct {
    pid_t pid;
    uid_t uid;
    gid_t gid;
    uint16_t mode;
    uint8_t paramType;
} ParamDacData;

typedef struct {
    ParamDacData dacData;
    const char *name;
#ifdef PARAM_SUPPORT_SELINUX
    char label[SELINUX_CONTENT_LEN];
#endif
} ParamAuditData;

typedef struct {
    char name[10];
    int (*securityInitLabel)(ParamSecurityLabel *label, int isInit);
    int (*securityGetLabel)(const char *path);
    int (*securityCheckFilePermission)(const ParamSecurityLabel *label, const char *fileName, int flags);
    int (*securityCheckParamPermission)(const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode);
    int (*securityFreeLabel)(ParamSecurityLabel *srcLabel);
} ParamSecurityOps;

typedef int (*RegisterSecurityOpsPtr)(ParamSecurityOps *ops, int isInit);
typedef struct SelinuxSpace_ {
    void *selinuxHandle;
#ifdef PARAM_SUPPORT_SELINUX
    void (*setSelinuxLogCallback)(void);
    int (*setParamCheck)(const char *paraName, const char *destContext, const SrcInfo *info);
    const char *(*getParamLabel)(const char *paraName);
    int (*initParamSelinux)(void);
    int (*readParamCheck)(const char *paraName);
    ParamContextsList *(*getParamList)(void);
    void (*destroyParamList)(ParamContextsList **list);
#endif
} SelinuxSpace;
#ifdef PARAM_SUPPORT_SELINUX
INIT_LOCAL_API int RegisterSecuritySelinuxOps(ParamSecurityOps *ops, int isInit);
#endif

INIT_LOCAL_API ParamSecurityOps *GetParamSecurityOps(int type);
INIT_LOCAL_API void LoadGroupUser(void);
INIT_LOCAL_API int RegisterSecurityDacOps(ParamSecurityOps *ops, int isInit);
INIT_LOCAL_API int RegisterSecurityOps(int onlyRead);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif  // BASE_STARTUP_PARAM_SECURITY_H
