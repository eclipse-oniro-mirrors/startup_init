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
#include "init_adapter.h"
#include "init_jobs.h"
#include "init_log.h"
#include "init_perms.h"
#include "init_read_cfg.h"
#include "init_capability.h"
#include "init_service_socket.h"
#include "init_utils.h"
#include "securec.h"

// All serivce processes that init will fork+exec.
static Service* g_services = NULL;
static int g_servicesCnt = 0;

void DumpAllServices()
{
    printf("[Init] Ready to dump all services:\n");
    printf("[Init] total service number: %d\n", g_servicesCnt);
    for (int i = 0; i < g_servicesCnt; i++) {
        printf("\tservice name: [%s]\n", g_services[i].name);
        printf("\tpath :");
        for (int j = 0; j < g_services[i].pathArgsCnt; j++) {
            printf(" %s", g_services[i].pathArgs[j]);
        }
        printf("\n");
    }
    printf("[Init] Dump all services finished\n");
}

void RegisterServices(Service* services, int servicesCnt)
{
    g_services = services;
    g_servicesCnt += servicesCnt;
}

static void ReleaseServiceMem(Service* curServ)
{
    if (curServ->pathArgs != NULL) {
        for (int i = 0; i < curServ->pathArgsCnt; ++i) {
            if (curServ->pathArgs[i] != NULL) {
                free(curServ->pathArgs[i]);
                curServ->pathArgs[i] = NULL;
            }
        }
        free(curServ->pathArgs);
        curServ->pathArgs = NULL;
    }
    curServ->pathArgsCnt = 0;

    if (curServ->servPerm.caps != NULL) {
        free(curServ->servPerm.caps);
        curServ->servPerm.caps = NULL;
    }
    curServ->servPerm.capsCnt = 0;

    for (int i = 0; i < MAX_WRITEPID_FILES; i++) {
        if (curServ->writepidFiles[i] != NULL) {
            free(curServ->writepidFiles[i]);
            curServ->writepidFiles[i] = NULL;
        }
    }

    if (curServ->servPerm.gIDArray != NULL) {
        free(curServ->servPerm.gIDArray);
        curServ->servPerm.gIDArray = NULL;
    }
    curServ->servPerm.gIDCnt = 0;
}

static int GetServiceName(const cJSON* curArrItem, Service* curServ)
{
    char* fieldStr = cJSON_GetStringValue(cJSON_GetObjectItem(curArrItem, "name"));
    if (fieldStr == NULL) {
        INIT_LOGE("[init] GetServiceName cJSON_GetStringValue error\n");
        return SERVICE_FAILURE;
    }

    size_t strLen = strlen(fieldStr);
    if (strLen == 0 || strLen > MAX_SERVICE_NAME) {
        INIT_LOGE("[init] GetServiceName strLen = %d, error\n", strLen);
        return SERVICE_FAILURE;
    }

    if (memcpy_s(curServ->name, MAX_SERVICE_NAME, fieldStr, strLen) != EOK) {
        INIT_LOGE("[init] GetServiceName memcpy_s error\n");
        return SERVICE_FAILURE;
    }
    curServ->name[strLen] = '\0';
    return SERVICE_SUCCESS;
}

static int IsForbidden(const char* fieldStr)
{
    size_t fieldLen = strlen(fieldStr);
    size_t forbidStrLen = strlen(BIN_SH_NOT_ALLOWED);
    if (fieldLen == forbidStrLen) {
        if (strncmp(fieldStr, BIN_SH_NOT_ALLOWED, fieldLen) == 0) {
            return 1;
        }
        return 0;
    } else if (fieldLen > forbidStrLen) {
        // "/bin/shxxxx" is valid but "/bin/sh xxxx" is invalid
        if (strncmp(fieldStr, BIN_SH_NOT_ALLOWED, forbidStrLen) == 0) {
            if (fieldStr[forbidStrLen] == ' ') {
                return 1;
            }
        }
        return 0;
    } else {
        return 0;
    }
}

// TODO: move this function to common files
static cJSON* GetArrItem(const cJSON* fileRoot, int* arrSize, const char* arrName)
{
    cJSON* arrItem = cJSON_GetObjectItemCaseSensitive(fileRoot, arrName);
    if (!cJSON_IsArray(arrItem)) {
        printf("[Init] GetArrItem, item %s is not an array!\n", arrName);
        return NULL;
    }

    *arrSize = cJSON_GetArraySize(arrItem);
    if (*arrSize <= 0) {
        return NULL;
    }
    return arrItem;
}

static int GetWritepidStrings(const cJSON *curArrItem, Service *curServ)        // writepid
{
    int writepidCnt = 0;
    cJSON* filedJ = GetArrItem(curArrItem, &writepidCnt, "writepid");
    if (writepidCnt <= 0) {             // not item is ok.
        return SERVICE_SUCCESS;
    }

    if (writepidCnt > MAX_WRITEPID_FILES) {
        INIT_LOGE("[Init] GetWritepidStrings, too many writepid[cnt %d] for one service, should not exceed %d.\n",
            writepidCnt, MAX_WRITEPID_FILES);
        return SERVICE_FAILURE;
    }

    for (int i = 0; i < writepidCnt; ++i) {
        if (!cJSON_GetArrayItem(filedJ, i) || !cJSON_GetStringValue(cJSON_GetArrayItem(filedJ, i))
            || strlen(cJSON_GetStringValue(cJSON_GetArrayItem(filedJ, i))) <= 0) {      // check all errors
            INIT_LOGE("[Init] GetWritepidStrings, parse item[%d] error.\n", i);
            return SERVICE_FAILURE;
        }

        char *fieldStr = cJSON_GetStringValue(cJSON_GetArrayItem(filedJ, i));
        size_t strLen = strlen(fieldStr);
        curServ->writepidFiles[i] = (char *)malloc(sizeof(char) * strLen + 1);
        if (curServ->writepidFiles[i] == NULL) {
            INIT_LOGE("[Init] GetWritepidStrings, malloc item[%d] error.\n", i);
            return SERVICE_FAILURE;
        }
        if (memcpy_s(curServ->writepidFiles[i], strLen + 1, fieldStr, strLen) != EOK) {
            INIT_LOGE("[Init] GetWritepidStrings, memcpy_s error.\n");
            return SERVICE_FAILURE;
        }
        curServ->writepidFiles[i][strLen] = '\0';
    }

    return SERVICE_SUCCESS;
}

static int GetGidOneItem(const cJSON *curArrItem, Service *curServ)        // gid one item
{
    cJSON* filedJ = cJSON_GetObjectItem(curArrItem, GID_STR_IN_CFG);
    if (filedJ == NULL) {
        INIT_LOGE("[Init] GetGidOneItem, gid is not an item.\n");
        return SERVICE_FAILURE;             // not found
    }
    curServ->servPerm.gIDCnt = 1;
    curServ->servPerm.gIDArray = (gid_t *)malloc(sizeof(gid_t));
    if (curServ->servPerm.gIDArray == NULL) {
        INIT_LOGE("[Init] GetGidOneItem, can't malloc, error.\n");
        return SERVICE_FAILURE;
    }

    if (cJSON_IsString(filedJ)) {
        char* fieldStr = cJSON_GetStringValue(filedJ);
        gid_t gID = DecodeUid(fieldStr);
        if (gID == (gid_t)(-1)) {
            INIT_LOGE("[Init] GetGidOneItem, DecodeUid %s error.\n", fieldStr);
            return SERVICE_FAILURE;
        }
        curServ->servPerm.gIDArray[0] = gID;
        return SERVICE_SUCCESS;
    }

    if (cJSON_IsNumber(filedJ)) {
        gid_t gID = (int)cJSON_GetNumberValue(filedJ);
        if (gID < 0) {
            INIT_LOGE("[Init] GetGidOneItem, gID = %d error.\n", gID);
            return SERVICE_FAILURE;
        }
        curServ->servPerm.gIDArray[0] = gID;
        return SERVICE_SUCCESS;
    }

    INIT_LOGE("[Init] GetGidOneItem, this gid is neither a string nor a number, error.\n");
    return SERVICE_FAILURE;
}

static int GetGidArray(const cJSON *curArrItem, Service *curServ)        // gid array
{
    int gIDCnt = 0;
    cJSON* filedJ = GetArrItem(curArrItem, &gIDCnt, GID_STR_IN_CFG);        // "gid" must have 1 item.
    if (gIDCnt <= 0) {              // not a array, but maybe a item?
        INIT_LOGE("[Init] GetGidArray, gid is not a list.\n");
        return GetGidOneItem(curArrItem, curServ);
    }

    if (gIDCnt > NGROUPS_MAX + 1) {
        INIT_LOGE("[Init] GetGidArray, too many gids[cnt %d] for one service, should not exceed %d.\n",
            gIDCnt, NGROUPS_MAX + 1);
        return SERVICE_FAILURE;
    }

    curServ->servPerm.gIDArray = (gid_t *)malloc(sizeof(gid_t) * gIDCnt);
    if (curServ->servPerm.gIDArray == NULL) {
        INIT_LOGE("[init] GetGidArray malloc error\n");
        return SERVICE_FAILURE;
    }
    curServ->servPerm.gIDCnt = gIDCnt;
    int i = 0;
    for (; i < gIDCnt; ++i) {
        if (cJSON_GetArrayItem(filedJ, i) == NULL || !cJSON_GetStringValue(cJSON_GetArrayItem(filedJ, i))
            || strlen(cJSON_GetStringValue(cJSON_GetArrayItem(filedJ, i))) <= 0) {      // check all errors
            INIT_LOGE("[Init] GetGidArray, parse item[%d] as string, error.\n", i);
            break;
        }
        char* fieldStr = cJSON_GetStringValue(cJSON_GetArrayItem(filedJ, i));
        gid_t gID = DecodeUid(fieldStr);
        if ((gID) == (gid_t)(-1)) {
            INIT_LOGE("[Init] GetGidArray, DecodeUid item[%d] error.\n", i);
            return SERVICE_FAILURE;
        }
        curServ->servPerm.gIDArray[i] = gID;
    }
    if (i == gIDCnt) {
        return SERVICE_SUCCESS;
    }
    for (i = 0; i < gIDCnt; ++i) {
        if (cJSON_GetArrayItem(filedJ, i) == NULL || !cJSON_IsNumber(cJSON_GetArrayItem(filedJ, i))) {
            INIT_LOGE("[Init] GetGidArray, parse item[%d] as number, error.\n", i);
            break;
        }
        gid_t gID = (int)cJSON_GetNumberValue(cJSON_GetArrayItem(filedJ, i));
        if (gID < 0) {
            INIT_LOGE("[init] GetGidArray gID = %d, error\n", gID);
            break;
        }
        curServ->servPerm.gIDArray[i] = gID;
    }
    int ret = i == gIDCnt ? SERVICE_SUCCESS : SERVICE_FAILURE;
    return ret;
}

static int GetServicePathAndArgs(const cJSON* curArrItem, Service* curServ)
{
    cJSON* pathItem = cJSON_GetObjectItem(curArrItem, "path");
    if (!cJSON_IsArray(pathItem)) {
        INIT_LOGE("[init] GetServicePathAndArgs path item not found or not a array\n");
        return SERVICE_FAILURE;
    }

    int arrSize = cJSON_GetArraySize(pathItem);
    if (arrSize <= 0 || arrSize > MAX_PATH_ARGS_CNT) {  // array size invalid
        INIT_LOGE("[init] GetServicePathAndArgs arrSize = %d, error\n", arrSize);
        return SERVICE_FAILURE;
    }

    curServ->pathArgs = (char**)malloc((arrSize + 1) * sizeof(char*));
    if (curServ->pathArgs == NULL) {
        INIT_LOGE("[init] GetServicePathAndArgs malloc 1 error\n");
        return SERVICE_FAILURE;
    }
    for (int i = 0; i < arrSize + 1; ++i) {
        curServ->pathArgs[i] = NULL;
    }
    curServ->pathArgsCnt = arrSize + 1;

    for (int i = 0; i < arrSize; ++i) {
        char* curParam = cJSON_GetStringValue(cJSON_GetArrayItem(pathItem, i));
        if (curParam == NULL || strlen(curParam) > MAX_ONE_ARG_LEN) {
            // resources will be released by function: ReleaseServiceMem
            if (curParam == NULL) {
                INIT_LOGE("[init] GetServicePathAndArgs curParam == NULL, error\n");
            } else {
                INIT_LOGE("[init] GetServicePathAndArgs strlen = %d, error\n", strlen(curParam));
            }
            return SERVICE_FAILURE;
        }

        if (i == 0 && IsForbidden(curParam)) {
            // resources will be released by function: ReleaseServiceMem
            INIT_LOGE("[init] GetServicePathAndArgs i == 0 && IsForbidden, error\n");
            return SERVICE_FAILURE;
        }

        size_t paramLen = strlen(curParam);
        curServ->pathArgs[i] = (char*)malloc(paramLen + 1);
        if (curServ->pathArgs[i] == NULL) {
            // resources will be released by function: ReleaseServiceMem
            INIT_LOGE("[init] GetServicePathAndArgs i == 0 && IsForbidden, error\n");
            return SERVICE_FAILURE;
        }

        if (memcpy_s(curServ->pathArgs[i], paramLen + 1, curParam, paramLen) != EOK) {
            // resources will be released by function: ReleaseServiceMem
            INIT_LOGE("[init] GetServicePathAndArgs malloc 2 error.\n");
            return SERVICE_FAILURE;
        }
        curServ->pathArgs[i][paramLen] = '\0';
    }
    return SERVICE_SUCCESS;
}

static int GetServiceNumber(const cJSON* curArrItem, Service* curServ, const char* targetField)
{
    cJSON* filedJ = cJSON_GetObjectItem(curArrItem, targetField);
    if (filedJ == NULL && (strncmp(targetField, CRITICAL_STR_IN_CFG, strlen(CRITICAL_STR_IN_CFG)) == 0
        || strncmp(targetField, DISABLED_STR_IN_CFG, strlen(DISABLED_STR_IN_CFG)) == 0
        || strncmp(targetField, ONCE_STR_IN_CFG, strlen(ONCE_STR_IN_CFG)) == 0
        || strncmp(targetField, IMPORTANT_STR_IN_CFG, strlen(IMPORTANT_STR_IN_CFG)) == 0)) {
        return SERVICE_SUCCESS;             // not found "critical","disabled","once","importance" item is ok
    }

    if (!cJSON_IsNumber(filedJ)) {
        INIT_LOGE("[Init] GetServiceNumber, %s is null or is not a number, error.\n", targetField);
        return SERVICE_FAILURE;
    }

    int value = (int)cJSON_GetNumberValue(filedJ);
    if (value < 0) {
        INIT_LOGE("[Init] GetServiceNumber, value = %d, error.\n", value);
        return SERVICE_FAILURE;
    }

    if (strncmp(targetField, ONCE_STR_IN_CFG, strlen(ONCE_STR_IN_CFG)) == 0) {
        if (value != 0) {
            curServ->attribute |= SERVICE_ATTR_ONCE;
        }
    } else if (strncmp(targetField, IMPORTANT_STR_IN_CFG, strlen(IMPORTANT_STR_IN_CFG)) == 0) {
        if (value != 0) {
            curServ->attribute |= SERVICE_ATTR_IMPORTANT;
        }
    } else if (strncmp(targetField, CRITICAL_STR_IN_CFG, strlen(CRITICAL_STR_IN_CFG)) == 0) {       // set critical
        curServ->attribute &= ~SERVICE_ATTR_CRITICAL;
        if (value == 1) {
            curServ->attribute |= SERVICE_ATTR_CRITICAL;
        }
    } else if (strncmp(targetField, DISABLED_STR_IN_CFG, strlen(DISABLED_STR_IN_CFG)) == 0) {       // set disabled
        curServ->attribute &= ~SERVICE_ATTR_DISABLED;
        if (value == 1) {
            curServ->attribute |= SERVICE_ATTR_DISABLED;
        }
    } else {
        INIT_LOGE("[Init] GetServiceNumber, item = %s, not expected, error.\n", targetField);
        return SERVICE_FAILURE;
    }
    return SERVICE_SUCCESS;
}

static int GetUidStringNumber(const cJSON *curArrItem, Service *curServ)
{
    cJSON* filedJ = cJSON_GetObjectItem(curArrItem, UID_STR_IN_CFG);
    if (filedJ == NULL) {
        INIT_LOGE("[Init] GetUidStringNumber, %s not found, error.\n", UID_STR_IN_CFG);
        return SERVICE_FAILURE;             // not found
    }

    if (cJSON_IsString(filedJ)) {
        char* fieldStr = cJSON_GetStringValue(filedJ);
        int uID = DecodeUid(fieldStr);
        if (uID < 0) {
            INIT_LOGE("[Init] GetUidStringNumber, DecodeUid %s error.\n", fieldStr);
            return SERVICE_FAILURE;
        }
        curServ->servPerm.uID = uID;
        return SERVICE_SUCCESS;
    }

    if (cJSON_IsNumber(filedJ)) {
        int uID = (int)cJSON_GetNumberValue(filedJ);
        if (uID < 0) {
            INIT_LOGE("[Init] GetUidStringNumber, uID = %d error.\n", uID);
            return SERVICE_FAILURE;
        }
        curServ->servPerm.uID = uID;
        return SERVICE_SUCCESS;
    }

    INIT_LOGE("[Init] GetUidStringNumber, this uid is neither a string nor a number, error.\n");
    return SERVICE_FAILURE;
}

static int SplitString(char *srcPtr, char **dstPtr)
{
    if (!srcPtr) {
        return -1;
    }
    char *buf = NULL;
    dstPtr[0] = strtok_r(srcPtr, " ", &buf);
    int i = 0;
    while (dstPtr[i])
    {
        i++;
        dstPtr[i] = strtok_r(NULL, " ", &buf);
    }
    dstPtr[i] = "\0";
    int num = i;
    for (int j = 0; j < num; j++)
    {
        printf("dstPtr[%d] is %s \n", j, dstPtr[j]);
    }
    return num;
}

#define MAX_SOCK_NAME_LEN  16
#define SOCK_OPT_NUMS  6
enum SockOptionTab
{
    SERVICE_SOCK_NAME = 0,
    SERVICE_SOCK_TYPE,
    SERVICE_SOCK_PERM,
    SERVICE_SOCK_UID,
    SERVICE_SOCK_GID,
    SERVICE_SOCK_SETOPT
};

static int ParseServiceSocket(char **opt, const int optNum, struct ServiceSocket *sockopt)
{
    printf("[init] ParseServiceSocket\n");
    if (optNum != SOCK_OPT_NUMS) {
        return -1;
    }
    sockopt->name = (char *)calloc(MAX_SOCK_NAME_LEN, sizeof(char));
    if (sockopt->name == NULL) {
        return -1;
    }
    if (opt[SERVICE_SOCK_NAME] == NULL) {
        return -1;
    }
    printf("[init] ParseServiceSocket SERVICE_SOCK_NAME is %s \n", opt[SERVICE_SOCK_NAME]);
    int ret = memcpy_s(sockopt->name, MAX_SOCK_NAME_LEN, opt[SERVICE_SOCK_NAME], MAX_SOCK_NAME_LEN - 1);
    if (ret != 0) {
        return -1;
    }

    if (opt[SERVICE_SOCK_TYPE] == NULL) {
        return -1;
    }
    printf("[init] ParseServiceSocket SERVICE_SOCK_TYPE is %s \n", opt[SERVICE_SOCK_TYPE]);
    sockopt->type =
        strncmp(opt[SERVICE_SOCK_TYPE], "stream", strlen(opt[SERVICE_SOCK_TYPE])) == 0 ? SOCK_STREAM :
        (strncmp(opt[SERVICE_SOCK_TYPE], "dgram", strlen(opt[SERVICE_SOCK_TYPE])) == 0 ? SOCK_DGRAM : SOCK_SEQPACKET);

    if (opt[SERVICE_SOCK_PERM] == NULL) {
        return -1;
    }
    printf("[init] ParseServiceSocket SERVICE_SOCK_PERM is %s \n", opt[SERVICE_SOCK_PERM]);
    sockopt->perm = strtoul(opt[SERVICE_SOCK_PERM], 0, 8);  //¡Áa?¡¥?a8????

    if (opt[SERVICE_SOCK_UID] == NULL) {
        return -1;
    }
    printf("[init] ParseServiceSocket SERVICE_SOCK_UID is %s \n", opt[SERVICE_SOCK_UID]);
    int uuid = DecodeUid(opt[SERVICE_SOCK_UID]);
    if (uuid < 0) {
        return -1;
    }
    sockopt->uid = uuid;
    printf("[init] ParseServiceSocket uuid is %d \n", uuid);

    if (opt[SERVICE_SOCK_GID] == NULL) {
        return -1;
    }
    printf("[init] ParseServiceSocket SERVICE_SOCK_GID is %s \n", opt[SERVICE_SOCK_GID]);
    int ggid = DecodeUid(opt[SERVICE_SOCK_GID]);
    if (ggid < 0) {
        return -1;
    }
    sockopt->gid = ggid;
    printf("[init] ParseServiceSocket ggid is %d \n", ggid);

    if (opt[SERVICE_SOCK_SETOPT] == NULL) {
        return -1;
    }
    printf("[init] ParseServiceSocket SERVICE_SOCK_SETOPT is %s \n", opt[SERVICE_SOCK_SETOPT]);
    sockopt->passcred = strncmp(opt[SERVICE_SOCK_SETOPT], "passcred", strlen(opt[SERVICE_SOCK_SETOPT])) == 0 ? true : false;
    sockopt->next = NULL;
    return 0;
}

static int GetServiceSocket(const cJSON* curArrItem, Service* curServ)
{
    printf("[init] GetServiceSocket \n");
    cJSON* filedJ = cJSON_GetObjectItem(curArrItem, "socket");
    if (!cJSON_IsArray(filedJ)) {
        return SERVICE_FAILURE;
    }

    int sockCnt = cJSON_GetArraySize(filedJ);
    if (sockCnt <= 0) {
        return SERVICE_FAILURE;
    }
    printf("[init] GetServiceSocket sockCnt is %d \n", sockCnt);
    curServ->socketCfg = NULL;
    for (int i = 0; i < sockCnt; ++i) {
        cJSON* sockJ = cJSON_GetArrayItem(filedJ, i);
        if (!cJSON_IsString(sockJ) || !cJSON_GetStringValue(sockJ)) {
            return SERVICE_FAILURE;
        }
        char *sockStr = cJSON_GetStringValue(sockJ);
        char *tmpStr[SOCK_OPT_NUMS] = {NULL,};
        int num = SplitString(sockStr, tmpStr);
        if (num != SOCK_OPT_NUMS) {
            return SERVICE_FAILURE;
        }
        printf("[init] GetServiceSocket num is %d \n", num);
        struct ServiceSocket *socktmp = (struct ServiceSocket *)calloc(1, sizeof(struct ServiceSocket));
        if (!socktmp) {
            return SERVICE_FAILURE;
        }
        printf("[init] ParseServiceSocket\n");
        int ret = ParseServiceSocket(tmpStr, SOCK_OPT_NUMS, socktmp);
        if (ret < 0) {
            return SERVICE_FAILURE;
        }
        if (curServ->socketCfg == NULL) {
            curServ->socketCfg = socktmp;
        } else {
            socktmp->next = curServ->socketCfg->next;
            curServ->socketCfg->next = socktmp;
        }
    }
    return SERVICE_SUCCESS;
}

static int GetServiceOnRestart(const cJSON* curArrItem, Service* curServ)
{
    printf("[init] GetServiceOnRestart \n");
    cJSON* filedJ = cJSON_GetObjectItem(curArrItem, "onrestart");
    if (!cJSON_IsArray(filedJ)) {
            return SERVICE_FAILURE;
    }
    int cmdCnt = cJSON_GetArraySize(filedJ);
    if (cmdCnt <= 0) {
        return SERVICE_FAILURE;
    }
    curServ->onRestart = (struct OnRestartCmd *)calloc(1, sizeof(struct OnRestartCmd));
    if (curServ->onRestart == NULL) {
        return SERVICE_FAILURE;
    }
    curServ->onRestart->cmdLine = (CmdLine *)calloc(cmdCnt, sizeof(CmdLine));
    if (curServ->onRestart->cmdLine == NULL) {
        return SERVICE_FAILURE;
    }
    curServ->onRestart->cmdNum = cmdCnt;
    for (int i = 0; i < cmdCnt; ++i) {
        cJSON* cmdJ = cJSON_GetArrayItem(filedJ, i);
        if (!cJSON_IsString(cmdJ) || !cJSON_GetStringValue(cmdJ)) {
            return SERVICE_FAILURE;
        }
        char *cmdStr = cJSON_GetStringValue(cmdJ);
        ParseCmdLine(cmdStr, &curServ->onRestart->cmdLine[i]);
        printf("[init] SetOnRestart cmdLine->name %s  cmdLine->cmdContent %s \n", curServ->onRestart->cmdLine[i].name,
                curServ->onRestart->cmdLine[i].cmdContent);
    }
    return SERVICE_SUCCESS;
}

static int CheckServiceKeyName(const cJSON* curService)
{
    char *cfgServiceKeyList[] = {"name", "path", "uid", "gid", "once",
        "importance", "caps", "disabled", "writepid", "critical", "socket",
    };
    if (curService == NULL) {
        return SERVICE_FAILURE;
    }
    cJSON *child = curService->child;
    if (child == NULL) {
        return SERVICE_FAILURE;
    }
    while (child) {
        int i = 0;
        int keyListSize = sizeof(cfgServiceKeyList) / sizeof(char *);
        for (; i < keyListSize; i++) {
            if (!strcmp(child->string, cfgServiceKeyList[i])) {
                break;
            }
        }
        if(i < keyListSize) {
            child = child->next;
        } else {
            INIT_LOGE("[Init] CheckServiceKeyName, key name %s is not found. error.\n", child->string);
            return SERVICE_FAILURE;
        }
    }
    return SERVICE_SUCCESS;
}

void ParseAllServices(const cJSON* fileRoot)
{
    int servArrSize = 0;
    cJSON* serviceArr = GetArrItem(fileRoot, &servArrSize, SERVICES_ARR_NAME_IN_JSON);
    if (serviceArr == NULL) {
        printf("[Init] InitReadCfg, get array %s failed.\n", SERVICES_ARR_NAME_IN_JSON);
        return;
    }

    printf("[Init] servArrSize is %d \n", servArrSize);
    if (servArrSize > MAX_SERVICES_CNT_IN_FILE) {
        printf("[Init] InitReadCfg, too many services[cnt %d] detected, should not exceed %d.\n",
            servArrSize, MAX_SERVICES_CNT_IN_FILE);
        return;
    }

    Service* retServices = (Service*)realloc(g_services, sizeof(Service) * (g_servicesCnt + servArrSize));
    if (retServices == NULL) {
        printf("[Init] InitReadCfg, realloc for %s arr failed! %d.\n", SERVICES_ARR_NAME_IN_JSON, servArrSize);
        return;
    }
    // Skip already saved services,
    Service* tmp = retServices + g_servicesCnt;
    if (memset_s(tmp, sizeof(Service) * servArrSize, 0, sizeof(Service) * servArrSize) != EOK) {
        free(retServices);
        retServices = NULL;
        return;
    }

    for (int i = 0; i < servArrSize; ++i) {
        cJSON* curItem = cJSON_GetArrayItem(serviceArr, i);
        if (CheckServiceKeyName(curItem) != SERVICE_SUCCESS) {
            ReleaseServiceMem(&tmp[i]);
            tmp[i].attribute |= SERVICE_ATTR_INVALID;
            continue;
        }
        tmp[i].servPerm.uID = (uid_t)(-1);           // set 0xffffffff as init value
        int ret1 = GetServiceName(curItem, &tmp[i]);
        int ret2 = GetServicePathAndArgs(curItem, &tmp[i]);
        int ret3 = GetUidStringNumber(curItem, &tmp[i]);                        // uid in string or number form
        int ret4 = GetGidArray(curItem, &tmp[i]);                               // gid array
        int ret5 = GetServiceNumber(curItem, &tmp[i], ONCE_STR_IN_CFG);
        int ret6 = GetServiceNumber(curItem, &tmp[i], IMPORTANT_STR_IN_CFG);
        int ret7 = GetServiceNumber(curItem, &tmp[i], CRITICAL_STR_IN_CFG);     // critical
        int ret8 = GetServiceNumber(curItem, &tmp[i], DISABLED_STR_IN_CFG);     // disabled
        int ret9 = GetWritepidStrings(curItem, &tmp[i]);                        // writepid
        int reta = GetServiceCaps(curItem, &tmp[i]);
        int retAll = ret1 | ret2 | ret3 | ret4 | ret5 | ret6 | ret7 | ret8 | ret9 | reta;
        if (retAll != SERVICE_SUCCESS) {
            // release resources if it fails
            ReleaseServiceMem(&tmp[i]);
            tmp[i].attribute |= SERVICE_ATTR_INVALID;
            printf("[Init] InitReadCfg, parse information for service %d failed. ", i);
            printf("service name = [%s]\n", tmp[i].name);
            printf("ret1=%d,ret2=%d,ret3=%d,ret4=%d,ret5=%d,ret6=%d,ret7=%d,ret8=%d,ret9=%d,reta=%d,",
                ret1, ret2, ret3, ret4, ret5, ret6, ret7, ret8, ret9, reta);
            continue;
        } else {
            printf("[init] ParseAllServices ParseAllServices Service[%d] name=%s, uid=%d, critical=%d, disabled=%d\n",
                 i, tmp[i].name, tmp[i].servPerm.uID, tmp[i].attribute & SERVICE_ATTR_CRITICAL ? 1 : 0,
                 tmp[i].attribute & SERVICE_ATTR_DISABLED ? 1 : 0);
            for(int j = 0; j < tmp[i].servPerm.gIDCnt; j++) {
                 printf("\t\tgIDArray[%d] = %d\n", j, tmp[i].servPerm.gIDArray[j]);
            }
            for(int j = 0; j < MAX_WRITEPID_FILES; j++) {
                 if(tmp[i].writepidFiles[j])
                     printf("\t\twritepidFiles[%d] = %s\n", j, tmp[i].writepidFiles[j]);
            }
        }
        if (GetServiceSocket(curItem, &tmp[i]) != SERVICE_SUCCESS) {
            // free list
        }
        if (GetServiceOnRestart(curItem, &tmp[i]) != SERVICE_SUCCESS) {
            // free
        }
    }
    // Increase service counter.
    RegisterServices(retServices, servArrSize);
}

static int FindServiceByName(const char* servName)
{
    if (servName == NULL) {
        return -1;
    }

    for (int i = 0; i < g_servicesCnt; ++i) {
        if (strlen(g_services[i].name) == strlen(servName) &&
            strncmp(g_services[i].name, servName, strlen(g_services[i].name)) == 0) {
            return i;
        }
    }
    return -1;
}

void StartServiceByName(const char* servName)
{
    // find service by name
    int servIdx = FindServiceByName(servName);
    if (servIdx < 0) {
        printf("[Init] StartServiceByName, cannot find service %s.\n", servName);
        return;
    }

    if (ServiceStart(&g_services[servIdx]) != SERVICE_SUCCESS) {
        printf("[Init] StartServiceByName, service %s start failed!\n", g_services[servIdx].name);
    }

    return;
}

void StopServiceByName(const char* servName)
{
    // find service by name
    int servIdx = FindServiceByName(servName);
    if (servIdx < 0) {
        INIT_LOGE("[Init] StopServiceByName, cannot find service %s.\n", servName);
        return;
    }

    if (ServiceStop(&g_services[servIdx]) != SERVICE_SUCCESS) {
        INIT_LOGE("[Init] StopServiceByName, service %s start failed!\n", g_services[servIdx].name);
    }

    return;
}

void StopAllServices()
{
    for (int i = 0; i < g_servicesCnt; i++) {
        if (ServiceStop(&g_services[i]) != SERVICE_SUCCESS) {
            printf("[Init] StopAllServices, service %s stop failed!\n", g_services[i].name);
        }
    }
}

void ReapServiceByPID(int pid)
{
    for (int i = 0; i < g_servicesCnt; i++) {
        if (g_services[i].pid == pid) {
            if (g_services[i].attribute & SERVICE_ATTR_IMPORTANT) {
                // important process exit, need to reboot system
                g_services[i].pid = -1;
                StopAllServices();
                RebootSystem();
            }
            ServiceReap(&g_services[i]);
            break;
        }
    }
}


