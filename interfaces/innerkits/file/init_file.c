/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "init_file.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "init_log.h"
#include "init_utils.h"
#include "securec.h"

#define N_DEC 10

int GetControlFile(const char *pathName)
{
    if (pathName == NULL) {
        return -1;
    }
    char *name = (char *)calloc(1, strlen(pathName) + 1);
    INIT_CHECK(name != NULL, return -1);
    if (strncpy_s(name, strlen(pathName) + 1, pathName, strlen(pathName)) < 0) {
        INIT_LOGE("Failed strncpy_s err=%d", errno);
        free(name);
        return -1;
    }
    if (StringReplaceChr(name, '/', '_') < 0) {
        free(name);
        return -1;
    }
    char path[PATH_MAX] = { 0 };
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, OHOS_FILE_ENV_PREFIX"%s", name) == -1) {
        free(name);
        return -1;
    }
    free(name);
    INIT_LOGI("Environment path is %s ", path);
    const char *val = getenv(path);
    if (val == NULL) {
        INIT_LOGE("Failed getenv err=%d", errno);
        return -1;
    }
    errno = 0;
    int fd = strtol(val, NULL, N_DEC);
    if (errno != 0) {
        INIT_LOGE("Failed strtol val");
        return -1;
    }
    INIT_LOGI("Get environment fd is %d ", fd);
    if (fcntl(fd, F_GETFD) < 0) {
        INIT_LOGE("Failed fcntl fd err=%d ", errno);
        return -1;
    }
    return fd;
}
