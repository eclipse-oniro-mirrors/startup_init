/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>

#include "init_utils.h"
#include "param_manager.h"
#include "param_security.h"
#include "param_trie.h"
#include "param_utils.h"
#include "param_base.h"

#define USER_BUFFER_LEN 64
#define MAX_BUF_SIZE  1024
#define GROUP_FORMAT "const.group"

#define OCT_BASE 8
static void GetUserIdByName(uid_t *uid, const char *name, uint32_t nameLen)
{
    *uid = -1;
    struct passwd *data = NULL;
    while ((data = getpwent()) != NULL) {
        if ((data->pw_name != NULL) && (strlen(data->pw_name) == nameLen) &&
            (strncmp(data->pw_name, name, nameLen) == 0)) {
            *uid = data->pw_uid;
            break;
        }
    }
    endpwent();
}

static void GetGroupIdByName(gid_t *gid, const char *name, uint32_t nameLen)
{
    *gid = -1;
    struct group *data = NULL;
    while ((data = getgrent()) != NULL) {
        if ((data->gr_name != NULL) && (strlen(data->gr_name) == nameLen) &&
            (strncmp(data->gr_name, name, nameLen) == 0)) {
            *gid = data->gr_gid;
            break;
        }
    }
    endgrent();
}

// user:group:r|w
static int GetParamDacData(ParamDacData *dacData, const char *value)
{
    if (dacData == NULL) {
        return -1;
    }
    char *groupName = strstr(value, ":");
    if (groupName == NULL) {
        return -1;
    }
    char *mode = strstr(groupName + 1, ":");
    if (mode == NULL) {
        return -1;
    }
    GetUserIdByName(&dacData->uid, value, groupName - value);
    GetGroupIdByName(&dacData->gid, groupName + 1, mode - groupName - 1);
    dacData->mode = strtol(mode + 1, NULL, OCT_BASE);
    return 0;
}

static int InitLocalSecurityLabel(ParamSecurityLabel *security, int isInit)
{
    UNUSED(isInit);
    PARAM_CHECK(security != NULL, return -1, "Invalid security");
    security->cred.pid = getpid();
    security->cred.uid = geteuid();
    security->cred.gid = getegid();
    // support check write permission in client
    security->flags[PARAM_SECURITY_DAC] |= LABEL_CHECK_IN_ALL_PROCESS;
    return 0;
}

static int FreeLocalSecurityLabel(ParamSecurityLabel *srcLabel)
{
    return 0;
}

static int LoadOneParam_(const uint32_t *context, const char *name, const char *value)
{
    ParamAuditData auditData = {0};
    auditData.name = name;
    int ret = GetParamDacData(&auditData.dacData, value);
    PARAM_CHECK(ret == 0, return -1, "Failed to get param info %d %s", ret, name);
    AddSecurityLabel(&auditData);
    return 0;
}

static int LoadParamLabels(const char *fileName)
{
    uint32_t infoCount = 0;
    FILE *fp = fopen(fileName, "r");
    const uint32_t buffSize = PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX + 10;  // 10 size
    char *buff = (char *)calloc(1, buffSize);
    while (fp != NULL && buff != NULL && fgets(buff, buffSize, fp) != NULL) {
        buff[buffSize - 1] = '\0';
        int ret = SplitParamString(buff, NULL, 0, LoadOneParam_, NULL);
        if (ret != 0) {
            PARAM_LOGE("Failed to split string %s fileName %s", buff, fileName);
            continue;
        }
        infoCount++;
    }
    PARAM_LOGI("Load parameter label total %u success %s", infoCount, fileName);
    if (fp != NULL) {
        (void)fclose(fp);
    }
    if (buff != NULL) {
        free(buff);
    }
    return 0;
}

static int ProcessParamFile(const char *fileName, void *context)
{
    UNUSED(context);
    return LoadParamLabels(fileName);
}

static int DacGetParamSecurityLabel(const char *path)
{
    PARAM_CHECK(path != NULL, return -1, "Invalid param");
    struct stat st;
    if ((stat(path, &st) == 0) && !S_ISDIR(st.st_mode)) {
        return ProcessParamFile(path, NULL);
    }

    PARAM_LOGV("DacGetParamSecurityLabel %s ", path);
    DIR *pDir = opendir(path);
    PARAM_CHECK(pDir != NULL, return -1, "Read dir :%s failed.%d", path, errno);
    char *fileName = malloc(MAX_BUF_SIZE);
    PARAM_CHECK(fileName != NULL, closedir(pDir);
        return -1, "Failed to malloc for %s", path);

    struct dirent *dp;
    uint32_t count = 0;
    while ((dp = readdir(pDir)) != NULL) {
        if (dp->d_type == DT_DIR) {
            continue;
        }
        char *tmp = strstr(dp->d_name, ".para.dac");
        if (tmp == NULL) {
            continue;
        }
        if (strcmp(tmp, ".para.dac") != 0) {
            continue;
        }
        int ret = ParamSprintf(fileName, MAX_BUF_SIZE, "%s/%s", path, dp->d_name);
        if (ret <= 0) {
            PARAM_LOGE("Failed to get file name for %s", dp->d_name);
            continue;
        }
        if ((stat(fileName, &st) == 0) && !S_ISDIR(st.st_mode)) {
            count++;
            ProcessParamFile(fileName, NULL);
        }
    }
    PARAM_LOGI("DacGetParamSecurityLabel path %s %d", path, count);
    free(fileName);
    closedir(pDir);
    return 0;
}

static int CheckFilePermission(const ParamSecurityLabel *localLabel, const char *fileName, int flags)
{
    UNUSED(flags);
    PARAM_CHECK(localLabel != NULL && fileName != NULL, return -1, "Invalid param");
    return 0;
}

static int CheckUserInGroup(WorkSpace *space, gid_t groupId, uid_t uid)
{
#ifdef __MUSL__
    static char buffer[USER_BUFFER_LEN] = {0};
    uint32_t labelIndex = 0;
    int ret = ParamSprintf(buffer, sizeof(buffer), "%s.%d.%d", GROUP_FORMAT, groupId, uid);
    PARAM_CHECK(ret >= 0, return -1, "Failed to format name for %s.%d.%d", GROUP_FORMAT, groupId, uid);
    (void)FindTrieNode(space, buffer, strlen(buffer), &labelIndex);
    ParamSecurityNode *node = (ParamSecurityNode *)GetTrieNode(space, labelIndex);
    PARAM_CHECK(node != NULL, return DAC_RESULT_FORBIDED, "Can not get security label %d", labelIndex);
    PARAM_LOGV("CheckUserInGroup %s groupid %d uid %d", buffer, groupId, uid);
    if (node->gid == groupId && node->uid == uid) {
        return 0;
    }
    return -1;
#else
    return 0;
#endif
}

static int DacCheckParamPermission(const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
    int ret = DAC_RESULT_FORBIDED;
    uint32_t labelIndex = 0;
    // get dac label
    WorkSpace *space = GetWorkSpace(WORKSPACE_NAME_DAC);
    PARAM_CHECK(space != NULL, return DAC_RESULT_FORBIDED, "Failed to get dac space %s", name);
    (void)FindTrieNode(space, name, strlen(name), &labelIndex);
    ParamSecurityNode *node = (ParamSecurityNode *)GetTrieNode(space, labelIndex);
    PARAM_CHECK(node != NULL, return DAC_RESULT_FORBIDED, "Can not get security label %d", labelIndex);
    /**
     * DAC group
     * user:group:read|write|watch
     */
    uint32_t localMode;
    if (srcLabel->cred.uid == node->uid) {
        localMode = mode & (DAC_READ | DAC_WRITE | DAC_WATCH);
    } else if (srcLabel->cred.gid == node->gid) {
        localMode = (mode & (DAC_READ | DAC_WRITE | DAC_WATCH)) >> DAC_GROUP_START;
    } else if (CheckUserInGroup(space, node->gid, srcLabel->cred.uid) == 0) {  // user in group
        localMode = (mode & (DAC_READ | DAC_WRITE | DAC_WATCH)) >> DAC_GROUP_START;
    } else {
        localMode = (mode & (DAC_READ | DAC_WRITE | DAC_WATCH)) >> DAC_OTHER_START;
    }
    if ((node->mode & localMode) != 0) {
        ret = DAC_RESULT_PERMISSION;
    }
    if (ret != DAC_RESULT_PERMISSION) {
        PARAM_LOGW("Param '%s' label gid:%d uid:%d mode 0%o", name, srcLabel->cred.gid, srcLabel->cred.uid, localMode);
        PARAM_LOGW("Cfg label %d gid:%d uid:%d mode 0%o ", labelIndex, node->gid, node->uid, node->mode);
#ifndef STARTUP_INIT_TEST
        ret = DAC_RESULT_PERMISSION;
#endif
    }
    return ret;
}

INIT_LOCAL_API int RegisterSecurityDacOps(ParamSecurityOps *ops, int isInit)
{
    PARAM_CHECK(ops != NULL, return -1, "Invalid param");
    PARAM_LOGV("RegisterSecurityDacOps %d", isInit);
    int ret = ParamStrCpy(ops->name, sizeof(ops->name), "dac");
    ops->securityInitLabel = InitLocalSecurityLabel;
    ops->securityCheckFilePermission = CheckFilePermission;
    ops->securityCheckParamPermission = DacCheckParamPermission;
    ops->securityFreeLabel = FreeLocalSecurityLabel;
    if (isInit) {
        ops->securityGetLabel = DacGetParamSecurityLabel;
    }
    return ret;
}

static void AddGroupUser(unsigned int uid, unsigned int gid, int mode, const char *format)
{
    ParamAuditData auditData = {0};
    char buffer[USER_BUFFER_LEN] = {0};
    int ret = ParamSprintf(buffer, sizeof(buffer), "%s.%u.%u", format, gid, uid);
    PARAM_CHECK(ret >= 0, return, "Failed to format name for %s.%d.%d", format, gid, uid);
    auditData.name = buffer;
    auditData.dacData.uid = uid;
    auditData.dacData.gid = gid;
    auditData.dacData.mode = mode;
    AddSecurityLabel(&auditData);
}

INIT_LOCAL_API void LoadGroupUser(void)
{
#ifndef __MUSL__
    return;
#endif
    PARAM_LOGV("LoadGroupUser ");
    uid_t uid = 0;
    struct group *data = NULL;
    while ((data = getgrent()) != NULL) {
        if (data->gr_name == NULL || data->gr_mem == NULL) {
            continue;
        }
        if (data->gr_mem[0] == NULL) { // default user in group
            GetUserIdByName(&uid, data->gr_name, strlen(data->gr_name));
            PARAM_LOGV("LoadGroupUser %s gid %d uid %d", data->gr_name, data->gr_gid, uid);
            AddGroupUser(uid, data->gr_gid, 0550, GROUP_FORMAT); // 0550 read and watch
            continue;
        }
        int index = 0;
        while (data->gr_mem[index]) { // user in this group
            GetUserIdByName(&uid, data->gr_mem[index], strlen(data->gr_mem[index]));
            PARAM_LOGV("LoadGroupUser %s gid %d uid %d user %s", data->gr_name, data->gr_gid, uid, data->gr_mem[index]);
            AddGroupUser(uid, data->gr_gid, 0550, GROUP_FORMAT); // 0550 read and watch
            index++;
        }
    }
    PARAM_LOGV("LoadGroupUser getgrent fail errnor %d ", errno);
    endgrent();
}
