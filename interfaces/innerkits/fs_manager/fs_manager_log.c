/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fs_manager/fs_manager_log.h"

#include "init_log.h"
#include "securec.h"

#define LOG_BUFFER_MAX (1024)
static FILE *g_logFile = NULL;
static FsMgrLogLevel g_defaultLogLevel = FSMGR_DEBUG;
FsManagerLogFunc g_logFunc = FsManagerLogToStd;

static InitLogLevel ConvertToInitLog(FsMgrLogLevel level)
{
    switch (level) {
        case FSMGR_VERBOSE: // fall through
        case FSMGR_DEBUG:
            return INIT_DEBUG;
        case FSMGR_INFO:
            return INIT_INFO;
        case FSMGR_WARNING:
            return INIT_WARN;
        case FSMGR_ERROR:
            return INIT_ERROR;
        case FSMGR_FATAL:
            return INIT_FATAL;
        // Unexpected log level, set it as lowest
        default:
            return INIT_DEBUG;
    }
}

static const char *ConvertToKernelLog(FsMgrLogLevel level)
{
     switch (level) {
        case FSMGR_VERBOSE: // fall through
        case FSMGR_DEBUG:
            return "<7>";
        case FSMGR_INFO:
            return "<6>";
        case FSMGR_WARNING:
            return "<4>";
        case FSMGR_ERROR:
        case FSMGR_FATAL:
            return "<3>";
        // Unexpected log level, set level as lowest
        default:
            return "<7>";
    }
}

// Wrap init log
static void FsManagerLogToKernel(FsMgrLogLevel level, const char *fileName, int line, const char *fmt, ...)
{
    va_list vargs;
    va_start(vargs, fmt);
    char tmpFmt[LOG_BUFFER_MAX];
    if (vsnprintf_s(tmpFmt, LOG_BUFFER_MAX, LOG_BUFFER_MAX - 1, fmt, vargs) == -1) {
        return;
    }
    char logInfo[LOG_BUFFER_MAX];
    if (snprintf_s(logInfo, LOG_BUFFER_MAX, LOG_BUFFER_MAX - 1, "[%s:%d]%s", fileName, line, tmpFmt) == -1) {
        return;
    }
    InitLog(fileName, ConvertToInitLog(level), ConvertToKernelLog(level), logInfo);
    va_end(vargs);
    return;
}

static void WriteLogToFile(FILE *fp, const char *fileName, int line, const char *fmt, ...)
{
    // Sanity checks
    INIT_CHECK_ONLY_RETURN(fp != NULL && fmt != NULL);

    va_list vargs;
    va_start(vargs, fmt);
    va_end(vargs);

    char fullLogMsg[LOG_BUFFER_MAX];
    if (snprintf_s(fullLogMsg, LOG_BUFFER_MAX, LOG_BUFFER_MAX - 1, "[%s:%d][%s] %s",
        fileName == NULL ? "" : fileName, line, "fs_manager", fmt) == -1) {
        return;
    }
    fprintf(fp, "%s", fullLogMsg);
    return;
}

void FsManagerLogToStd(FsMgrLogLevel level, const char *fileName, int line, const char *fmt, ...)
{
    if (level < g_defaultLogLevel) {
        return;
    }
    FILE *fp = NULL;
    if (level >= FSMGR_ERROR) {
        fp = stderr;
    } else {
        fp = stdout;
    }
    va_list vargs;
    va_start(vargs, fmt);
    char tmpFmt[LOG_BUFFER_MAX];
    if (vsnprintf_s(tmpFmt, LOG_BUFFER_MAX, LOG_BUFFER_MAX - 1, fmt, vargs) == -1) {
        return;
    }
    WriteLogToFile(fp, fileName, line, tmpFmt, vargs);
    va_end(vargs);
    return;
}

static void FsManagerLogToFile(FsMgrLogLevel level, const char *fileName, int line, const char *fmt, ...)
{
    if (level < g_defaultLogLevel) {
        return;
    }

    va_list vargs;
    va_start(vargs, fmt);
    // If @g_logFile is NULL, output the log to standard I/O.
    if (g_logFile == NULL) {
        FsManagerLogToStd(level, fileName, line, fmt, vargs);
    } else {
        char tmpFmt[LOG_BUFFER_MAX];
        if (vsnprintf_s(tmpFmt, LOG_BUFFER_MAX, LOG_BUFFER_MAX - 1, fmt, vargs) == -1) {
            return;
        }
        WriteLogToFile(g_logFile, fileName, line, tmpFmt, vargs);
    }
    va_end(vargs);
    return;
}

void FsManagerLogInit(LogTarget target, const char *fileName)
{
    setbuf(stderr, NULL);
    setbuf(stdout, NULL);
    switch (target) {
        case LOG_TO_KERNEL:
            g_logFunc = FsManagerLogToKernel;
            break;
        case LOG_TO_FILE:
            if (fileName != NULL && *fileName != '\0') {
                g_logFile = fopen(fileName, "a+");
                setbuf(g_logFile, NULL);
                // Do not check return values. The log writte function will do this.
            }
            g_logFunc = FsManagerLogToFile;
            break;
        case LOG_TO_STDIO:
            g_logFunc = FsManagerLogToStd;
            break;
        default:
            // Output log to standard I/O by default
            g_logFunc = FsManagerLogToStd;
            break;
    }
    return;
}

void FsManagerLogDeInit(void)
{
    if (g_logFile != NULL) {
        fclose(g_logFile);
        g_logFile = NULL;
    }
    return;
}
