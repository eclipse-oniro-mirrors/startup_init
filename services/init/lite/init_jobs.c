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
#include "init_jobs_internal.h"

#include <string.h>
#include <unistd.h>

#include "init_cmds.h"
#include "init_log.h"
#include "securec.h"

#define JOBS_ARR_NAME_IN_JSON "jobs"
#define CMDS_ARR_NAME_IN_JSON "cmds"
#define MAX_JOBS_COUNT 100

static Job *g_jobs = NULL;
static int g_jobCnt = 0;

void DumpAllJobs(void)
{
    INIT_LOGV("Start to dump all jobs...");
    for (int i = 0; i < g_jobCnt; i++) {
        INIT_LOGV("\tjob name: %s", g_jobs[i].name);
        if (g_jobs[i].cmdLines == NULL) {
            continue;
        }
        INIT_LOGV("\tlist all commands:");
        for (int j = 0; j < g_jobs[i].cmdLines->cmdNum; j++) {
            CmdLine *cmd = &g_jobs[i].cmdLines->cmds[j];
            INIT_LOGV("\t\tcommand: %s %s", GetCmdKey(cmd->cmdIndex), cmd->cmdContent);
        }
    }
    INIT_LOGV("Finish dump all jobs");
}

static int GetJobName(const cJSON *jobItem, Job *resJob)
{
    char *jobNameStr = cJSON_GetStringValue(cJSON_GetObjectItem(jobItem, "name"));
    if (jobNameStr == NULL) {
        return 0;
    }

    if (memcpy_s(resJob->name, MAX_JOB_NAME_LEN, jobNameStr, strlen(jobNameStr)) != EOK) {
        INIT_LOGE("Get job name \"%s\" failed", jobNameStr);
        return 0;
    }
    resJob->name[strlen(jobNameStr)] = '\0';
    return 1;
}

static void ParseJob(const cJSON *jobItem, Job *resJob)
{
    if (!GetJobName(jobItem, resJob)) {
        INIT_LOGE("get JobName failed");
        (void)memset_s(resJob, sizeof(*resJob), 0, sizeof(*resJob));
        return;
    }

    cJSON *cmdsItem = cJSON_GetObjectItem(jobItem, CMDS_ARR_NAME_IN_JSON);
    if (!cJSON_IsArray(cmdsItem)) {
        INIT_LOGE("job %s is not an array", resJob->name);
        return;
    }
    int ret = GetCmdLinesFromJson(cmdsItem, &resJob->cmdLines);
    if (ret != 0) {
        INIT_LOGE("ParseJob, failed to get cmds for job!");
        return;
    }
    return;
}

void ParseAllJobs(const cJSON *fileRoot)
{
    if (fileRoot == NULL) {
        INIT_LOGE("ParseAllJobs, input fileRoot is NULL!");
        return;
    }

    cJSON *jobArr = cJSON_GetObjectItemCaseSensitive(fileRoot, JOBS_ARR_NAME_IN_JSON);
    if (!cJSON_IsArray(jobArr)) {
        INIT_LOGE("ParseAllJobs, job item is not array!");
        return;
    }

    int jobArrSize = cJSON_GetArraySize(jobArr);
    if (jobArrSize <= 0 || jobArrSize > MAX_JOBS_COUNT) {
        INIT_LOGE("ParseAllJobs, jobs count %d is invalid, should be positive and not exceeding %d.",
            jobArrSize, MAX_JOBS_COUNT);
        return;
    }

    Job *retJobs = (Job *)realloc(g_jobs, sizeof(Job) * (g_jobCnt + jobArrSize));
    if (retJobs == NULL) {
        INIT_LOGE("ParseAllJobs, malloc failed! job arrSize %d.", jobArrSize);
        return;
    }

    Job *tmp = retJobs + g_jobCnt;
    if (memset_s(tmp, sizeof(Job) * jobArrSize, 0, sizeof(Job) * jobArrSize) != EOK) {
        INIT_LOGE("ParseAllJobs, memset_s failed.");
        free(retJobs);
        retJobs = NULL;
        return;
    }

    for (int i = 0; i < jobArrSize; ++i) {
        cJSON *jobItem = cJSON_GetArrayItem(jobArr, i);
        ParseJob(jobItem, &(tmp[i]));
    }
    g_jobs = retJobs;
    g_jobCnt += jobArrSize;
}

void DoJob(const char *jobName)
{
    if (jobName == NULL) {
        INIT_LOGE("DoJob, input jobName NULL!");
        return;
    }

    INIT_LOGV("Call job with name %s", jobName);
    for (int i = 0; i < g_jobCnt; ++i) {
        if (strncmp(jobName, g_jobs[i].name, strlen(g_jobs[i].name)) == 0) {
            CmdLines *cmdLines = g_jobs[i].cmdLines;
            if (cmdLines == NULL) {
                continue;
            }
            for (int j = 0; j < cmdLines->cmdNum; ++j) {
                DoCmdByIndex(cmdLines->cmds[j].cmdIndex, cmdLines->cmds[j].cmdContent);
            }
        }
    }
}

void ReleaseAllJobs(void)
{
    if (g_jobs == NULL) {
        return;
    }

    for (int i = 0; i < g_jobCnt; ++i) {
        if (g_jobs[i].cmdLines != NULL) {
            free(g_jobs[i].cmdLines);
        }
    }

    free(g_jobs);
    g_jobs = NULL;
    g_jobCnt = 0;
}

int DoJobNow(const char *jobName)
{
    return 0;
}
