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
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "init_log.h"
#include "securec.h"
#include "service_control.h"

#define MAX_BUF_SIZE  1024
#define MAX_SMALL_BUFFER 3096

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

#ifndef __LITEOS_M__
static bool CheckDigit(const char *name)
{
    size_t nameLen = strlen(name);
    for (size_t i = 0; i < nameLen; ++i) {
        if (!isdigit(name[i])) {
            return false;
        }
    }
    return true;
}
#endif

int StringToUint(const char *name, unsigned int *value)
{
    errno = 0;
    *value = (unsigned int)strtoul(name, 0, DECIMAL_BASE);
    INIT_CHECK_RETURN_VALUE(errno == 0, -1);
    return 0;
}

uid_t DecodeUid(const char *name)
{
#ifndef __LITEOS_M__
    INIT_CHECK_RETURN_VALUE(name != NULL, -1);
    uid_t uid = -1;
    if (CheckDigit(name)) {
        if (!StringToUint(name, &uid)) {
            return uid;
        } else {
            INIT_LOGE("Failed to decode uid for %s", name);
            return -1;
        }
    }
    struct passwd *p = getpwnam(name);
    if (p == NULL) {
        INIT_LOGE("Failed to decode uid for %s", name);
        return -1;
    }
    return p->pw_uid;
#else
    (void)name;
    return -1;
#endif
}

gid_t DecodeGid(const char *name)
{
#ifndef __LITEOS_M__
    INIT_CHECK_RETURN_VALUE(name != NULL, -1);
    gid_t gid = -1;
    if (CheckDigit(name)) {
        if (!StringToUint(name, &gid)) {
            return gid;
        } else {
            INIT_LOGE("Failed to decode gid for %s", name);
            return -1;
        }
    }
    struct group *data = getgrnam(name);
    if (data != NULL) {
        return data->gr_gid;
    }
    while ((data = getgrent()) != NULL) {
        if ((data->gr_name != NULL) && (strcmp(data->gr_name, name) == 0)) {
            gid = data->gr_gid;
            break;
        }
    }
    endgrent();
    return gid;
#else
    (void)name;
    return -1;
#endif
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

void CloseStdio(void)
{
#ifndef __LITEOS_M__
    int fd = open("/dev/null", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return;
    }
    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, STDERR_HANDLE);
    close(fd);
#endif
}

char *ReadFileData(const char *fileName)
{
    if (fileName == NULL) {
        return NULL;
    }
    char *buffer = NULL;
    int fd = -1;
    fd = open(fileName, O_RDONLY);
    INIT_ERROR_CHECK(fd >= 0, return NULL, "Failed to read file %s", fileName);
    buffer = (char *)calloc(1, MAX_SMALL_BUFFER); // fsmanager not create, can not get fileStat st_size
    INIT_ERROR_CHECK(buffer != NULL, close(fd);
        return NULL, "Failed to allocate memory for %s", fileName);
    ssize_t readLen = read(fd, buffer, MAX_SMALL_BUFFER - 1);
    INIT_ERROR_CHECK((readLen > 0) && (readLen < (MAX_SMALL_BUFFER - 1)), close(fd);
        free(buffer);
        return NULL, "Failed to read data for %s", fileName);
    buffer[readLen] = '\0';
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
        if (*tmp == ' ' || *tmp == '\n' || *tmp == '\r' || *tmp == '\t') {
            endIndex = i;
            break;
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
    while ((counter < maxNum) && (dstPtr[counter] != NULL)) {
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
    // Now we use this function to split a string with a given delimiter
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
            INIT_LOGV("Too many items,expand size");
            char **expand = (char **)(realloc(items, sizeof(char *) * itemCounts));
            INIT_ERROR_CHECK(expand != NULL, FreeStringVector(items, count);
                return NULL, "Failed to expand memory");
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

void WaitForFile(const char *source, unsigned int maxSecond)
{
    INIT_ERROR_CHECK(maxSecond <= WAIT_MAX_SECOND, maxSecond = WAIT_MAX_SECOND, "WaitForFile max time is 5s");
    struct stat sourceInfo = {0};
    unsigned int waitTime = 500000;
    /* 500ms interval, check maxSecond*2 times total */
    unsigned int maxCount = maxSecond * 2;
#ifdef STARTUP_INIT_TEST
    maxCount = 0;
#endif
    unsigned int count = 0;
    while ((stat(source, &sourceInfo) < 0) && (errno == ENOENT) && (count < maxCount)) {
        usleep(waitTime);
        count++;
    }
    INIT_CHECK_ONLY_ELOG(count != maxCount, "wait for file:%s failed after %d second.", source, maxSecond);
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
    char buffer[PATH_MAX] = {0};
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

void CheckAndCreateDir(const char *fileName)
{
#ifndef __LITEOS_M__
    if (fileName == NULL || *fileName == '\0') {
        return;
    }
    char *path = strndup(fileName, strrchr(fileName, '/') - fileName);
    if (path == NULL) {
        return;
    }
    if (access(path, F_OK) == 0) {
        free(path);
        return;
    }
    MakeDirRecursive(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    free(path);
#else
    (void)fileName;
#endif
}

int CheckAndCreatFile(const char *file, mode_t mode)
{
    if (access(file, F_OK) == 0) {
        BEGET_LOGW("File \' %s \' already exist", file);
        return 0;
    } else {
        if (errno == ENOENT) {
            CheckAndCreateDir(file);
            int fd = open(file, O_CREAT, mode);
            if (fd < 0) {
                BEGET_LOGE("Failed create %s, err=%d", file, errno);
                return -1;
            } else {
                BEGET_LOGI("Success create %s", file);
                close(fd);
            }
        } else {
            BEGET_LOGW("Failed to access \' %s \', err = %d", file, errno);
            return -1;
        }
    }
    return 0;
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
    char *fileName = calloc(1, MAX_BUF_SIZE);
    INIT_ERROR_CHECK(fileName != NULL, closedir(pDir);
        return -1, "Failed to malloc for %s", dirPath);

    struct dirent *dp;
    uint32_t count = 0;
    while ((dp = readdir(pDir)) != NULL) {
        if (dp->d_type == DT_DIR) {
            continue;
        }
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
            count++;
            processFile(fileName, context);
        }
    }
    INIT_LOGV("ReadFileInDir dirPath %s %d", dirPath, count);
    free(fileName);
    closedir(pDir);
    return 0;
}

// Check if in updater mode.
int InUpdaterMode(void)
{
#ifdef OHOS_LITE
    return 0;
#else
    const char * const updaterExecutableFile = "/bin/updater";
    if (access(updaterExecutableFile, X_OK) == 0) {
        return 1;
    } else {
        return 0;
    }
#endif
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
    INIT_LOGV("strl is %s", strl);
    return 0;
}

void RedirectStdio(int fd)
{
#ifndef __LITEOS_M__
    const int stdError = 2;
    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, stdError); // Redirect fd to 0, 1, 2
#endif
}

void OpenConsole(void)
{
#ifndef __LITEOS_M__
    setsid();
    WaitForFile("/dev/console", WAIT_MAX_SECOND);
    int fd = open("/dev/console", O_RDWR);
    if (fd >= 0) {
        ioctl(fd, TIOCSCTTY, 0);
        RedirectStdio(fd);
        close(fd);
    } else {
        INIT_LOGE("Open /dev/console failed. err = %d", errno);
    }
    return;
#endif
}

INIT_LOCAL_API int StringToLL(const char *str, long long int *out)
{
    INIT_ERROR_CHECK(str != NULL && out != NULL, return -1, "Invalid parament");
    const char *s = str;
    while (isspace(*s)) {
        s++;
    }

    size_t len = strlen(str);
    int positiveHex = (len > 1 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'));
    int negativeHex = (len > 2 && s[0] == '-' && s[1] == '0' && (s[2] == 'x' || s[2] == 'X')); // 2: shorttest
    int base = (positiveHex || negativeHex) ? HEX_BASE : DECIMAL_BASE;
    char *end = NULL;
    errno = 0;
    *out = strtoll(s, &end, base);
    if (errno != 0) {
        INIT_LOGE("StringToLL %s err = %d", str, errno);
        return -1;
    }
    BEGET_CHECK(!(s == end || *end != '\0'), return -1);
    return 0;
}

INIT_LOCAL_API int StringToULL(const char *str, unsigned long long int *out)
{
    INIT_ERROR_CHECK(str != NULL && out != NULL, return -1, "Invalid parament");
    const char *s = str;
    while (isspace(*s)) {
        s++;
    }
    BEGET_CHECK(s[0] != '-', return -1);
    int base = (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) ? HEX_BASE : DECIMAL_BASE;
    char *end = NULL;
    errno = 0;
    *out = strtoull(s, &end, base);
    if (errno != 0) {
        INIT_LOGE("StringToULL %s err = %d", str, errno);
        return -1;
    }
    BEGET_CHECK(end != s, return -1);
    BEGET_CHECK(*end == '\0', return -1);
    return 0;
}

void TrimTail(char *str, char c)
{
    char *end = str + strlen(str) - 1;
    while (end >= str && *end == c) {
        *end = '\0';
        end--;
    }
}

char *TrimHead(char *str, char c)
{
    char *head = str;
    const char *end = str + strlen(str);
    while (head < end && *head == c) {
        *head = '\0';
        head++;
    }
    return head;
}
