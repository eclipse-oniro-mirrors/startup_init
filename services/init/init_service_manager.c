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
#include "init_service_manager.h"

#include <ctype.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "cJSON.h"
#include "init.h"
#include "init_jobs_internal.h"
#include "init_log.h"
#include "init_service_file.h"
#include "init_service_socket.h"
#include "init_utils.h"
#include "securec.h"
#ifdef WITH_SELINUX
#   include "init_selinux_param.h"
#endif // WITH_SELINUX

// All serivce processes that init will fork+exec.
static ServiceSpace g_serviceSpace = { { &g_serviceSpace.services, &g_serviceSpace.services }, 0 };

#ifdef OHOS_SERVICE_DUMP
static void DumpServiceArgs(const char *info, const ServiceArgs *args)
{
    INIT_LOGD("\t\t%s count %d", info, args->count);
    for (int j = 0; j < args->count; j++) {
        if (args->argv[j] != NULL) {
            INIT_LOGD("\t\tinfo [%d] %s", j, args->argv[j]);
        }
    }
}

void DumpAllServices()
{
    INIT_LOGD("Ready to dump all services:");
    INIT_LOGD("total service number: %d", g_serviceSpace.serviceCount);
    ListNode *node = g_serviceSpace.services.next;
    while (node != &g_serviceSpace.services) {
        Service *service = ListEntry(node, Service, node);
        INIT_LOGD("\tservice name: [%s]", service->name);
        INIT_LOGD("\tservice pid: [%d]", service->pid);
        INIT_LOGD("\tservice crashCnt: [%d]", service->crashCnt);
        INIT_LOGD("\tservice attribute: [%d]", service->attribute);
        INIT_LOGD("\tservice importance: [%d]", service->importance);
        INIT_LOGD("\tservice perms uID [%d]", service->servPerm.uID);
        DumpServiceArgs("path arg", &service->pathArgs);
        DumpServiceArgs("writepid file", &service->writePidArgs);

        INIT_LOGD("\tservice perms groupId %d", service->servPerm.gIDCnt);
        for (int i = 0; i < service->servPerm.gIDCnt; i++) {
            INIT_LOGD("\t\tservice perms groupId %d", service->servPerm.gIDArray[i]);
        }

        INIT_LOGD("\tservice perms capability %d", service->servPerm.capsCnt);
        for (int i = 0; i < service->servPerm.capsCnt; i++) {
            INIT_LOGD("\t\tservice perms capability %d", service->servPerm.caps[i]);
        }
        if (service->restartArg != NULL) {
            for (int j = 0; j < service->restartArg->cmdNum; j++) {
                CmdLine *cmd = &service->restartArg->cmds[j];
                INIT_LOGD("\t\tcmd arg: %s %s", GetCmdKey(cmd->cmdIndex), cmd->cmdContent);
            }
        }
        if (service->socketCfg != NULL) {
            INIT_LOGD("\tservice socket name: %s", service->socketCfg->name);
            INIT_LOGD("\tservice socket type: %d", service->socketCfg->type);
            INIT_LOGD("\tservice socket uid: %d", service->socketCfg->uid);
            INIT_LOGD("\tservice socket gid: %d", service->socketCfg->gid);
        }
        node = node->next;
    }
    INIT_LOGD("Dump all services finished");
}
#endif

static void FreeServiceArg(ServiceArgs *arg)
{
    if (arg != NULL) {
        return;
    }
    for (int i = 0; i < arg->count; ++i) {
        if (arg->argv[i] != NULL) {
            free(arg->argv[i]);
            arg->argv[i] = NULL;
        }
    }
    free(arg->argv);
    arg->argv = NULL;
    arg->count = 0;
}

static void FreeServiceSocket(ServiceSocket *sockopt)
{
    while (sockopt != NULL) {
        ServiceSocket *tmp = sockopt;
        if (tmp->sockFd >= 0) {
            close(tmp->sockFd);
            tmp->sockFd = -1;
        }
        sockopt = sockopt->next;
        free(tmp);
    }
    return;
}

static Service *AddService(void)
{
    Service *service = (Service *)calloc(1, sizeof(Service));
    INIT_ERROR_CHECK(service != NULL, return NULL, "Failed to malloc for service");
    ListInit(&service->node);
    ListAddTail(&g_serviceSpace.services, &service->node);
    g_serviceSpace.serviceCount++;
    return service;
}

static void FreeServiceFile(ServiceFile *fileOpt)
{
    while (fileOpt != NULL) {
        ServiceFile *tmp = fileOpt;
        if (tmp->fd >= 0) {
            close(tmp->fd);
            tmp->fd = -1;
        }
        fileOpt = fileOpt->next;
        free(tmp);
    }
    return;
}

static void ReleaseService(Service *service)
{
    if (service == NULL) {
        return;
    }
    FreeServiceArg(&service->pathArgs);
    FreeServiceArg(&service->writePidArgs);

    if (service->servPerm.caps != NULL) {
        free(service->servPerm.caps);
        service->servPerm.caps = NULL;
    }
    service->servPerm.capsCnt = 0;
    if (service->servPerm.gIDArray != NULL) {
        free(service->servPerm.gIDArray);
        service->servPerm.gIDArray = NULL;
    }
    service->servPerm.gIDCnt = 0;
    FreeServiceSocket(service->socketCfg);
    FreeServiceFile(service->fileCfg);

    if (!ListEmpty(service->node)) {
        ListRemove(&service->node);
        g_serviceSpace.serviceCount--;
    }
    free(service);
}

static int GetStringItem(const cJSON *json, const char *name, char *buffer, int buffLen)
{
    INIT_ERROR_CHECK(json != NULL, return SERVICE_FAILURE, "Invalid json for %s", name);
    char *fieldStr = cJSON_GetStringValue(cJSON_GetObjectItem(json, name));
    INIT_ERROR_CHECK(fieldStr != NULL, return SERVICE_FAILURE, "Failed to get string for %s", name);
    size_t strLen = strlen(fieldStr);
    if ((strLen == 0) || (strLen > (size_t)buffLen)) {
        INIT_LOGE("Invalid str filed %s for %s", fieldStr, name);
        return SERVICE_FAILURE;
    }
    return strcpy_s(buffer, buffLen, fieldStr);
}

cJSON *GetArrayItem(const cJSON *fileRoot, int *arrSize, const char *arrName)
{
    cJSON *arrItem = cJSON_GetObjectItemCaseSensitive(fileRoot, arrName);
    if (!cJSON_IsArray(arrItem)) {
        return NULL;
    }
    *arrSize = cJSON_GetArraySize(arrItem);
    if (*arrSize <= 0) {
        return NULL;
    }
    return arrItem;
}

static int GetServiceArgs(const cJSON *argJson, const char *name, int maxCount, ServiceArgs *args)
{
    INIT_ERROR_CHECK(argJson != NULL, return SERVICE_FAILURE, "Invalid argJson");
    cJSON *obj = cJSON_GetObjectItem(argJson, name);
    INIT_CHECK(obj != NULL, return SERVICE_FAILURE);

    int ret = cJSON_IsArray(obj);
    INIT_ERROR_CHECK(ret, return SERVICE_FAILURE, "Invalid type");
    int count = cJSON_GetArraySize(obj);
    INIT_ERROR_CHECK((count > 0) && (count < maxCount), return SERVICE_FAILURE, "Array size = %d is wrong", count);

    args->argv = (char **)malloc((count + 1) * sizeof(char *));
    INIT_ERROR_CHECK(args->argv != NULL, return SERVICE_FAILURE, "Failed to malloc for argv");
    for (int i = 0; i < count + 1; ++i) {
        args->argv[i] = NULL;
    }
    args->count = count + 1;
    for (int i = 0; i < count; ++i) {
        char *curParam = cJSON_GetStringValue(cJSON_GetArrayItem(obj, i));
        INIT_ERROR_CHECK(curParam != NULL, return SERVICE_FAILURE, "Invalid arg %d", i);
        INIT_ERROR_CHECK(strlen(curParam) <= MAX_ONE_ARG_LEN, return SERVICE_FAILURE, "Arg %s is tool long", curParam);
        args->argv[i] = strdup(curParam);
        INIT_ERROR_CHECK(args->argv[i] != NULL, return SERVICE_FAILURE, "Failed to dupstring %s", curParam);
    }
    return SERVICE_SUCCESS;
}

static int GetUid(cJSON *json, uid_t *uid)
{
    INIT_CHECK_RETURN_VALUE(json != NULL, SERVICE_SUCCESS);
    if (cJSON_IsString(json)) {
        char *str = cJSON_GetStringValue(json);
        INIT_ERROR_CHECK(str != NULL, return SERVICE_FAILURE, "Invalid str");
        *uid = DecodeUid(str);
    } else if (cJSON_IsNumber(json)) {
        *uid = (uid_t)cJSON_GetNumberValue(json);
    }
    if (*uid == (uid_t)(-1)) {
        return SERVICE_FAILURE;
    }
    return SERVICE_SUCCESS;
}

static int GetServiceGids(const cJSON *curArrItem, Service *curServ)
{
    int gidCount = 0;
    cJSON *arrItem = cJSON_GetObjectItemCaseSensitive(curArrItem, GID_STR_IN_CFG);
    if (!cJSON_IsArray(arrItem)) {
        gidCount = 1;
    } else {
        gidCount = cJSON_GetArraySize(arrItem);
    }
    if ((gidCount == 0) || (gidCount > NGROUPS_MAX + 1)) {
        INIT_LOGE("Invalid gid count %d", gidCount);
        return SERVICE_FAILURE;
    }
    curServ->servPerm.gIDArray = (gid_t *)malloc(sizeof(gid_t) * gidCount);
    INIT_ERROR_CHECK(curServ->servPerm.gIDArray != NULL, return SERVICE_FAILURE, "Failed to malloc");
    curServ->servPerm.gIDCnt = gidCount;

    gid_t gid;
    if (!cJSON_IsArray(arrItem)) {
        int ret = GetUid(arrItem, &gid);
        INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to uid");
        curServ->servPerm.gIDArray[0] = gid;
        return SERVICE_SUCCESS;
    }

    for (int i = 0; i < gidCount; ++i) {
        cJSON *item = cJSON_GetArrayItem(arrItem, i);
        int ret = GetUid(item, &gid);
        if (ret != 0) {
            curServ->servPerm.gIDArray[i] = 0;
            continue;
        }
        curServ->servPerm.gIDArray[i] = gid;
    }
    return SERVICE_SUCCESS;
}

static int GetServiceAttr(const cJSON *curArrItem, Service *curServ, const char *attrName, int flag,
    int (*processAttr)(Service *curServ, const char *attrName, int value, int flag))
{
    cJSON *filedJ = cJSON_GetObjectItem(curArrItem, attrName);
    if (filedJ == NULL) {
        return SERVICE_SUCCESS;
    }
    if (!cJSON_IsNumber(filedJ)) {
        INIT_LOGE("%s is null or is not a number, service name is %s", attrName, curServ->name);
        return SERVICE_FAILURE;
    }

    int value = (int)cJSON_GetNumberValue(filedJ);
    if (processAttr == NULL) {
        curServ->attribute &= ~flag;
        if (value == 1) {
            curServ->attribute |= flag;
        }
        return 0;
    }
    return processAttr(curServ, attrName, value, flag);
}

static int AddServiceSocket(cJSON *json, Service *service)
{
    char *opt[SOCK_OPT_NUMS] = {
        NULL,
    };
    if (!cJSON_IsString(json) || !cJSON_GetStringValue(json)) {
        return SERVICE_FAILURE;
    }
    char *sockStr = cJSON_GetStringValue(json);
    int num = SplitString(sockStr, " ", opt, SOCK_OPT_NUMS);
    if (num != SOCK_OPT_NUMS) {
        return SERVICE_FAILURE;
    }
    if (opt[SERVICE_SOCK_NAME] == NULL || opt[SERVICE_SOCK_TYPE] == NULL || opt[SERVICE_SOCK_PERM] == NULL ||
        opt[SERVICE_SOCK_UID] == NULL || opt[SERVICE_SOCK_GID] == NULL || opt[SERVICE_SOCK_SETOPT] == NULL) {
        INIT_LOGE("Invalid socket opt");
        return SERVICE_FAILURE;
    }

    ServiceSocket *sockopt = (ServiceSocket *)calloc(1, sizeof(ServiceSocket) + strlen(opt[SERVICE_SOCK_NAME]) + 1);
    INIT_INFO_CHECK(sockopt != NULL, return SERVICE_FAILURE, "Failed to malloc for socket %s", opt[SERVICE_SOCK_NAME]);
    sockopt->sockFd = -1;
    int ret = strcpy_s(sockopt->name, strlen(opt[SERVICE_SOCK_NAME]) + 1, opt[SERVICE_SOCK_NAME]);
    INIT_INFO_CHECK(ret == 0, free(sockopt);
        return SERVICE_FAILURE, "Failed to copy socket name %s", opt[SERVICE_SOCK_NAME]);

    sockopt->type = SOCK_SEQPACKET;
    if (strncmp(opt[SERVICE_SOCK_TYPE], "stream", strlen(opt[SERVICE_SOCK_TYPE])) == 0) {
        sockopt->type = SOCK_STREAM;
    } else if (strncmp(opt[SERVICE_SOCK_TYPE], "dgram", strlen(opt[SERVICE_SOCK_TYPE])) == 0) {
        sockopt->type = SOCK_DGRAM;
    }
    sockopt->perm = strtoul(opt[SERVICE_SOCK_PERM], 0, OCTAL_BASE);
    sockopt->uid = DecodeUid(opt[SERVICE_SOCK_UID]);
    sockopt->gid = DecodeUid(opt[SERVICE_SOCK_GID]);
    if (sockopt->uid == (uid_t)-1 || sockopt->gid == (uid_t)-1) {
        free(sockopt);
        INIT_LOGE("Invalid uid %d or gid %d", sockopt->uid, sockopt->gid);
        return SERVICE_FAILURE;
    }
    sockopt->passcred = false;
    if (strncmp(opt[SERVICE_SOCK_SETOPT], "passcred", strlen(opt[SERVICE_SOCK_SETOPT])) == 0) {
        sockopt->passcred = true;
    }
    sockopt->sockFd = -1;
    sockopt->next = NULL;
    if (service->socketCfg == NULL) {
        service->socketCfg = sockopt;
    } else {
        sockopt->next = service->socketCfg->next;
        service->socketCfg->next = sockopt;
    }
    return SERVICE_SUCCESS;
}

static int ParseServiceSocket(const cJSON *curArrItem, Service *curServ)
{
    int sockCnt = 0;
    cJSON *filedJ = GetArrayItem(curArrItem, &sockCnt, "socket");
    INIT_CHECK(filedJ != NULL && sockCnt > 0, return SERVICE_FAILURE);
    int ret = 0;
    curServ->socketCfg = NULL;
    for (int i = 0; i < sockCnt; ++i) {
        cJSON *sockJ = cJSON_GetArrayItem(filedJ, i);
        ret = AddServiceSocket(sockJ, curServ);
        if (ret != 0) {
            break;
        }
    }
    return ret;
}

static int AddServiceFile(cJSON *json, Service *service)
{
    if (!cJSON_IsString(json) || !cJSON_GetStringValue(json)) {
        return SERVICE_FAILURE;
    }
    char *fileStr = cJSON_GetStringValue(json);
    char *opt[FILE_OPT_NUMS] = {
        NULL,
    };
    int num = SplitString(fileStr, " ", opt, FILE_OPT_NUMS);
    if (num != FILE_OPT_NUMS) {
        return SERVICE_FAILURE;
    }
    if (opt[SERVICE_FILE_NAME] == NULL || opt[SERVICE_FILE_FLAGS] == NULL || opt[SERVICE_FILE_PERM] == NULL ||
            opt[SERVICE_FILE_UID] == NULL || opt[SERVICE_FILE_GID] == NULL) {
        INIT_LOGE("Invalid file opt");
        return SERVICE_FAILURE;
    }
    ServiceFile *fileOpt = (ServiceFile *)calloc(1, sizeof(ServiceFile) + strlen(opt[SERVICE_FILE_NAME]) + 1);
    INIT_INFO_CHECK(fileOpt != NULL, return SERVICE_FAILURE, "Failed to calloc for file %s", opt[SERVICE_FILE_NAME]);
    int ret = strcpy_s(fileOpt->fileName, strlen(opt[SERVICE_FILE_NAME]) + 1, opt[SERVICE_FILE_NAME]);
    INIT_INFO_CHECK(ret == 0, free(fileOpt);
        return SERVICE_FAILURE, "Failed to copy file name %s", opt[SERVICE_FILE_NAME]);
    if (strcmp(opt[SERVICE_FILE_FLAGS], "rd") == 0) {
        fileOpt->flags = O_RDONLY;
    } else if (strcmp(opt[SERVICE_FILE_FLAGS], "wd") == 0) {
        fileOpt->flags = O_WRONLY;
    } else if (strcmp(opt[SERVICE_FILE_FLAGS], "rw") == 0) {
        fileOpt->flags = O_RDWR;
    } else {
        INIT_LOGE("Failed file flags %s", opt[SERVICE_FILE_FLAGS]);
        return SERVICE_FAILURE;
    }
    fileOpt->perm = strtoul(opt[SERVICE_FILE_PERM], 0, OCTAL_BASE);
    fileOpt->uid = DecodeUid(opt[SERVICE_FILE_UID]);
    fileOpt->gid = DecodeUid(opt[SERVICE_FILE_GID]);
    if (fileOpt->uid == (uid_t)-1 || fileOpt->gid == (gid_t)-1) {
        free(fileOpt);
        INIT_LOGE("Invalid uid %d or gid %d", fileOpt->uid, fileOpt->gid);
        return SERVICE_FAILURE;
    }
    fileOpt->fd = -1;
    fileOpt->next = NULL;
    if (service->fileCfg == NULL) {
        service->fileCfg = fileOpt;
    } else {
        fileOpt->next = service->fileCfg->next;
        service->fileCfg->next = fileOpt;
    }
    return SERVICE_SUCCESS;
}

static int ParseServiceFile(const cJSON *curArrItem, Service *curServ)
{
    int fileCnt = 0;
    cJSON *filedJ = GetArrayItem(curArrItem, &fileCnt, "file");
    INIT_CHECK(filedJ != NULL && fileCnt > 0, return SERVICE_FAILURE);
    int ret = 0;
    curServ->fileCfg = NULL;
    for (int i = 0; i < fileCnt; ++i) {
        cJSON *fileJ = cJSON_GetArrayItem(filedJ, i);
        ret = AddServiceFile(fileJ, curServ);
        if (ret != 0) {
            break;
        }
    }
    return ret;
}

static bool IsServiceInMainStrap(Service *curServ)
{
    char *mainServiceList[] = {
        "appspawn", "udevd",  "samgr",      "multimodalinput", "weston",         "installs",
        "hiview",   "hilogd", "hdf_devmgr", "distributedsche", "softbus_server", "foundation"
    };
    unsigned int length = ARRAY_LENGTH(mainServiceList);
    for (unsigned int i = 0; i < length; ++i) {
        if (strncmp(curServ->name, mainServiceList[i], strlen(mainServiceList[i])) == 0) {
            INIT_LOGI("%s must be main service", curServ->name);
            return true;
        }
    }
    return false;
}

static int GetDynamicService(const cJSON *curArrItem, Service *curServ)
{
    cJSON *item = cJSON_GetObjectItem(curArrItem, "dynamic");
    if (item == NULL) {
        return SERVICE_SUCCESS;
    }

    INIT_ERROR_CHECK(cJSON_IsBool(item), return SERVICE_FAILURE,
        "Service : %s dynamic value only support bool.", curServ->name);
    bool isDynamic = (bool)cJSON_GetNumberValue(item);
    if (!isDynamic) {
        INIT_LOGI("Service : %s dynamic value is false, it will be started with init.", curServ->name);
        return SERVICE_SUCCESS;
    }

    if (IsServiceInMainStrap(curServ)) {
        return SERVICE_SUCCESS;
    }
    INIT_LOGI("%s is dynamic service", curServ->name);
    curServ->attribute |= SERVICE_ATTR_DYNAMIC;
    curServ->attribute |= SERVICE_ATTR_ONCE;
    return SERVICE_SUCCESS;
}

static int CheckServiceKeyName(const cJSON *curService)
{
    char *cfgServiceKeyList[] = {
        "name", "path", "uid", "gid", "once", "importance", "caps", "disabled",
        "writepid", "critical", "socket", "console", "dynamic", "file",
#ifdef WITH_SELINUX
        SECON_STR_IN_CFG,
#endif // WITH_SELINUX
    };
    if (curService == NULL) {
        return SERVICE_FAILURE;
    }
    cJSON *child = curService->child;
    if (child == NULL) {
        return SERVICE_FAILURE;
    }
    while (child != NULL) {
        int i = 0;
        int keyListSize = ARRAY_LENGTH(cfgServiceKeyList);
        for (; i < keyListSize; i++) {
            if (strcmp(child->string, cfgServiceKeyList[i]) == 0) {
                break;
            }
        }
        if (i < keyListSize) {
            child = child->next;
        } else {
            INIT_LOGE("CheckServiceKeyName, key name %s is not found. error.", child->string);
            return SERVICE_FAILURE;
        }
    }
    return SERVICE_SUCCESS;
}

static int ParseOneService(const cJSON *curItem, Service *service)
{
    if (curItem == NULL || service == NULL) {
        return SERVICE_FAILURE;
    }
    int ret = GetStringItem(curItem, "name", service->name, MAX_SERVICE_NAME);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get service name");
#ifdef WITH_SELINUX
    ret = GetStringItem(curItem, SECON_STR_IN_CFG, service->secon, MAX_SECON_LEN);
    INIT_CHECK_ONLY_ELOG(ret == 0, "GetServiceSecon %s section not found, skip", SECON_STR_IN_CFG);
#endif // WITH_SELINUX
    ret = GetServiceArgs(curItem, "path", MAX_PATH_ARGS_CNT, &service->pathArgs);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get path for service %s", service->name);
    if ((service->pathArgs.count > 0) && IsForbidden(service->pathArgs.argv[0])) {
        INIT_LOGE("Service %s is forbidden.", service->name);
        return SERVICE_FAILURE;
    }
    ret = GetUid(cJSON_GetObjectItem(curItem, UID_STR_IN_CFG), &service->servPerm.uID);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get uid for service %s", service->name);
    ret = GetServiceGids(curItem, service);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get gid for service %s", service->name);

    ret = GetServiceAttr(curItem, service, ONCE_STR_IN_CFG, SERVICE_ATTR_ONCE, NULL);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get once flag for service %s", service->name);
    ret = GetServiceAttr(curItem, service, IMPORTANT_STR_IN_CFG, SERVICE_ATTR_IMPORTANT, SetImportantValue);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get import flag for service %s", service->name);
    ret = GetServiceAttr(curItem, service, CRITICAL_STR_IN_CFG, SERVICE_ATTR_CRITICAL, NULL);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get critical flag for service %s", service->name);
    ret = GetServiceAttr(curItem, service, DISABLED_STR_IN_CFG, SERVICE_ATTR_DISABLED, NULL);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get disabled flag for service %s", service->name);
    ret = GetServiceAttr(curItem, service, CONSOLE_STR_IN_CFG, SERVICE_ATTR_CONSOLE, NULL);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get console for service %s", service->name);

    ret = GetServiceArgs(curItem, "writepid", MAX_WRITEPID_FILES, &service->writePidArgs);
    INIT_CHECK_ONLY_ELOG(ret == 0, "No writepid arg for service %s", service->name);
    ret = GetServiceCaps(curItem, service);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get caps for service %s", service->name);
    ret = GetDynamicService(curItem, service);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get dynamic flag for service %s", service->name);
    return ret;
}

void ParseAllServices(const cJSON *fileRoot)
{
    int servArrSize = 0;
    cJSON *serviceArr = GetArrayItem(fileRoot, &servArrSize, SERVICES_ARR_NAME_IN_JSON);
    INIT_INFO_CHECK(serviceArr != NULL, return, "This config does not contain service array.");

    INIT_ERROR_CHECK(servArrSize <= MAX_SERVICES_CNT_IN_FILE, return,
        "Too many services[cnt %d] detected, should not exceed %d.",
        servArrSize, MAX_SERVICES_CNT_IN_FILE);

    Service tmpService = {};
    for (int i = 0; i < servArrSize; ++i) {
        cJSON *curItem = cJSON_GetArrayItem(serviceArr, i);
        int ret = GetStringItem(curItem, "name", tmpService.name, MAX_SERVICE_NAME);
        if (ret != 0) {
            INIT_LOGE("Failed to get service name %s", tmpService.name);
            continue;
        }
        if (CheckServiceKeyName(curItem) != SERVICE_SUCCESS) { // invalid service
            INIT_LOGE("Invalid service name %s", tmpService.name);
            continue;
        }
        Service *service = GetServiceByName(tmpService.name);
        if (service != NULL) {
            INIT_LOGE("Service name %s has been exist", tmpService.name);
            continue;
        }
        service = AddService();
        if (service == NULL) {
            INIT_LOGE("Failed to create service name %s", tmpService.name);
            continue;
        }

        ret = ParseOneService(curItem, service);
        if (ret != SERVICE_SUCCESS) {
            ReleaseService(service);
            continue;
        }
        if (ParseServiceSocket(curItem, service) != SERVICE_SUCCESS) {
            FreeServiceSocket(service->socketCfg);
            service->socketCfg = NULL;
        }
        if (ParseServiceFile(curItem, service) != SERVICE_SUCCESS) {
            FreeServiceFile(service->fileCfg);
            service->fileCfg = NULL;
        }
        ret = GetCmdLinesFromJson(cJSON_GetObjectItem(curItem, "onrestart"), &service->restartArg);
        if (ret != SERVICE_SUCCESS) {
            service->restartArg = NULL;
        }
        INIT_LOGD("service[%d] name=%s, uid=%d, critical=%d, disabled=%d",
            i, service->name, service->servPerm.uID,
            (service->attribute & SERVICE_ATTR_CRITICAL) ? 1 : 0,
            (service->attribute & SERVICE_ATTR_DISABLED) ? 1 : 0);
    }
}

void StartServiceByName(const char *servName, bool checkDynamic)
{
    Service *service = GetServiceByName(servName);
    INIT_ERROR_CHECK(service != NULL, return, "Cannot find service %s.", servName);

    if (checkDynamic && (service->attribute & SERVICE_ATTR_DYNAMIC)) {
        INIT_LOGI("%s is dynamic service.", servName);
        return;
    }
    if (ServiceStart(service) != SERVICE_SUCCESS) {
        INIT_LOGE("Service %s start failed!", servName);
    }
    return;
}

void StopServiceByName(const char *servName)
{
    Service *service = GetServiceByName(servName);
    INIT_ERROR_CHECK(service != NULL, return, "Cannot find service %s.", servName);

    if (ServiceStop(service) != SERVICE_SUCCESS) {
        INIT_LOGE("Service %s start failed!", servName);
    }
    return;
}

void StopAllServices(int flags)
{
    ListNode *node = g_serviceSpace.services.next;
    while (node != &g_serviceSpace.services) {
        Service *service = ListEntry(node, Service, node);
        service->attribute |= flags;
        int ret = ServiceStop(service);
        if (ret != SERVICE_SUCCESS) {
            INIT_LOGE("Service %s stop failed!", service->name);
        }
        node = node->next;
    }
}

Service *GetServiceByPid(pid_t pid)
{
    ListNode *node = g_serviceSpace.services.next;
    while (node != &g_serviceSpace.services) {
        Service *service = ListEntry(node, Service, node);
        if (service->pid == pid) {
            return service;
        }
        node = node->next;
    }
    return NULL;
}

Service *GetServiceByName(const char *servName)
{
    ListNode *node = g_serviceSpace.services.next;
    while (node != &g_serviceSpace.services) {
        Service *service = ListEntry(node, Service, node);
        if (strcmp(service->name, servName) == 0) {
            return service;
        }
        node = node->next;
    }
    return NULL;
}
