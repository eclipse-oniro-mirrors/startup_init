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

#ifndef BEGET_EXT_API_H
#define BEGET_EXT_API_H

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#if defined(ENABLE_HILOG) || defined(OHOS_LITE)
#include "hilog/log.h"
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD000719
#endif

typedef enum InitLogLevel {
    INIT_DEBUG = 0,
    INIT_INFO,
    INIT_WARN,
    INIT_ERROR,
    INIT_FATAL
} InitLogLevel;

#ifndef INIT_LOG_PATH
#define INIT_LOG_PATH "/data/init_agent/"
#endif

#define FILE_NAME   (strrchr((__FILE__), '/') ? strrchr((__FILE__), '/') + 1 : (__FILE__))
void InitLogInit(const char *outFileName, InitLogLevel logLevel, const char *kLevel, const char *fmt, ...);
void InitLogAgent(const char *outFileName, InitLogLevel logLevel, const char *kLevel, const char *fmt, ...);
void OpenLogDevice(void);

#ifndef INIT_AGENT
#define InitLogPrint InitLogInit
#else
#define InitLogPrint InitLogAgent
#endif

#ifndef OHOS_LITE
#ifndef ENABLE_HILOG
#define STARTUP_LOGV(logFile, LABEL, fmt, ...) \
    do {    \
        InitLogPrint(INIT_LOG_PATH logFile, INIT_DEBUG, LABEL, "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)

#define STARTUP_LOGI(logFile, LABEL, fmt, ...) \
    do {    \
        InitLogPrint(INIT_LOG_PATH logFile, INIT_INFO, LABEL, "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)

#define STARTUP_LOGE(logFile, LABEL, fmt, ...) \
    do {    \
        InitLogPrint(INIT_LOG_PATH logFile, INIT_ERROR, LABEL, "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)

#define STARTUP_LOGW(logFile, LABEL, fmt, ...) \
    do {    \
        InitLogPrint(INIT_LOG_PATH logFile, INIT_WARN, LABEL, "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)

#else
#define STARTUP_LOGV(logFile, LABEL, fmt, ...) \
    do {    \
        InitLogPrint(INIT_LOG_PATH logFile, INIT_DEBUG, LABEL, "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
        (void)HiLogPrint(LOG_APP, LOG_DEBUG, LOG_DOMAIN, LABEL, "[%{public}s(%{public}d)] " fmt, \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)

#define STARTUP_LOGI(logFile, LABEL, fmt, ...) \
    do {    \
        InitLogPrint(INIT_LOG_PATH logFile, INIT_INFO, LABEL, "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
        (void)HiLogPrint(LOG_APP, LOG_INFO, LOG_DOMAIN, LABEL, "[%{public}s(%{public}d)] " fmt, \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)

#define STARTUP_LOGE(logFile, LABEL, fmt, ...) \
    do {    \
        InitLogPrint(INIT_LOG_PATH logFile, INIT_ERROR, LABEL, "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
        (void)HiLogPrint(LOG_APP, LOG_ERROR, LOG_DOMAIN, LABEL, "[%{public}s(%{public}d)] " fmt, \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)

#define STARTUP_LOGW(logFile, LABEL, fmt, ...) \
    do {    \
        InitLogPrint(INIT_LOG_PATH logFile, INIT_WARN, LABEL, "[%s:%d)] " fmt "\n", \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
        (void)HiLogPrint(LOG_APP, LOG_WARN, LOG_DOMAIN, LABEL, "[%{public}s(%{public}d)] " fmt, \
            (FILE_NAME), (__LINE__), ##__VA_ARGS__); \
    } while (0)
#endif
#endif

#define BEGET_LOG_FILE "beget.log"
#define BEGET_LABEL "BEGET"
#define BEGET_LOGI(fmt, ...) STARTUP_LOGI(BEGET_LOG_FILE, BEGET_LABEL, fmt, ##__VA_ARGS__)
#define BEGET_LOGE(fmt, ...) STARTUP_LOGE(BEGET_LOG_FILE, BEGET_LABEL, fmt, ##__VA_ARGS__)
#define BEGET_LOGV(fmt, ...) STARTUP_LOGV(BEGET_LOG_FILE, BEGET_LABEL, fmt, ##__VA_ARGS__)
#define BEGET_LOGW(fmt, ...) STARTUP_LOGW(BEGET_LOG_FILE, BEGET_LABEL, fmt, ##__VA_ARGS__)

#define BEGET_ERROR_CHECK(ret, statement, format, ...) \
    if (!(ret)) {                                     \
        BEGET_LOGE(format, ##__VA_ARGS__);             \
        statement;                                    \
    }                                                 \

#define BEGET_INFO_CHECK(ret, statement, format, ...) \
    if (!(ret)) {                                    \
        BEGET_LOGI(format, ##__VA_ARGS__);            \
        statement;                                   \
    }                                          \

#define BEGET_WARNING_CHECK(ret, statement, format, ...) \
    if (!(ret)) {                                     \
        BEGET_LOGW(format, ##__VA_ARGS__);             \
        statement;                                    \
    }                                                 \

#define BEGET_CHECK(ret, statement) \
    if (!(ret)) {                  \
        statement;                 \
    }                         \

#define BEGET_CHECK_RETURN_VALUE(ret, result) \
    do {                                \
        if (!(ret)) {                            \
            return result;                       \
        }                                  \
    } while (0)

#define BEGET_CHECK_ONLY_RETURN(ret) \
    do {                                \
        if (!(ret)) {                   \
            return;                     \
        } \
    } while (0)

#define BEGET_CHECK_ONLY_ELOG(ret, format, ...) \
    do {                                       \
        if (!(ret)) {                          \
            BEGET_LOGE(format, ##__VA_ARGS__);  \
        } \
    } while (0)

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif