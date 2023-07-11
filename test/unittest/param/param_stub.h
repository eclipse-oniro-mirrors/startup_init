/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#ifndef PARAM_TEST_STUB
#define PARAM_TEST_STUB
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifndef OHOS_LITE
#include "init_eng.h"
#include "init_context.h"
#endif
#include "param_manager.h"
#include "param_security.h"
#include "param_utils.h"
#include "securec.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#ifdef SUPPORT_64BIT
#define MODULE_LIB_NAME "/system/lib64/init"
#else
#define MODULE_LIB_NAME "/system/lib/init"
#endif

#define DEFAULT_ERROR (-65535)

#ifndef PARAM_SUPPORT_SELINUX
typedef struct ParameterNode {
    const char *paraName;
    const char *paraContext;
    int index;
} ParameterNode;

typedef struct ParamContextsList {
    struct ParameterNode info;
    struct ParamContextsList *next;
} ParamContextsList;

typedef struct SrcInfo {
    int sockFd;
    struct ucred uc;
} SrcInfo;
#endif

void CreateTestFile(const char *fileName, const char *data);
void PrepareInitUnitTestEnv(void);
void TestSetSelinuxOps(void);
void SetTestPermissionResult(int result);
void TestSetParamCheckResult(const char *prefix, uint16_t mode, int result);
int TestCheckParamPermission(const ParamLabelIndex *labelIndex,
    const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode);
int TestFreeLocalSecurityLabel(ParamSecurityLabel *srcLabel);

typedef enum {
    STUB_SPRINTF,
    STUB_MOUNT,
    STUB_MKNODE,
    STUB_MAX
} STUB_TYPE;
void SetStubResult(STUB_TYPE type, int result);
void PrepareCmdLineData();
ParamLabelIndex *TestGetParamLabelIndex(const char *name);

// for test
void HandleUevent(const struct Uevent *uevent);

#ifndef OHOS_LITE
void EngineerOverlay(void);
void DebugFilesOverlay(const char *source, const char *target);
void BindMountFile(const char *source, const char *target);
void MountEngPartitions(void);
void BuildMountCmd(char *buffer, size_t len, const char *mp, const char *dev, const char *fstype);
bool IsFileExistWithType(const char *file, FileType type);

int DoRoot_(const char *jobName, int type);
int DoRebootShutdown(int id, const char *name, int argc, const char **argv);
int DoRebootFlashed(int id, const char *name, int argc, const char **argv);
int DoRebootOther(int id, const char *name, int argc, const char **argv);

SubInitContext *GetSubInitContext(InitContextType type);
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif  // PARAM_TEST_STUB_
