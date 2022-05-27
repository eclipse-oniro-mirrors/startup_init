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
#ifdef OHOS_LITE
#ifndef INIT_LOG_INIT
#define INIT_LOG_INIT LOG_CORE
#endif
#include "hilog/log.h"
#endif
#ifdef INIT_AGENT
#include "hilog_base/log_base.h"
#endif

#define DEF_LOG_SIZE 128
#define MAX_LOG_SIZE 1024
#define BASE_YEAR 1900

static InitLogLevel g_logLevel = INIT_INFO;
void SetInitLogLevel(InitLogLevel logLevel)
{
    g_logLevel = logLevel;
}

#ifdef INIT_FILE
static void LogToFile(const char *logFile, const char *tag, const char *info)
{
    time_t second = time(0);
    if (second <= 0) {
        return;
    }
    struct tm *t = localtime(&second);
    FILE *outfile = fopen(logFile, "a+");
    if (t == NULL || outfile == NULL) {
        return;
    }
    (void)fprintf(outfile, "[%d-%d-%d %d:%d:%d][pid=%d][%s]%s \n",
        (t->tm_year + BASE_YEAR), (t->tm_mon + 1), t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, getpid(), tag, info);
    (void)fflush(outfile);
    fclose(outfile);
    return;
}
#endif

#ifdef INIT_DMESG
static int g_fd = -1;
void OpenLogDevice(void)
{
    int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd >= 0) {
        g_fd = fd;
    }
    return;
}

void LogToDmesg(InitLogLevel logLevel, const char *tag, const char *info)
{
    static const char *LOG_LEVEL_STR[] = { "DEBUG", "INFO", "WARNING", "ERROR", "FATAL" };
    static const char *LOG_KLEVEL_STR[] = { "<7>", "<6>", "<4>", "<3>", "<3>" };

    if (UNLIKELY(g_fd < 0)) {
        OpenLogDevice();
        if (g_fd < 0) {
            return;
        }
    }
    char logInfo[MAX_LOG_SIZE];
    if (snprintf_s(logInfo, MAX_LOG_SIZE, MAX_LOG_SIZE - 1, "%s[pid=%d %d][%s][%s]%s",
        LOG_KLEVEL_STR[logLevel], getpid(), getppid(), tag, LOG_LEVEL_STR[logLevel], info) == -1) {
        close(g_fd);
        g_fd = -1;
        return;
    }
    if (write(g_fd, logInfo, strlen(logInfo)) < 0) {
        close(g_fd);
        g_fd = -1;
    }
    return;
}
#endif

void InitLog(InitLogLevel logLevel, unsigned int domain, const char *tag, const char *fmt, ...)
{
    if (g_logLevel > logLevel) {
        return;
    }
    va_list vargs;
    va_start(vargs, fmt);
    char tmpFmt[DEF_LOG_SIZE] = {0};
    if (vsnprintf_s(tmpFmt, sizeof(tmpFmt), sizeof(tmpFmt) - 1, fmt, vargs) == -1) {
        va_end(vargs);
#ifdef OHOS_LITE
        static LogLevel LOG_LEVEL[] = { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };
        (void)HiLogPrint(INIT_LOG_INIT, LOG_LEVEL[logLevel], domain, tag, "%{public}s", fmt);
#endif
        return;
    }
    va_end(vargs);
#ifdef OHOS_LITE
    static LogLevel LOG_LEVEL[] = { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };
    (void)HiLogPrint(INIT_LOG_INIT, LOG_LEVEL[logLevel], domain, tag, "%{public}s", tmpFmt);
#endif
#ifdef INIT_DMESG
    LogToDmesg(logLevel, tag, tmpFmt);
#endif
#ifdef INIT_AGENT
    static LogLevel LOG_LEVEL[] = { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };
    HiLogBasePrint(LOG_CORE, LOG_LEVEL[logLevel], domain, tag, "%{public}s", tmpFmt);
#ifdef INIT_FILE
    LogToFile("/data/init_agent/begetctl.log", tag, tmpFmt);
#endif
#endif
}
