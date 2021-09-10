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
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "init_log.h"
#include "init_utils.h"
#include "securec.h"

#define WAIT_MAX_COUNT 10
#define MAX_BUF_SIZE  1024
#ifdef STARTUP_UT
#define LOG_FILE_NAME "/media/sf_ubuntu/test/log.txt"
#else
#define LOG_FILE_NAME "/data/startup_log.txt"
#endif

#define MAX_JSON_FILE_LEN 102400    // max init.cfg size 100KB
#define CONVERT_MICROSEC_TO_SEC(x) ((x) / 1000 / 1000.0)

int DecodeUid(const char *name)
{
    bool digitFlag = true;
    for (unsigned int i = 0; i < strlen(name); ++i) {
        if (isalpha(name[i])) {
            digitFlag = false;
            break;
        }
    }
    if (digitFlag) {
        errno = 0;
        uid_t result = strtoul(name, 0, 10);
        if (errno != 0) {
            return -1;
        }
        return result;
    } else {
        struct passwd *pwd = getpwnam(name);
        if (!pwd) {
            return -1;
        }
        return pwd->pw_uid;
    }
}

void CheckAndCreateDir(const char *fileName)
{
    char *path = strndup(fileName, strrchr(fileName, '/') - fileName);
    if (path != NULL && access(path, F_OK) != 0) {
        mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP);
    }
    free(path);
}

char* ReadFileToBuf(const char *configFile)
{
    char* buffer = NULL;
    FILE* fd = NULL;
    struct stat fileStat = {0};
    if (configFile == NULL || *configFile == '\0') {
        return NULL;
    }

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
        buffer = (char*)malloc(fileStat.st_size + 1);
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
        fclose(fd);
        fd = NULL;
    }
    return buffer;
}

int SplitString(char *srcPtr, char **dstPtr, int maxNum)
{
    if ((!srcPtr) || (!dstPtr)){
        return -1;
    }
    char *buf = NULL;
    dstPtr[0] = strtok_r(srcPtr, " ", &buf);
    int i = 0;
    while (dstPtr[i] != NULL && (i < maxNum)) {
        i++;
        dstPtr[i] = strtok_r(NULL, " ", &buf);
    }
    dstPtr[i] = "\0";
    int num = i;
    for (int j = 0; j < num; j++) {
        INIT_LOGI("dstPtr[%d] is %s ", j, dstPtr[j]);
    }
    return num;
}

void WaitForFile(const char *source, unsigned int maxCount)
{
    if (maxCount > WAIT_MAX_COUNT) {
        INIT_LOGE("WaitForFile max time is 5s");
        maxCount = WAIT_MAX_COUNT;
    }
    struct stat sourceInfo;
    unsigned int waitTime = 500000;
    unsigned int count = 0;
    do {
        usleep(waitTime);
        count++;
    } while ((stat(source, &sourceInfo) < 0) && (errno == ENOENT) && (count < maxCount));
    if (count == maxCount) {
        INIT_LOGE("wait for file:%s failed after %f.", source, maxCount * CONVERT_MICROSEC_TO_SEC(waitTime));
    }
    return;
}

