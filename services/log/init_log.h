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

#ifndef INIT_LOG_H
#define INIT_LOG_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(ENABLE_HILOG) || defined(OHOS_LITE)
#include "hilog/log.h"
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD000719
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef enum InitLogLevel {
    INIT_DEBUG = 0,
    INIT_INFO,
    INIT_WARN,
    INIT_ERROR,
    INIT_FATAL
} InitLogLevel;

void SetInitLogLevel(InitLogLevel logLevel);

#ifndef INIT_LOG_TAG
#define INIT_LOG_TAG "Init"
#endif

#ifndef INIT_LOG_PATH
#define INIT_LOG_PATH "/data/init_agent/"
#endif

#ifdef OHOS_LITE
#define INIT_LOGD(fmt, ...) InitToHiLog(INIT_DEBUG, "%s : "fmt, (__FUNCTION__), ##__VA_ARGS__)
#define INIT_LOGI(fmt, ...) InitToHiLog(INIT_INFO, "%s : "fmt, (__FUNCTION__), ##__VA_ARGS__)
#define INIT_LOGW(fmt, ...) InitToHiLog(INIT_WARN, "%s : "fmt, (__FUNCTION__), ##__VA_ARGS__)
#define INIT_LOGE(fmt, ...) InitToHiLog(INIT_ERROR, "%s : "fmt, (__FUNCTION__), ##__VA_ARGS__)
#define INIT_LOGF(fmt, ...) InitToHiLog(INIT_FATAL, "%s : "fmt, (__FUNCTION__), ##__VA_ARGS__)

#define STARTUP_LOGD(logFIle, LABEL, fmt, ...) InitToHiLog(LABEL, INIT_DEBUG, "%s : "fmt, (__FUNCTION__), ##__VA_ARGS__)
#define STARTUP_LOGI(logFIle, LABEL, fmt, ...) InitToHiLog(INIT_INFO, "%s : "fmt, (__FUNCTION__), ##__VA_ARGS__)
#define STARTUP_LOGE(logFIle, LABEL, fmt, ...) InitToHiLog(INIT_ERROR, "%s : "fmt, (__FUNCTION__), ##__VA_ARGS__)

void InitToHiLog(InitLogLevel logLevel, const char *fmt, ...);

#else
#define FILE_NAME   (strrchr((__FILE__), '/') ? strrchr((__FILE__), '/') + 1 : (__FILE__))
void InitLog(const char *outFileName, InitLogLevel logLevel, const char *kLevel, const char *fmt, ...);
void OpenLogDevice(void);
void EnableDevKmsg(void);

#define INIT_LOGD(fmt, ...) \
    do {    \
        InitLog(INIT_LOG_PATH "init_agent.log", INIT_DEBUG, "<7>", "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)

#define INIT_LOGI(fmt, ...) \
    do {    \
        InitLog(INIT_LOG_PATH "init_agent.log", INIT_INFO, "<6>", "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)

#define INIT_LOGW(fmt, ...) \
    do {    \
        InitLog(INIT_LOG_PATH "init_agent.log", INIT_WARN, "<4>", "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)

#define INIT_LOGE(fmt, ...) \
    do {    \
        InitLog(INIT_LOG_PATH "init_agent.log", INIT_ERROR, "<3>", "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)

#define INIT_LOGF(fmt, ...) \
    do {    \
        InitLog(INIT_LOG_PATH "init_agent.log", INIT_FATAL, "<3>", "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)

#ifndef ENABLE_HILOG
#define STARTUP_LOGD(logFile, LABEL, fmt, ...) \
    do {    \
        InitLog(INIT_LOG_PATH logFile, INIT_DEBUG, LABEL, "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)

#define STARTUP_LOGI(logFile, LABEL, fmt, ...) \
    do {    \
        InitLog(INIT_LOG_PATH logFile, INIT_INFO, LABEL, "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)

#define STARTUP_LOGE(logFile, LABEL, fmt, ...) \
    do {    \
        InitLog(INIT_LOG_PATH logFile, INIT_ERROR, LABEL, "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)

#else
#define STARTUP_LOGD(logFile, LABEL, fmt, ...) \
    do {    \
        InitLog(INIT_LOG_PATH logFile, INIT_DEBUG, LABEL, "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
        (void)HiLogPrint(LOG_APP, LOG_DEBUG, LOG_DOMAIN, LABEL, "[%{public}s(%{public}d)] " fmt, \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)

#define STARTUP_LOGI(logFile, LABEL, fmt, ...) \
    do {    \
        InitLog(INIT_LOG_PATH logFile, INIT_INFO, LABEL, "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
        (void)HiLogPrint(LOG_APP, LOG_INFO, LOG_DOMAIN, LABEL, "[%{public}s(%{public}d)] " fmt, \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)

#define STARTUP_LOGE(logFile, LABEL, fmt, ...) \
    do {    \
        InitLog(INIT_LOG_PATH logFile, INIT_ERROR, LABEL, "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
        (void)HiLogPrint(LOG_APP, LOG_ERROR, LOG_DOMAIN, LABEL, "[%{public}s(%{public}d)] " fmt, \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)
#endif
#endif

#define INIT_ERROR_CHECK(ret, statement, format, ...) \
    do {                                                  \
        if (!(ret)) {                                     \
            INIT_LOGE(format, ##__VA_ARGS__);             \
            statement;                                    \
        }                                                 \
    } while (0)

#define INIT_INFO_CHECK(ret, statement, format, ...) \
    do {                                                  \
        if (!(ret)) {                                    \
            INIT_LOGI(format, ##__VA_ARGS__);            \
            statement;                                   \
        }                                          \
    } while (0)

#define INIT_WARNING_CHECK(ret, statement, format, ...) \
    do {                                                  \
        if (!(ret)) {                                     \
            INIT_LOGW(format, ##__VA_ARGS__);             \
            statement;                                    \
        }                                                 \
    } while (0)

#define INIT_CHECK(ret, statement) \
    do {                                \
        if (!(ret)) {                  \
            statement;                 \
        }                         \
    } while (0)

#define INIT_CHECK_RETURN_VALUE(ret, result) \
    do {                                \
        if (!(ret)) {                            \
            return result;                       \
        }                                  \
    } while (0)

#define INIT_CHECK_ONLY_RETURN(ret) \
    do {                                \
        if (!(ret)) {                   \
            return;                     \
        } \
    } while (0)

#define INIT_CHECK_ONLY_ELOG(ret, format, ...) \
    do {                                       \
        if (!(ret)) {                          \
            INIT_LOGE(format, ##__VA_ARGS__);  \
        } \
    } while (0)

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // INIT_LOG_H
