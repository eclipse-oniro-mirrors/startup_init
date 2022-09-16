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
#include "init_service_file.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "init_log.h"
#include "init_service.h"
#include "init_utils.h"
#include "securec.h"

#define HOS_FILE_ENV_PREFIX "OHOS_FILE_"
#define MAX_FILE_FD_LEN 16

static int CreateFile(ServiceFile *file)
{
    INIT_CHECK(file != NULL && file->fileName != NULL, return -1);
    char path[PATH_MAX] = {0};
    if (realpath(file->fileName, path) == NULL) {
        INIT_LOGE("Failed realpath err=%d", errno);
        INIT_ERROR_CHECK(strncpy_s(path, strlen(file->fileName) + 1, file->fileName, strlen(file->fileName)) >= 0,
            return -1, "Failed strncpy_s err=%d", errno);
    }
    INIT_LOGV("File path =%s . file flags =%d, file perm =%u ", path, file->flags, file->perm);
    if (file->fd >= 0) {
        close(file->fd);
        file->fd = -1;
    }
    int fd = open(path, file->flags | O_CREAT, file->perm);
    INIT_ERROR_CHECK(fd >= 0, return -1, "Failed open %s, err=%d ", path, errno);
    close(fd);
    fd = -1;
    INIT_CHECK_ONLY_ELOG(chmod(path, file->perm) >= 0, "Failed chmod err=%d", errno);
    INIT_CHECK_ONLY_ELOG(chown(path, file->uid, file->gid) >= 0, "Failed chown err=%d", errno);
    file->fd = open(path, file->flags);
    return file->fd;
}

static int SetFileEnv(int fd, const char *pathName)
{
    INIT_ERROR_CHECK(pathName != NULL, return -1, "Invalid fileName");
    char pubName[PATH_MAX] = { 0 };
    INIT_ERROR_CHECK(snprintf_s(pubName, sizeof(pubName), sizeof(pubName) - 1,
        HOS_FILE_ENV_PREFIX "%s", pathName) >= 0, return -1, "Failed snprintf_s err=%d", errno);
    INIT_ERROR_CHECK(StringReplaceChr(pubName, '/', '_') >= 0, return -1, "Failed StringReplaceChr");
    char val[MAX_FILE_FD_LEN] = { 0 };
    INIT_ERROR_CHECK(snprintf_s(val, sizeof(val), sizeof(val) - 1, "%d", fd) >= 0, return -1,
        "Failed snprintf_s err=%d", errno);
    int ret = setenv(pubName, val, 1);
    INIT_ERROR_CHECK(ret >= 0, return -1, "Failed setenv err=%d ", errno);
    fcntl(fd, F_SETFD, 0);
    return 0;
}

void CreateServiceFile(ServiceFile *fileOpt)
{
    INIT_CHECK(fileOpt != NULL, return);
    ServiceFile *tmpFile = fileOpt;
    while (tmpFile != NULL) {
        int fd = CreateFile(tmpFile);
        if (fd < 0) {
            INIT_LOGE("Failed Create File err=%d ", errno);
            tmpFile = tmpFile->next;
            continue;
        }
        int ret = SetFileEnv(fd, tmpFile->fileName);
        INIT_CHECK_ONLY_ELOG(ret >= 0, "Failed Set File Env");
        tmpFile = tmpFile->next;
    }
    return;
}

void CloseServiceFile(ServiceFile *fileOpt)
{
    INIT_CHECK(fileOpt != NULL, return);
    ServiceFile *tmpFileOpt = fileOpt;
    while (tmpFileOpt != NULL) {
        ServiceFile *tmp = tmpFileOpt;
        if (tmp->fd >= 0) {
            close(tmp->fd);
            tmp->fd = -1;
        }
        tmpFileOpt = tmpFileOpt->next;
    }
    return;
}
