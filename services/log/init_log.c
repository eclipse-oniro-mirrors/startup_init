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
#include <sys/stat.h>
#include <time.h>
#include "securec.h"

#define UNUSED(x) \
    do { \
        (void)(x) \
    } while (0)

#define MAX_LOG_SIZE 1024
#define BASE_YEAR 1900
#define UNLIKELY(x)    __builtin_expect(!!(x), 0)

static InitLogLevel g_logLevel = INIT_INFO;
static const char *LOG_LEVEL_STR[] = { "DEBUG", "INFO", "WARNING", "ERROR", "FATAL" };

void SetInitLogLevel(InitLogLevel logLevel)
{
    g_logLevel = logLevel;
}

#ifdef OHOS_LITE
static LogLevel ConvertToHiLog(InitLogLevel level)
{
    switch (level) {
        case INIT_DEBUG:
            return LOG_DEBUG;
        case INIT_INFO:
            return LOG_INFO;
        case INIT_WARN:
            return LOG_WARN;
        case INIT_ERROR:
            return LOG_ERROR;
        case INIT_FATAL:
            return LOG_FATAL;
        // Unexpected log level, set level as lowest
        default:
            return LOG_DEBUG;
    }
}

void InitToHiLog(InitLogLevel logLevel, const char *fmt, ...)
{
    if (logLevel < g_logLevel) {
        return;
    }

    va_list list;
    va_start(list, fmt);
    char tmpFmt[MAX_LOG_SIZE];
    if (vsnprintf_s(tmpFmt, MAX_LOG_SIZE, MAX_LOG_SIZE - 1, fmt, list) == -1) {
        va_end(list);
        return;
    }
    (void)HiLogPrint(LOG_CORE, ConvertToHiLog(logLevel), LOG_DOMAIN, INIT_LOG_TAG, "%{public}s", tmpFmt);
    va_end(list);
    return;
}
#endif

#ifndef INIT_AGENT // for init
static int g_fd = -1;
void OpenLogDevice(void)
{
    int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IRGRP);
    if (fd >= 0) {
        g_fd = fd;
    }
    return;
}

void EnableDevKmsg(void)
{
    /* printk_devkmsg default value is ratelimit, We need to set "on" and remove the restrictions */
    int fd = open("/proc/sys/kernel/printk_devkmsg", O_WRONLY | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IRGRP);
    if (fd < 0) {
        return;
    }
    char *kmsgStatus = "on";
    write(fd, kmsgStatus, strlen(kmsgStatus) + 1);
    close(fd);
    fd = -1;
    return;
}

void InitLog(const char *outFileName, InitLogLevel logLevel, const char *kLevel, const char *fmt, ...)
{
    if (logLevel < g_logLevel) {
        return;
    }

    if (UNLIKELY(g_fd < 0)) {
        OpenLogDevice();
        if (g_fd < 0) {
            return;
        }
    }
    va_list vargs;
    va_start(vargs, fmt);
    char tmpFmt[MAX_LOG_SIZE];
    if (vsnprintf_s(tmpFmt, MAX_LOG_SIZE, MAX_LOG_SIZE - 1, fmt, vargs) == -1) {
        close(g_fd);
        g_fd = -1;
        return;
    }

    char logInfo[MAX_LOG_SIZE];
    if (snprintf_s(logInfo, MAX_LOG_SIZE, MAX_LOG_SIZE - 1, "%s[pid=%d][%s][%s] %s",
        kLevel, getpid(), "INIT", LOG_LEVEL_STR[logLevel], tmpFmt) == -1) {
        close(g_fd);
        g_fd = -1;
        return;
    }
    va_end(vargs);

    if (write(g_fd, logInfo, strlen(logInfo)) < 0) {
        close(g_fd);
        g_fd = -1;
    }
    return;
}
#else // for other process
void InitLog(const char *outFileName, InitLogLevel logLevel, const char *kLevel, const char *fmt, ...)
{
    if (logLevel < g_logLevel) {
        return;
    }
    time_t second = time(0);
    INIT_CHECK_ONLY_RETURN(second >= 0 && outFileName != NULL);
    struct tm *t = localtime(&second);
    INIT_CHECK_ONLY_RETURN(t != NULL);
    FILE *outfile = NULL;
    outfile = fopen(outFileName, "a+");
    INIT_CHECK_ONLY_RETURN(outfile != NULL);
    (void)fprintf(outfile, "[%d-%d-%d %d:%d:%d][pid=%d][%s][%s] ",
        (t->tm_year + BASE_YEAR), (t->tm_mon + 1), t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec,
        getpid(), kLevel, LOG_LEVEL_STR[logLevel]);
    va_list list;
    va_start(list, fmt);
    (void)vfprintf(outfile, fmt, list);
    va_end(list);
    (void)fflush(outfile);
    fclose(outfile);
    return;
}
#endif
