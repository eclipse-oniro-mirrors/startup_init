/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include "param_osadp.h"
#include "param_security.h"
#include "securec.h"

static int InitLocalSecurityLabel(ParamSecurityLabel *security, int isInit)
{
    UNUSED(isInit);
    PARAM_CHECK(security != NULL, return -1, "Invalid security");
#if defined __LITEOS_A__
    security->cred.pid = getpid();
    security->cred.uid = getuid();
    security->cred.gid = 0;
#else
    security->cred.pid = 0;
    security->cred.uid = 0;
    security->cred.gid = 0;
#endif
    security->flags[PARAM_SECURITY_DAC] |= LABEL_CHECK_IN_ALL_PROCESS;
    return 0;
}

static int FreeLocalSecurityLabel(ParamSecurityLabel *srcLabel)
{
    (void)srcLabel;
    return 0;
}

static int DacGetParamSecurityLabel(const char *path)
{
    UNUSED(path);
    return 0;
}

static int CheckFilePermission(const ParamSecurityLabel *localLabel, const char *fileName, int flags)
{
    UNUSED(flags);
    PARAM_CHECK(localLabel != NULL && fileName != NULL, return -1, "Invalid param");
    return 0;
}

static int DacCheckParamPermission(const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
    UNUSED(srcLabel);
    UNUSED(name);
    UNUSED(mode);
#if defined(__LITEOS_A__)
    uid_t uid = getuid();
    return uid <= SYS_UID_INDEX ? DAC_RESULT_PERMISSION : DAC_RESULT_FORBIDED;
#endif
    return DAC_RESULT_PERMISSION;
}

INIT_LOCAL_API int RegisterSecurityDacOps(ParamSecurityOps *ops, int isInit)
{
    PARAM_CHECK(ops != NULL, return -1, "Invalid param");
    PARAM_LOGV("RegisterSecurityDacOps %d", isInit);
    int ret = strcpy_s(ops->name, sizeof(ops->name), "dac");
    ops->securityGetLabel = NULL;
    ops->securityInitLabel = InitLocalSecurityLabel;
    ops->securityCheckFilePermission = CheckFilePermission;
    ops->securityCheckParamPermission = DacCheckParamPermission;
    ops->securityFreeLabel = FreeLocalSecurityLabel;
    if (isInit) {
        ops->securityGetLabel = DacGetParamSecurityLabel;
    }
    return ret;
}

INIT_LOCAL_API void LoadGroupUser(void)
{
}
