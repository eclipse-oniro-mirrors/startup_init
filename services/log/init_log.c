/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include <sys/time.h>

#include "init_utils.h"
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
#define BASE_YEAR 1900

static InitLogLevel g_logLevel = INIT_INFO;
#ifdef INIT_FILE
static void LogToFile(const char *logFile, const char *tag, const char *info)
{
    struct timespec curr;
    if (clock_gettime(CLOCK_REALTIME, &curr) != 0) {
        return;
    }
    FILE *outfile = NULL;
    INIT_CHECK_ONLY_RETURN((outfile = fopen(logFile, "a+")) != NULL);
    struct tm t;
    char dateTime[80] = {"00-00-00 00:00:00"}; // 80 data time
    if (localtime_r(&curr.tv_sec, &t) != NULL) {
        strftime(dateTime, sizeof(dateTime), "%Y-%m-%d %H:%M:%S", &t);
    }
    (void)fprintf(outfile, "[%s.%ld][pid=%d %d][%s]%s \n", dateTime, curr.tv_nsec, getpid(), gettid(), tag, info);
    (void)fflush(outfile);
    fclose(outfile);
    return;
}
#endif

#ifdef INIT_DMESG
static int g_fd = -1;
INIT_LOCAL_API void OpenLogDevice(void)
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
    char logInfo[DEF_LOG_SIZE + DEF_LOG_SIZE] = {0};
    if (snprintf_s(logInfo, sizeof(logInfo), sizeof(logInfo) - 1, "%s[pid=%d][%s][%s]%s",
        LOG_KLEVEL_STR[logLevel], getpid(), tag, LOG_LEVEL_STR[logLevel], info) == -1) {
        logInfo[sizeof(logInfo) - 2] = '\n'; // 2 add \n to tail
        logInfo[sizeof(logInfo) - 1] = '\0';
        return;
    }
    if (write(g_fd, logInfo, strlen(logInfo)) < 0) {
        printf("%s\n", logInfo);
    }
    return;
}
#endif

static void PrintLog(InitLogLevel logLevel, unsigned int domain, const char *tag, const char *logInfo)
{
#ifdef OHOS_LITE
    static const LogLevel LOG_LEVEL[] = { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };
    (void)HiLogPrint(INIT_LOG_INIT, LOG_LEVEL[logLevel], domain, tag, "%s", logInfo);
#endif
#ifdef INIT_DMESG
    LogToDmesg(logLevel, tag, logInfo);
#endif
#ifdef INIT_AGENT
    static const LogLevel LOG_LEVEL[] = { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };
    HiLogBasePrint(LOG_CORE, LOG_LEVEL[logLevel], domain, tag, "%{public}s", logInfo);
#endif
#ifdef INIT_FILE
    LogToFile(INIT_LOG_PATH"begetctl.log", tag, logInfo);
#endif
}

INIT_LOCAL_API void InitLog(int logLevel, unsigned int domain, const char *tag, const char *fmt, va_list vargs)
{
    if ((int)g_logLevel > logLevel) {
        return;
    }
    char tmpFmt[DEF_LOG_SIZE] = {0};
    if (vsnprintf_s(tmpFmt, sizeof(tmpFmt), sizeof(tmpFmt) - 1, fmt, vargs) == -1) {
        tmpFmt[sizeof(tmpFmt) - 2] = '\n'; // 2 add \n to tail
        tmpFmt[sizeof(tmpFmt) - 1] = '\0';
    }
    PrintLog((InitLogLevel)logLevel, domain, tag, tmpFmt);
}

INIT_PUBLIC_API void SetInitLogLevel(InitLogLevel level)
{
    if (level <= INIT_FATAL) {
        g_logLevel = level;
    }
    return;
}

INIT_LOCAL_API void EnableInitLog(InitLogLevel level)
{
    g_logLevel = level;
    SetInitCommLog(InitLog);
}

INIT_LOCAL_API void EnableInitLogFromCmdline(void)
{
    SetInitCommLog(InitLog);
    char level[MAX_BUFFER_LEN] = {0};
    char *buffer = ReadFileData("/proc/cmdline");
    if (buffer == NULL) {
        INIT_LOGE("Failed to read \"/proc/cmdline\"");
        return;
    }
    int ret = GetProcCmdlineValue("initloglevel", buffer, level, MAX_BUFFER_LEN);
    free(buffer);
    if (ret != 0) {
        INIT_LOGE("Failed get log level from cmdline");
        return;
    }
    errno = 0;
    unsigned int logLevel = (unsigned int)strtoul(level, 0, 10); // 10 is decimal
    INIT_INFO_CHECK(errno == 0, return, "Failed strtoul %s, err=%d", level, errno);
    SetInitLogLevel((InitLogLevel)logLevel);
    return;
}
