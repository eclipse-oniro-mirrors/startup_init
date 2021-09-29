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

#ifndef BASE_STARTUP_PARAM_SECURITY_H
#define BASE_STARTUP_PARAM_SECURITY_H
#include <stdint.h>
#include <sys/types.h>
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define DAC_GROUP_START 3
#define DAC_OTHER_START 6
#define DAC_READ 0x0100
#define DAC_WRITE 0x0080
#define DAC_WATCH 0x0040
#define DAC_ALL_PERMISSION 0777

#define LABEL_ALL_PERMISSION 0x04
#define LABEL_CHECK_FOR_ALL_PROCESS 0x02
#define LABEL_INIT_FOR_INIT 0x01

#define LABEL_IS_CLIENT_CHECK_PERMITTED(label)                                                                   \
    ((label) != NULL) && ((((label)->flags & (LABEL_CHECK_FOR_ALL_PROCESS)) == (LABEL_CHECK_FOR_ALL_PROCESS)) && \
                          (((label)->flags & (LABEL_ALL_PERMISSION)) != (LABEL_ALL_PERMISSION)))

#define LABEL_IS_ALL_PERMITTED(label) \
    (((label) == NULL) || ((label)->flags & LABEL_ALL_PERMISSION) == (LABEL_ALL_PERMISSION))

typedef enum {
    DAC_RESULT_PERMISSION = 0,
    DAC_RESULT_INVALID_PARAM = 1000,
    DAC_RESULT_FORBIDED,
} DAC_RESULT;

typedef struct UserCred {
    pid_t pid;
    uid_t uid;
    gid_t gid;
} UserCred;

typedef struct {
    uint32_t flags;
    UserCred cred;
} ParamSecurityLabel;

typedef struct {
    pid_t pid;
    uid_t uid;
    gid_t gid;
    uint32_t mode; // 访问权限
} ParamDacData;

typedef struct {
    ParamDacData dacData;
    const char *name;
    const char *label;
} ParamAuditData;

typedef int (*SecurityLabelFunc)(const ParamAuditData *auditData, void *context);

typedef struct {
    int (*securityInitLabel)(ParamSecurityLabel **label, int isInit);
    int (*securityGetLabel)(SecurityLabelFunc label, const char *path, void *context);
    int (*securityCheckFilePermission)(const ParamSecurityLabel *label, const char *fileName, int flags);
    int (*securityCheckParamPermission)(const ParamSecurityLabel *srcLabel, const ParamAuditData *auditData, uint32_t mode);
    int (*securityEncodeLabel)(const ParamSecurityLabel *srcLabel, char *buffer, uint32_t *bufferSize);
    int (*securityDecodeLabel)(ParamSecurityLabel **srcLabel, const char *buffer, uint32_t bufferSize);
    int (*securityFreeLabel)(ParamSecurityLabel *srcLabel);
} ParamSecurityOps;

typedef int (*RegisterSecurityOpsPtr)(ParamSecurityOps *ops, int isInit);

int RegisterSecurityOps(ParamSecurityOps *ops, int isInit);

typedef struct {
    SecurityLabelFunc label;
    void *context;
} LabelFuncContext;


#ifdef PARAM_SUPPORT_SELINUX
#ifdef PARAM_SUPPORT_DAC
#error param security only support one.
#endif
#else
#define PARAM_SUPPORT_DAC 1 // default support dac
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif // BASE_STARTUP_PARAM_SECURITY_H