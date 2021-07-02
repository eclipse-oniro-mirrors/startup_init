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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "init_cmds.h"
#include "init_log.h"
#include "init_utils.h"
#include "securec.h"

#define MAX_BUF_SIZE  1024
#ifdef STARTUP_UT
#define LOG_FILE_NAME "/media/sf_ubuntu/test/log.txt"
#else
#define LOG_FILE_NAME "/data/startup_log.txt"
#endif
#define MAX_BUFFER 256
#define MAX_EACH_CMD_LENGTH 30
#define MAX_JSON_FILE_LEN 102400    // max init.cfg size 100KB

struct CmdArgs* GetCmd(const char *cmdContent, const char *delim)
{
    struct CmdArgs *ctx = (struct CmdArgs *)malloc(sizeof(struct CmdArgs));
    INIT_CHECK_ONLY_RETURN(ctx != NULL, return NULL);

    ctx->argv = (char**)malloc(sizeof(char*) * MAX_CMD_NAME_LEN);
    INIT_CHECK_ONLY_RETURN(ctx->argv != NULL, FreeCmd(&ctx); return NULL);

    char tmpCmd[MAX_BUFFER];
    INIT_CHECK_ONLY_RETURN(strncpy_s(tmpCmd, strlen(cmdContent) + 1, cmdContent, strlen(cmdContent)) == EOK,
        FreeCmd(&ctx);
        return NULL);
    tmpCmd[strlen(cmdContent)] = '\0';

    char *buffer = NULL;
    char *token = strtok_r(tmpCmd, delim, &buffer);
    ctx->argc = 0;
    while (token != NULL) {
        ctx->argv[ctx->argc] = calloc(sizeof(char *), MAX_EACH_CMD_LENGTH);
        INIT_CHECK_ONLY_RETURN(ctx->argv[ctx->argc] != NULL, FreeCmd(&ctx); return NULL);

        INIT_CHECK_ONLY_RETURN(strncpy_s(ctx->argv[ctx->argc], strlen(cmdContent) + 1, token, strlen(token)) == EOK,
            FreeCmd(&ctx);
            return NULL);
        if (ctx->argc > MAX_CMD_NAME_LEN - 1) {
            INIT_LOGE("GetCmd failed, max cmd number is 10.\n");
            FreeCmd(&ctx);
            return NULL;
        }
        token = strtok_r(NULL, delim, &buffer);
        ctx->argc += 1;
    }
    ctx->argv[ctx->argc] = NULL;
    return ctx;
}

void FreeCmd(struct CmdArgs **cmd)
{
    struct CmdArgs *tmpCmd = *cmd;
    INIT_CHECK_ONLY_RETURN(tmpCmd != NULL, return);
    for (int i = 0; i < tmpCmd->argc; ++i) {
        INIT_CHECK_ONLY_RETURN(tmpCmd->argv[i] == NULL, free(tmpCmd->argv[i]));
    }
    INIT_CHECK_ONLY_RETURN(tmpCmd->argv == NULL, free(tmpCmd->argv));
    free(tmpCmd);
    return;
}

void Logger(InitLogLevel level, const char *format, ...)
{
    FILE* pFile = fopen(LOG_FILE_NAME, "a");
    static char *logLeveInfo[] = { "VERBOSE", "INFO", "WARN", "ERROR", "FATAL" };
    if (level >= sizeof(logLeveInfo) / sizeof(char*) || pFile == NULL) {
        return;
    }
    time_t t;
    struct tm *localTimer;
    time(&t);
    localTimer = localtime (&t);
    fprintf(pFile, "[%d/%d/%d %d:%d:%d][%s]", localTimer->tm_year + 1900, localTimer->tm_mon, localTimer->tm_mday,
        localTimer->tm_hour, localTimer->tm_min, localTimer->tm_sec, logLeveInfo[level]);

    va_list list;
    va_start(list, format);
    vfprintf(pFile, format, list);
    va_end(list);

    fprintf(pFile, "%s", " \n");
	fflush(pFile);
    fclose(pFile);
}

int DecodeUid(const char *name)
{
    if (isalpha(name[0])) {
        struct passwd *pwd = getpwnam(name);
        if (!pwd) {
            return -1;
        }
        return pwd->pw_uid;
    } else if (isdigit(name[0])) {
        uid_t result = strtoul(name, 0, 10);
        return result;
    } else {
        return -1;
    }
}

void CheckAndCreateDir(const char *fileName)
{
    char *path = strndup(fileName, strrchr(fileName, '/') - fileName);
    if (path != NULL && access(path, F_OK) != 0) {
        mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
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
            break;
        }
        fd = fopen(configFile, "r");
        if (fd == NULL) {
            break;
        }
        buffer = (char*)malloc(fileStat.st_size + 1);
        if (buffer == NULL) {
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