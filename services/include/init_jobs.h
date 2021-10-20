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
#ifndef BASE_STARTUP_INITLITE_JOBS_H
#define BASE_STARTUP_INITLITE_JOBS_H

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define MAX_CMD_NAME_LEN 32
#define MAX_CMD_CNT_IN_ONE_JOB 200
#define MAX_CMD_CONTENT_LEN 256
#define MAX_JOB_NAME_LEN 64
// one cmd line
typedef struct {
    char name[MAX_CMD_NAME_LEN + 1]; // cmd name
    char cmdContent[MAX_CMD_CONTENT_LEN + 1]; // 256 cmd content
} CmdLine;

typedef struct {
    int cmdNum;
    CmdLine cmds[0];
} CmdLines;

// one job, could have many cmd lines
typedef struct {
    char name[MAX_JOB_NAME_LEN + 1];
    int cmdLinesCnt;
    CmdLine* cmdLines;
} Job;

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // BASE_STARTUP_INITLITE_JOBS_H
