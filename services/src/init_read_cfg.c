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

#include "init_read_cfg.h"

#include <errno.h>
#include <dirent.h>
#include <linux/capability.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "init_import.h"
#include "init_jobs.h"
#include "init_log.h"
#include "init_perms.h"
#include "init_service_manager.h"
#include "init_utils.h"

#ifndef OHOS_LITE
#include "trigger.h"
#endif
#include "securec.h"
#ifndef __LINUX__
#ifdef OHOS_LITE
#include "init_stage.h"
#endif
#endif

#define FILE_NAME_MAX_SIZE 100
static void ParseInitCfgContents(cJSON *root)
{
     // parse services
    ParseAllServices(root);

    // parse jobs
    ParseAllJobs(root);

#ifndef OHOS_LITE
	ParseTriggerConfig(root);
#endif

    // parse imports
    ParseAllImports(root);
}

void ParseInitCfg(const char *configFile)
{
    if (configFile == NULL || *configFile == '\0') {
        printf("[Init] Invalid config file\n");
        return;
    }

    char *fileBuf = ReadFileToBuf(configFile);
    //printf("[Init] start dump config file: \n");
    //printf("%s", fileBuf);
    //printf("[Init] end dump config file: \n");

    cJSON* fileRoot = cJSON_Parse(fileBuf);
    free(fileBuf);
    fileBuf = NULL;

    if (fileRoot == NULL) {
        printf("[Init] InitReadCfg, parse failed! please check file %s format.\n", configFile);
        return;
    }
    ParseInitCfgContents(fileRoot);
    // Release JSON object
    cJSON_Delete(fileRoot);
    return;
}

static void ReadCfgs(const char *dirPath)
{
    DIR *pDir = opendir(dirPath);
    if (pDir == NULL) {
        INIT_LOGE("[Init], ParseCfgs open cfg dir :%s failed.%d\n", dirPath, errno);
        return;
    }
    struct dirent *dp;
    while ((dp = readdir(pDir)) != NULL) {
        char fileName[FILE_NAME_MAX_SIZE];
        if (snprintf_s(fileName, FILE_NAME_MAX_SIZE, FILE_NAME_MAX_SIZE - 1, "%s/%s", dirPath, dp->d_name) == -1) {
            INIT_LOGE("[Init], ParseCfgs snprintf_s failed.\n");
            closedir(pDir);
            return;
        }
        struct stat st;
        if (stat(fileName, &st) == 0) {
            if (strstr(dp->d_name, ".cfg") == NULL) {
                continue;
            }
            INIT_LOGE("[Init], fileName :%s.\n", fileName);
            ParseInitCfg(fileName);
        }
    }
    closedir(pDir);
    return;
}

static void ParseOtherCfgs()
{
    ReadCfgs("/system/etc/init");
    return;
}

void InitReadCfg()
{
    ParseInitCfg(INIT_CONFIGURATION_FILE);
    ParseOtherCfgs();
    printf("[init], Parse init config file done.\n");

    DumpAllServices();
    // DumpAllJobs();
    // do jobs
    DoJob("pre-init");
#ifndef __LINUX__
#ifdef OHOS_LITE
    TriggerStage(EVENT1, EVENT1_WAITTIME, QS_STAGE1);
#endif
#endif

    DoJob("init");
#ifndef __LINUX__
#ifdef OHOS_LITE
    TriggerStage(EVENT2, EVENT2_WAITTIME, QS_STAGE2);
#endif
#endif

    DoJob("post-init");
#ifndef __LINUX__
#ifdef OHOS_LITE
    TriggerStage(EVENT3, EVENT3_WAITTIME, QS_STAGE3);

    InitStageFinished();
#endif
#endif
    ReleaseAllJobs();
}

