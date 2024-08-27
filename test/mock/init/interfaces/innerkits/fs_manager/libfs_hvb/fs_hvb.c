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

#include "fs_hvb.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

int FsHvbGetValueFromCmdLine(char *val, size_t size, const char *key)
{
    char *swtypeValue = getenv("SWTYPE_VALUE");
    char *enableValue = getenv("ENABLE_VALUE");
    errno_t err;

    printf("[Replace]:FsHvbGetValueFromCmdLine in\n");

    if (strcmp(key, "ohos.boot.hvb.oem_swtype") == 0) {
        if (swtypeValue == NULL) {
            return -1;
        }

        err = memcpy_s(val, size, swtypeValue, strlen(swtypeValue));
        if (err != EOK) {
            return -1;
        }
        return 0;
    }

    if (strcmp(key, "ohos.boot.hvb.enable") == 0) {
        if (enableValue == NULL) {
            return -1;
        }

        err = memcpy_s(val, size, enableValue, strlen(enableValue));
        if (err != EOK) {
            return -1;
        }
        return 0;
    }

    return 0;
}

int FsHvbInit(void)
{
    char *initValue = getenv("INIT_VALUE");
    printf("[Replace]:FsHvbInit in\n");

    if (!initValue) {
        return -1;
    }

    return 0;
}

int FsHvbFinal(void)
{
    char *finalValue = getenv("FINAL_VALUE");
    printf("[Replace]:FsHvbFinal in\n");

    if (!finalValue) {
        return -1;
    }

    return 0;
}

int FsHvbSetupHashtree(FstabItem *fsItem)
{
    char *hashValue = getenv("HASH_VALUE");
    printf("[Replace]:FsHvbSetupHashtree in\n");

    if (!hashValue) {
        return -1;
    }

    return 0;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif