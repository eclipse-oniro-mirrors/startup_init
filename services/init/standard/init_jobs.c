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
#include "init_group_manager.h"
#include "init_param.h"

static int CheckJobValid(const char *jobName)
{
    // check job in group
    return CheckNodeValid(NODE_TYPE_JOBS, jobName);
}

void ParseAllJobs(const cJSON *fileRoot)
{
    ParseTriggerConfig(fileRoot, CheckJobValid);
}

int DoJobNow(const char *jobName)
{
    DoJobExecNow(jobName);
    return 0;
}
