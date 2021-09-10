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
#ifdef OHOS_LITE
#include "hilog/log.h"
#endif
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "securec.h"

#define UNUSED(x) (void)(x)
#define MAX_FORMAT_SIZE 2048
#define MAX_LOG_SIZE 2048
#define BASE_YEAR 1900

static InitLogLevel g_logLevel = INIT_INFO;
static const char *LOG_LEVEL_STR[] = { "DEBUG", "INFO", "WARNING", "ERROR", "FATAL" };

void SetLogLevel(InitLogLevel logLevel)
{
    g_logLevel = logLevel;
}

#ifdef OHOS_LITE
static LogLevel g_hiLogLevel = LOG_INFO;

void SetHiLogLevel(LogLevel logLevel)
{
    g_hiLogLevel = logLevel;
}

void InitToHiLog(const char *tag, LogLevel logLevel, const char *fmt, ...)
{
    if (logLevel < g_hiLogLevel) {
        return;
    }
    va_list list;
    va_start(list, fmt);
    char tmpFmt[MAX_FORMAT_SIZE];
    if (vsnprintf_s(tmpFmt, MAX_FORMAT_SIZE, MAX_FORMAT_SIZE - 1, fmt, list) == -1) {
        va_end(list);
        return;
    }
    (void)HiLogPrint(LOG_CORE, logLevel, LOG_DOMAIN, tag, "%{public}s", tmpFmt);
    va_end(list);
    return;
}
#endif

void InitLog(const char *tag, InitLogLevel logLevel, const char *fileName, int line, const char *fmt, ...)
{
    if (logLevel < g_logLevel) {
        return;
    }
    // 可以替换stdout这个为对应的文件句柄
    time_t logTime;
    time(&logTime);
    struct tm *t = gmtime(&logTime);
    if (t == NULL) {
        return;
    }
    fprintf(stdout, "[%d-%d-%d %d:%d:%d][pid=%d][%s:%d][%s][%s] ",
        (t->tm_year + BASE_YEAR), (t->tm_mon + 1), t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec,
        getpid(), fileName, line, tag, LOG_LEVEL_STR[logLevel]);

    va_list list;
    va_start(list, fmt);
    vfprintf(stdout, fmt, list);
    va_end(list);
    fflush(stdout);
}
