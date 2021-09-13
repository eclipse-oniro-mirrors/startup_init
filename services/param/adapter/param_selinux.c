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

#include <sys/stat.h>

#include "init_utils.h"
#include "param_security.h"
#include "param_utils.h"

#define LABEL "PARAM_SELINUX"
#define SELINUX_LABEL_LEN 128

typedef struct SELinuxSecurityLabel {
    ParamSecurityLabel securityLabel;
    char label[SELINUX_LABEL_LEN];
} SELinuxSecurityLabel;

static SELinuxSecurityLabel g_localSecurityLabel = {};

static int InitLocalSecurityLabel(ParamSecurityLabel **security, int isInit)
{
    UNUSED(isInit);
    PARAM_LOGI("TestDacGetLabel uid:%d gid:%d euid: %d egid: %d ", getuid(), getgid(), geteuid(), getegid());
    g_localSecurityLabel.securityLabel.cred.pid = getpid();
    g_localSecurityLabel.securityLabel.cred.uid = geteuid();
    g_localSecurityLabel.securityLabel.cred.gid = getegid();
    *security = &g_localSecurityLabel.securityLabel;
    return 0;
}

static int FreeLocalSecurityLabel(ParamSecurityLabel *srcLabel)
{
    return 0;
}

static int EncodeSecurityLabel(const ParamSecurityLabel *srcLabel, char *buffer, uint32_t *bufferSize)
{
    PARAM_CHECK(bufferSize != NULL, return -1, "Invalid param");
    if (buffer == NULL) {
        *bufferSize = sizeof(SELinuxSecurityLabel);
        return 0;
    }
    PARAM_CHECK(*bufferSize >= sizeof(SELinuxSecurityLabel), return -1, "Invalid buffersize %u", *bufferSize);
    *bufferSize = sizeof(SELinuxSecurityLabel);
    return memcpy_s(buffer, *bufferSize, srcLabel, sizeof(SELinuxSecurityLabel));
}

static int DecodeSecurityLabel(ParamSecurityLabel **srcLabel, char *buffer, uint32_t bufferSize)
{
    PARAM_CHECK(bufferSize >= sizeof(SELinuxSecurityLabel), return -1, "Invalid buffersize %u", bufferSize);
    PARAM_CHECK(srcLabel != NULL && buffer != NULL, return -1, "Invalid param");
    *srcLabel = &((SELinuxSecurityLabel *)buffer)->securityLabel;
    return 0;
}

static int LoadParamLabels(const char *fileName, SecurityLabelFunc label, void *context)
{
    int ret = 0;
    FILE *fp = fopen(fileName, "r");
    PARAM_CHECK(fp != NULL, return -1, "Open file %s fail", fileName);
    SubStringInfo *info = malloc(sizeof(SubStringInfo) * SUBSTR_INFO_DAC + 1);
    char buff[PARAM_BUFFER_SIZE];
    int infoCount = 0;
    ParamAuditData auditData = {};
    while (fgets(buff, PARAM_BUFFER_SIZE, fp) != NULL) {
        int subStrNumber = GetSubStringInfo(buff, strlen(buff), ' ', info, SUBSTR_INFO_DAC + 1);
        if (subStrNumber <= SUBSTR_INFO_DAC) {
            continue;
        }
        auditData.name = info[SUBSTR_INFO_NAME].value;
        auditData.label = info[SUBSTR_INFO_LABEL].value;
        ret = label(&auditData, context);
        PARAM_CHECK(ret == 0, continue, "Failed to write param info %d %s", ret, buff);
        infoCount++;
    }
    (void)fclose(fp);
    free(info);
    PARAM_LOGI("Load parameter info %d success %s", infoCount, fileName);
    return 0;
}

static int ProcessParamFile(const char *fileName, void *context)
{
    LabelFuncContext *cxt = (LabelFuncContext *)context;
    return LoadParamLabels(fileName, cxt->label, cxt->context);
}

static int GetParamSecurityLabel(SecurityLabelFunc label, const char *path, void *context)
{
    PARAM_CHECK(label != NULL, return -1, "Invalid param");
    int ret = 0;
    struct stat st;
    LabelFuncContext cxt = { label, context };
    if ((stat(path, &st) == 0) && !S_ISDIR(st.st_mode)) {
        ret = ProcessParamFile(path, &cxt);
    } else {
        ret = ReadFileInDir(path, ".para.selinux", ProcessParamFile, &cxt);
    }
    return ret;
}

static int CheckFilePermission(const ParamSecurityLabel *localLabel, const char *fileName, int flags)
{
    UNUSED(flags);
    PARAM_CHECK(localLabel != NULL && fileName != NULL, return -1, "Invalid param");
    return 0;
}

static int CheckParamPermission(const ParamSecurityLabel *srcLabel, const ParamAuditData *auditData, int mode)
{
    PARAM_LOGI("CheckParamPermission ");
    PARAM_CHECK(srcLabel != NULL && auditData != NULL && auditData->name != NULL, return -1, "Invalid param");
    return 0;
}

PARAM_STATIC int RegisterSecuritySelinuxOps(ParamSecurityOps *ops, int isInit)
{
    PARAM_CHECK(ops != NULL, return -1, "Invalid param");
    ops->securityGetLabel = NULL;
    ops->securityDecodeLabel = NULL;
    ops->securityEncodeLabel = NULL;
    ops->securityInitLabel = InitLocalSecurityLabel;
    ops->securityCheckFilePermission = CheckFilePermission;
    ops->securityCheckParamPermission = CheckParamPermission;
    ops->securityFreeLabel = FreeLocalSecurityLabel;
    if (isInit) {
        ops->securityGetLabel = GetParamSecurityLabel;
        ops->securityDecodeLabel = DecodeSecurityLabel;
    } else {
        ops->securityEncodeLabel = EncodeSecurityLabel;
    }
    return 0;
}
#ifdef PARAM_SUPPORT_SELINUX
int RegisterSecurityOps(ParamSecurityOps *ops, int isInit)
{
    return RegisterSecuritySelinuxOps(ops, isInit);
}
#endif