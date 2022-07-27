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

#include "beget_ext.h"
#include "init_utils.h"
#include "securec.h"

#define N_DEC 10

int GetControlFile(const char *pathName)
{
    if (pathName == NULL) {
        return -1;
    }
    char path[PATH_MAX] = { 0 };
    BEGET_ERROR_CHECK(snprintf_s(path, sizeof(path), sizeof(path) - 1, OHOS_FILE_ENV_PREFIX "%s", pathName) >= 0,
        return -1, "Failed snprintf_s err=%d", errno);
    BEGET_ERROR_CHECK(StringReplaceChr(path, '/', '_') == 0,
        return -1, "Failed string replace char");
    BEGET_LOGI("Environment path is %s ", path);
    const char *val = getenv(path);
    BEGET_ERROR_CHECK(val != NULL, return -1, "Failed getenv err=%d", errno);
    errno = 0;
    int fd = strtol(val, NULL, N_DEC);
    BEGET_ERROR_CHECK(errno == 0, return -1, "Failed strtol val");
    BEGET_LOGI("Get environment fd is %d ", fd);
    BEGET_ERROR_CHECK(fcntl(fd, F_GETFD) >= 0, return -1, "Failed fcntl fd err=%d ", errno);
    return fd;
}
