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
#ifndef BASE_STARTUP_INITLITE_JOBS_INTERNAL_H
#define BASE_STARTUP_INITLITE_JOBS_INTERNAL_H
#include "cJSON.h"
#include "init_cmds.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define MAX_JOB_NAME_LEN 64

// one job, could have many cmd lines
typedef struct {
    char name[MAX_JOB_NAME_LEN + 1];
    CmdLines *cmdLines;
} Job;

void ParseAllJobs(const cJSON *fileRoot);
void DoJob(const char *jobName);
void ReleaseAllJobs(void);
void DumpAllJobs(void);

int DoJobNow(const char *jobName);

#ifndef DISABLE_INIT_TWO_STAGES
#define INIT_CONFIGURATION_FILE "/etc/init.cfg"
#else
#define INIT_CONFIGURATION_FILE "/etc/init.without_two_stages.cfg"
#endif
#define OTHER_CFG_PATH "/system/etc/init"
#define OTHER_CHARGE_PATH "/vendor/etc/charger"
#define MAX_PATH_ARGS_CNT 20

void ReadConfig(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // BASE_STARTUP_INITLITE_JOBS_INTERNAL_H
