/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs_dm.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

int FsDmInitDmDev(char *devPath, bool useSocket)
{
    char *initValue = getenv("FSDM_VALUE");
    printf("[Replace]:FsDmInitDmDev in\n");

    if ((initValue == NULL) || (strcmp(initValue, "InitFail") == 0)) {
        return -1;
    }

    return 0;
}

int FsDmCreateDevice(char **dmDevPath, const char *devName, DmVerityTarget *target)
{
    char *createValue = getenv("FSDM_VALUE");

    printf("[Replace]:FsDmCreateDevice in\n");

    if ((createValue == NULL) || (strcmp(createValue, "CreateFail") == 0)) {
        return -1;
    }

    return 0;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif