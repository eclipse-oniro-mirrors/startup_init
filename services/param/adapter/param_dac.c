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
#include <dirent.h>
#include <string.h>

#include "param_manager.h"
#include "param_security.h"
#include "param_trie.h"
#include "param_utils.h"
#include "param_base.h"

#define USER_BUFFER_LEN 64
#define MAX_BUF_SIZE  1024
#define GROUP_FORMAT "const.group"
#define INVALID_MODE 0550
#define GROUP_FILE_PATH "/etc/group"
#define OCT_BASE 8
#define INVALID_UID(uid) ((uid) == (uid_t)-1)

static void GetUserIdByName(uid_t *uid, const char *name)
{
    struct passwd *data = getpwnam(name);
    if (data == NULL) {
        *uid = -1;
        return ;
    }
    *uid = data->pw_uid;
}

static void GetGroupIdByName(gid_t *gid, const char *name)
{
    *gid = -1;
    struct group *data = getgrnam(name);
    if (data != NULL) {
        *gid = data->gr_gid;
        return;
    }
    while ((data = getgrent()) != NULL) {
        if ((data->gr_name != NULL) && (strcmp(data->gr_name, name) == 0)) {
            *gid = data->gr_gid;
            break;
        }
    }
    endgrent();
}

// user:group:r|w
static int GetParamDacData(ParamDacData *dacData, const char *value)
{
    static const struct {
        const char *name;
        int value;
    } paramTypes[] = {
        { "int", PARAM_TYPE_INT },
        { "string", PARAM_TYPE_STRING },
        { "bool", PARAM_TYPE_BOOL },
    };

    char *groupName = strstr(value, ":");
    if (groupName == NULL) {
        return -1;
    }
    char *mode = strstr(groupName + 1, ":");
    if (mode == NULL) {
        return -1;
    }

    uint32_t nameLen = groupName - value;
    char *name = (char *)value;
    name[nameLen] = '\0';
    GetUserIdByName(&dacData->uid, name);
    nameLen = mode - groupName - 1;
    name = (char *)groupName + 1;
    name[nameLen] = '\0';
    GetGroupIdByName(&dacData->gid, name);

    dacData->paramType = PARAM_TYPE_STRING;
    char *type = strstr(mode + 1, ":");
    if (type != NULL) {
        *type = '\0';
        type++;
        for (size_t i = 0; (type != NULL) && (i < ARRAY_LENGTH(paramTypes)); i++) {
            if (strcmp(paramTypes[i].name, type) == 0) {
                dacData->paramType = paramTypes[i].value;
            }
        }
    }
    dacData->mode = (uint16_t)strtol(mode + 1, NULL, OCT_BASE);
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
    auditData.dacData.gid = -1;
    auditData.dacData.uid = -1;
    auditData.name = name;
    int ret = GetParamDacData(&auditData.dacData, value);
    PARAM_CHECK(ret == 0, return -1, "Failed to get param info %d %s", ret, name);
    if (INVALID_UID(auditData.dacData.gid) || INVALID_UID(auditData.dacData.uid)) {
        PARAM_LOGW("Invalid dac for '%s' gid %d uid %d", name, auditData.dacData.gid, auditData.dacData.uid);
    }
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
    PARAM_LOGV("Get parameter security label dac number is %d, from %s.", count, path);
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
    char buffer[USER_BUFFER_LEN] = {0};
    uint32_t labelIndex = 0;
    int ret = ParamSprintf(buffer, sizeof(buffer), "%s.%d.%d", GROUP_FORMAT, groupId, uid);
    PARAM_CHECK(ret >= 0, return -1, "Failed to format name for %s.%d.%d", GROUP_FORMAT, groupId, uid);
    (void)FindTrieNode(space, buffer, strlen(buffer), &labelIndex);
    ParamSecurityNode *node = (ParamSecurityNode *)GetTrieNode(space, labelIndex);
    PARAM_CHECK(node != NULL, return DAC_RESULT_FORBIDED, "Can not get security label %d", labelIndex);
    PARAM_LOGV("CheckUserInGroup %s groupid %d uid %d", buffer, node->gid, node->uid);
    if (node->gid == groupId && node->uid == uid) {
        return 0;
    }
    return -1;
}

static int DacCheckParamPermission(const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
#ifndef STARTUP_INIT_TEST
    if (srcLabel->cred.uid == 0) {
        return DAC_RESULT_PERMISSION;
    }
#endif
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
#ifndef __MUSL__
#ifndef STARTUP_INIT_TEST
        ret = DAC_RESULT_PERMISSION;
#endif
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

static void AddGroupUser(const char *userName, gid_t gid)
{
    if (userName == NULL || strlen(userName) == 0) {
        return;
    }
    uid_t uid = 0;
    GetUserIdByName(&uid, userName);
    PARAM_LOGV("Add group user '%s' gid %d uid %d", userName, gid, uid);
    if (INVALID_UID(gid) || INVALID_UID(uid)) {
        PARAM_LOGW("Invalid user for '%s' gid %d uid %d", userName, gid, uid);
        return;
    }
    ParamAuditData auditData = {0};
    char buffer[USER_BUFFER_LEN] = {0};
    int ret = ParamSprintf(buffer, sizeof(buffer), "%s.%u.%u", GROUP_FORMAT, gid, uid);
    PARAM_CHECK(ret >= 0, return, "Failed to format name for %d.%d", gid, uid);
    auditData.name = buffer;
    auditData.dacData.uid = uid;
    auditData.dacData.gid = gid;
    auditData.dacData.mode = INVALID_MODE;
    AddSecurityLabel(&auditData);
}

#ifdef PARAM_DECODE_GROUPID_FROM_FILE
static char *UserNameTrim(char *str)
{
    if (str == NULL) {
        return NULL;
    }
    size_t len = strlen(str);
    if (str == NULL || len == 0) {
        return NULL;
    }
    char *end = str + len - 1;
    while (end >= str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
    len = strlen(str);
    char *head = str;
    end = str + strlen(str);
    while (head < end && (*head == ' ' || *head == '\t' || *head == '\n' || *head == '\r')) {
        *head = '\0';
        head++;
    }
    if (strlen(str) == 0) {
        return NULL;
    }
    return head;
}

static void LoadGroupUser_(void)
{
    // decode group file
    FILE *fp = fopen(GROUP_FILE_PATH, "r");
    const uint32_t buffSize = 1024;  // 1024 max buffer for decode
    char *buff = (char *)calloc(1, buffSize);
    while (fp != NULL && buff != NULL && fgets(buff, buffSize, fp) != NULL) {
        buff[buffSize - 1] = '\0';
        // deviceprivate:x:1053:root,shell,system,samgr,hdf_devmgr,deviceinfo,dsoftbus,dms,account
        char *buffer = UserNameTrim(buff);
        PARAM_CHECK(buffer != NULL, continue, "Invalid buffer %s", buff);

        PARAM_LOGV("LoadGroupUser_ '%s'", buffer);
        // group name
        char *groupName = strtok(buffer, ":");
        groupName = UserNameTrim(groupName);
        PARAM_CHECK(groupName != NULL, continue, "Invalid group name %s", buff);
        gid_t gid = -1;
        GetGroupIdByName(&gid, groupName);

        // skip x
        (void)strtok(NULL, ":");
        char *strGid = strtok(NULL, ":");
        char *userName = strGid + strlen(strGid) + 1;
        userName = UserNameTrim(userName);
        PARAM_LOGV("LoadGroupUser_ %s userName '%s'", groupName, userName);
        if (userName == NULL) {
            AddGroupUser(groupName, gid);
            continue;
        }
        char *tmp = strtok(userName, ",");
        while (tmp != NULL) {
            PARAM_LOGV("LoadGroupUser_ %s userName '%s'", groupName, tmp);
            AddGroupUser(UserNameTrim(tmp), gid);
            userName = tmp + strlen(tmp) + 1;
            tmp = strtok(NULL, ",");
        }
        // last username
        if (userName != NULL) {
            AddGroupUser(UserNameTrim(userName), gid);
        }
    }
    if (fp != NULL) {
        (void)fclose(fp);
    }
    if (buff != NULL) {
        free(buff);
    }
    return;
}
#else
static void LoadGroupUser_(void)
{
    struct group *data = NULL;
    while ((data = getgrent()) != NULL) {
        if (data->gr_name == NULL || data->gr_mem == NULL) {
            continue;
        }
        if (data->gr_mem[0] == NULL) { // default user in group
            AddGroupUser(data->gr_name, data->gr_gid);
            continue;
        }
        int index = 0;
        while (data->gr_mem[index]) { // user in this group
            AddGroupUser(data->gr_mem[index], data->gr_gid);
            index++;
        }
    }
    endgrent();
}
#endif

INIT_LOCAL_API void LoadGroupUser(void)
{
    PARAM_LOGV("LoadGroupUser ");
    LoadGroupUser_();
}
