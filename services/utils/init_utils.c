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
#include "init_utils.h"

#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "init_log.h"
#include "securec.h"

#define MAX_BUF_SIZE  1024
#define MAX_DATA_BUFFER 2048

#ifdef STARTUP_UT
#define LOG_FILE_NAME "/media/sf_ubuntu/test/log.txt"
#else
#define LOG_FILE_NAME "/data/startup_log.txt"
#endif

#define MAX_JSON_FILE_LEN 102400    // max init.cfg size 100KB
#define CONVERT_MICROSEC_TO_SEC(x) ((x) / 1000 / 1000.0)
#ifndef DT_DIR
#define DT_DIR 4
#endif

#define THOUSAND_UNIT_INT 1000
#define THOUSAND_UNIT_FLOAT 1000.0

float ConvertMicrosecondToSecond(int x)
{
    return ((x / THOUSAND_UNIT_INT) / THOUSAND_UNIT_FLOAT);
}

uid_t DecodeUid(const char *name)
{
    INIT_CHECK_RETURN_VALUE(name != NULL, -1);
    int digitFlag = 1;
    for (unsigned int i = 0; i < strlen(name); ++i) {
        if (isalpha(name[i])) {
            digitFlag = 0;
            break;
        }
    }
    if (digitFlag) {
        errno = 0;
        uid_t result = strtoul(name, 0, DECIMAL_BASE);
        INIT_CHECK_RETURN_VALUE(errno == 0, -1);
        return result;
    } else {
        struct passwd *userInf = getpwnam(name);
        if (userInf == NULL) {
            return -1;
        }
        return userInf->pw_uid;
    }
}

char *ReadFileToBuf(const char *configFile)
{
    char *buffer = NULL;
    FILE *fd = NULL;
    struct stat fileStat = {0};
    INIT_CHECK_RETURN_VALUE(configFile != NULL && *configFile != '\0', NULL);
    do {
        if (stat(configFile, &fileStat) != 0 ||
            fileStat.st_size <= 0 || fileStat.st_size > MAX_JSON_FILE_LEN) {
            INIT_LOGE("Unexpected config file \" %s \", check if it exist. if exist, check file size", configFile);
            break;
        }
        fd = fopen(configFile, "r");
        if (fd == NULL) {
            INIT_LOGE("Open %s failed. err = %d", configFile, errno);
            break;
        }
        buffer = (char*)malloc((size_t)(fileStat.st_size + 1));
        if (buffer == NULL) {
            INIT_LOGE("Failed to allocate memory for config file, err = %d", errno);
            break;
        }

        if (fread(buffer, fileStat.st_size, 1, fd) != 1) {
            free(buffer);
            buffer = NULL;
            break;
        }
        buffer[fileStat.st_size] = '\0';
    } while (0);

    if (fd != NULL) {
        (void)fclose(fd);
        fd = NULL;
    }
    return buffer;
}

char *ReadFileData(const char *fileName)
{
    if (fileName == NULL) {
        return NULL;
    }
    char *buffer = NULL;
    int fd = -1;
    do {
        fd = open(fileName, O_RDONLY);
        INIT_ERROR_CHECK(fd >= 0, break, "Failed to read file %s", fileName);

        buffer = (char *)malloc(MAX_DATA_BUFFER); // fsmanager not create, can not get fileStat st_size
        INIT_ERROR_CHECK(buffer != NULL, break, "Failed to allocate memory for %s", fileName);
        ssize_t readLen = read(fd, buffer, MAX_DATA_BUFFER - 1);
        INIT_ERROR_CHECK(readLen > 0, break, "Failed to read data for %s", fileName);
        buffer[readLen] = '\0';
    } while (0);
    if (fd != -1) {
        close(fd);
    }
    return buffer;
}


int GetProcCmdlineValue(const char *name, const char *buffer, char *value, int length)
{
    INIT_ERROR_CHECK(name != NULL && buffer != NULL && value != NULL, return -1, "Failed get parameters");
    char *endData = (char *)buffer + strlen(buffer);
    char *tmp = strstr(buffer, name);
    do {
        if (tmp == NULL) {
            return -1;
        }
        tmp = tmp + strlen(name);
        while (tmp < endData && *tmp == ' ') {
            tmp++;
        }
        if (*tmp == '=') {
            break;
        }
        tmp = strstr(tmp + 1, name);
    } while (tmp < endData);
    tmp++;
    size_t i = 0;
    size_t endIndex = 0;
    while (tmp < endData && *tmp == ' ') {
        tmp++;
    }
    for (; i < (size_t)length; tmp++) {
        if (tmp >= endData) {
            endIndex = i;
            break;
        }
        if (*tmp == ' ') {
            endIndex = i;
        }
        if (*tmp == '=') {
            if (endIndex != 0) { // for root=uuid=xxxx
                break;
            }
            i = 0;
            endIndex = 0;
            continue;
        }
        value[i++] = *tmp;
    }
    if (i >= (size_t)length) {
        return -1;
    }
    value[endIndex] = '\0';
    return 0;
}

int SplitString(char *srcPtr, const char *del, char **dstPtr, int maxNum)
{
    INIT_CHECK_RETURN_VALUE(srcPtr != NULL && dstPtr != NULL && del != NULL, -1);
    char *buf = NULL;
    dstPtr[0] = strtok_r(srcPtr, del, &buf);
    int counter = 0;
    while (dstPtr[counter] != NULL && (counter < maxNum)) {
        counter++;
        if (counter >= maxNum) {
            break;
        }
        dstPtr[counter] = strtok_r(NULL, del, &buf);
    }
    return counter;
}

void FreeStringVector(char **vector, int count)
{
    if (vector != NULL) {
        for (int i = 0; i < count; i++) {
            if (vector[i] != NULL) {
                free(vector[i]);
            }
        }
        free(vector);
    }
}

char **SplitStringExt(char *buffer, const char *del, int *returnCount, int maxItemCount)
{
    INIT_CHECK_RETURN_VALUE((maxItemCount >= 0) && (buffer != NULL) && (del != NULL) && (returnCount != NULL), NULL);
    // Why is this number?
    // Now we use this function to split a string with a given delimeter
    // We do not know how many sub-strings out there after splitting.
    // 50 is just a guess value.
    const int defaultItemCounts = 50;
    int itemCounts = maxItemCount;

    if (maxItemCount > defaultItemCounts) {
        itemCounts = defaultItemCounts;
    }
    char **items = (char **)malloc(sizeof(char*) * itemCounts);
    INIT_ERROR_CHECK(items != NULL, return NULL, "No enough memory to store items");
    char *rest = NULL;
    char *p = strtok_r(buffer, del, &rest);
    int count = 0;
    while (p != NULL) {
        if (count > itemCounts - 1) {
            itemCounts += (itemCounts / 2) + 1; // 2 Request to increase the original memory by half.
            INIT_LOGD("Too many items,expand size");
            char **expand = (char **)(realloc(items, sizeof(char *) * itemCounts));
            INIT_ERROR_CHECK(expand != NULL, FreeStringVector(items, count);
                return NULL, "Failed to expand memory for uevent config parser");
            items = expand;
        }
        size_t len = strlen(p);
        items[count] = (char *)malloc(len + 1);
        INIT_CHECK(items[count] != NULL, FreeStringVector(items, count);
            return NULL);
        if (strncpy_s(items[count], len + 1, p, len) != EOK) {
            INIT_LOGE("Copy string failed");
            FreeStringVector(items, count);
            return NULL;
        }
        items[count][len] = '\0';
        count++;
        p = strtok_r(NULL, del, &rest);
    }
    *returnCount = count;
    return items;
}

void WaitForFile(const char *source, unsigned int maxCount)
{
    unsigned int maxCountTmp = maxCount;
    INIT_ERROR_CHECK(maxCountTmp <= WAIT_MAX_COUNT, maxCountTmp = WAIT_MAX_COUNT, "WaitForFile max time is 5s");
    struct stat sourceInfo = {};
    unsigned int waitTime = 500000;
    unsigned int count = 0;
    do {
        usleep(waitTime);
        count++;
    } while ((stat(source, &sourceInfo) < 0) && (errno == ENOENT) && (count < maxCountTmp));
    float secTime = ConvertMicrosecondToSecond(waitTime);
    INIT_CHECK_ONLY_ELOG(count != maxCountTmp, "wait for file:%s failed after %f.", source, maxCountTmp * secTime);
    return;
}

size_t WriteAll(int fd, const char *buffer, size_t size)
{
    INIT_CHECK_RETURN_VALUE(buffer != NULL && fd >= 0 && *buffer != '\0', 0);
    const char *p = buffer;
    size_t left = size;
    ssize_t written;
    while (left > 0) {
        do {
            written = write(fd, p, left);
        } while (written < 0 && errno == EINTR);
        if (written < 0) {
            INIT_LOGE("Failed to write %lu bytes, err = %d", left, errno);
            break;
        }
        p += written;
        left -= written;
    }
    return size - left;
}

char *GetRealPath(const char *source)
{
    INIT_CHECK_RETURN_VALUE(source != NULL, NULL);
    char *path = realpath(source, NULL);
    if (path == NULL) {
        INIT_ERROR_CHECK(errno == ENOENT, return NULL, "Failed to resolve %s real path err=%d", source, errno);
    }
    return path;
}

int MakeDir(const char *dir, mode_t mode)
{
    int rc = -1;
    if (dir == NULL || *dir == '\0') {
        errno = EINVAL;
        return rc;
    }
    rc = mkdir(dir, mode);
    INIT_ERROR_CHECK(!(rc < 0 && errno != EEXIST), return rc,
        "Create directory \" %s \" failed, err = %d", dir, errno);
    // create dir success or it already exist.
    return 0;
}

int MakeDirRecursive(const char *dir, mode_t mode)
{
    int rc = -1;
    char buffer[PATH_MAX] = {};
    const char *p = NULL;
    if (dir == NULL || *dir == '\0') {
        errno = EINVAL;
        return rc;
    }
    p = dir;
    char *slash = strchr(dir, '/');
    while (slash != NULL) {
        int gap = slash - p;
        p = slash + 1;
        if (gap == 0) {
            slash = strchr(p, '/');
            continue;
        }
        if (gap < 0) { // end with '/'
            break;
        }
        INIT_CHECK_RETURN_VALUE(memcpy_s(buffer, PATH_MAX, dir, p - dir - 1) == 0, -1);
        rc = MakeDir(buffer, mode);
        INIT_CHECK_RETURN_VALUE(rc >= 0, rc);
        slash = strchr(p, '/');
    }
    return MakeDir(dir, mode);
}

int StringToInt(const char *str, int defaultValue)
{
    if (str == NULL || *str == '\0') {
        return defaultValue;
    }
    errno = 0;
    int value = (int)strtoul(str, NULL, DECIMAL_BASE);
    return (errno != 0) ? defaultValue : value;
}

int ReadFileInDir(const char *dirPath, const char *includeExt,
    int (*processFile)(const char *fileName, void *context), void *context)
{
    INIT_CHECK_RETURN_VALUE(dirPath != NULL && processFile != NULL, -1);
    DIR *pDir = opendir(dirPath);
    INIT_ERROR_CHECK(pDir != NULL, return -1, "Read dir :%s failed.%d", dirPath, errno);
    char *fileName = malloc(MAX_BUF_SIZE);
    INIT_ERROR_CHECK(fileName != NULL, closedir(pDir);
        return -1, "Failed to malloc for %s", dirPath);

    struct dirent *dp;
    while ((dp = readdir(pDir)) != NULL) {
        if (dp->d_type == DT_DIR) {
            continue;
        }
        INIT_LOGD("ReadFileInDir %s", dp->d_name);
        if (includeExt != NULL) {
            char *tmp = strstr(dp->d_name, includeExt);
            if (tmp == NULL) {
                continue;
            }
            if (strcmp(tmp, includeExt) != 0) {
                continue;
            }
        }
        int ret = snprintf_s(fileName, MAX_BUF_SIZE, MAX_BUF_SIZE - 1, "%s/%s", dirPath, dp->d_name);
        if (ret <= 0) {
            INIT_LOGE("Failed to get file name for %s", dp->d_name);
            continue;
        }
        struct stat st;
        if (stat(fileName, &st) == 0) {
            processFile(fileName, context);
        }
    }
    free(fileName);
    closedir(pDir);
    return 0;
}

// Check if in updater mode.
int InUpdaterMode(void)
{
    const char * const updaterExecutabeFile = "/bin/updater";
    if (access(updaterExecutabeFile, X_OK) == 0) {
        return 1;
    } else {
        return 0;
    }
}

int StringReplaceChr(char *strl, char oldChr, char newChr)
{
    INIT_ERROR_CHECK(strl != NULL, return -1, "Invalid parament");
    char *p = strl;
    while (*p != '\0') {
        if (*p == oldChr) {
            *p = newChr;
        }
        p++;
    }
    INIT_LOGD("strl is %s", strl);
    return 0;
}
