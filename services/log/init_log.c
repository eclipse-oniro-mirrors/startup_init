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

#include "init_log.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "securec.h"

#define UNUSED(x) (void)(x)
#define MAX_FORMAT_SIZE 1024
#define MAX_LOG_SIZE 2048
#define BASE_YEAR 1900

static InitLogLevel g_logLevel = INIT_INFO;

static const char *LOG_LEVEL_STR[] = { "DEBUG", "INFO", "WARNING", "ERROR", "FATAL" };

void SetLogLevel(InitLogLevel logLevel)
{
    g_logLevel = logLevel;
}

void InitLog(const char *tag, InitLogLevel logLevel, const char *fileName, int line, const char *fmt, ...)
{
    if (logLevel < g_logLevel) {
        return;
    }
    // ????stdout??????????
    time_t logTime;
    time(&logTime);
    struct tm *t = gmtime(&logTime);
    fprintf(stdout, "[%d-%d-%d %d:%d][pid=%d][%s:%d][%s][%s] ",
        (t->tm_year + BASE_YEAR), (t->tm_mon + 1), t->tm_mday, t->tm_hour, t->tm_min,
        getpid(), fileName, line, tag, LOG_LEVEL_STR[logLevel]);

    va_list list;
    va_start(list, fmt);
    vfprintf(stdout, fmt, list);
    va_end(list);
    fflush(stdout);
#if 0
    int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC | O_APPEND );
    if (fd < 1) {
        printf("xxxxxxxxxxxxxxx open failed. %d\n", errno);
        return;
    }
    if (write(fd, logInfo, strlen(logInfo)) < -1) {
        printf("xxxxxxxxxxxxxxx write failed.%d\n", errno);
        close(fd);
        return;
    }
    close(fd);
#endif
}

