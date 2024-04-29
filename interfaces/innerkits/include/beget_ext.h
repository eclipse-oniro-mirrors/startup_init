/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#ifdef __LITEOS_M__
#include "hilog_lite/hiview_log.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define INIT_DEBUG_LEVEL "persist.init.debug.loglevel"

#ifndef INIT_LOG_PATH
#define INIT_LOG_PATH STARTUP_INIT_UT_PATH"/data/init_agent/"
#endif

#if defined(__GNUC__) && (__GNUC__ >= 4)
    #define INIT_PUBLIC_API __attribute__((visibility ("default")))
    #define INIT_INNER_API __attribute__((visibility ("default")))
    #define INIT_LOCAL_API __attribute__((visibility("hidden")))
#else
    #define INIT_PUBLIC_API
    #define INIT_INNER_API
    #define INIT_LOCAL_API
#endif

typedef enum InitLogLevel {
    INIT_DEBUG = 0,
    INIT_INFO,
    INIT_WARN,
    INIT_ERROR,
    INIT_FATAL
} InitLogLevel;

typedef enum {
    FSTAB_MOUNT_FAILED = 0,
    SYS_PARAM_INIT_FAILED,
    INIT_GOLBALY_KEY_FAILED,
    CORE_PROCESS_FAILED
} StartupInitErrno;

#if (defined(STARTUP_INIT_TEST) || defined(APPSPAWN_TEST))
#define FILE_NAME   (strrchr((__FILE__), '/')  + 1)
#define STATIC
#else
#define FILE_NAME   (strrchr((__FILE__), '/') ? strrchr((__FILE__), '/') + 1 : (__FILE__))
#define STATIC static
#endif

#ifndef STARTUP_INIT_UT_PATH
#ifdef STARTUP_INIT_TEST // ut
#define STARTUP_INIT_UT_PATH "/data/init_ut"
#else
#define STARTUP_INIT_UT_PATH ""
#endif
#endif

INIT_PUBLIC_API void StartupLog(InitLogLevel logLevel, uint32_t domain, const char *tag, const char *fmt, ...);
INIT_PUBLIC_API void SetInitLogLevel(InitLogLevel level);
INIT_PUBLIC_API InitLogLevel GetInitLogLevel(void);

#if defined(INIT_NO_LOG)
#define STARTUP_LOGV(domain, tag, fmt, ...)
#define STARTUP_LOGI(domain, tag, fmt, ...)
#define STARTUP_LOGW(domain, tag, fmt, ...)
#define STARTUP_LOGE(domain, tag, fmt, ...)
#define STARTUP_LOGF(domain, tag, fmt, ...)
#else
#ifndef __LITEOS_M__
#define STARTUP_LOGV(domain, tag, fmt, ...) \
    StartupLog(INIT_DEBUG, domain, tag, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define STARTUP_LOGI(domain, tag, fmt, ...) \
    StartupLog(INIT_INFO, domain, tag, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define STARTUP_LOGW(domain, tag, fmt, ...) \
    StartupLog(INIT_WARN, domain, tag, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define STARTUP_LOGE(domain, tag, fmt, ...) \
    StartupLog(INIT_ERROR, domain, tag, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define STARTUP_LOGF(domain, tag, fmt, ...) \
    StartupLog(INIT_FATAL, domain, tag, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#else
#define STARTUP_LOGV(domain, tag, fmt, ...) \
    HILOG_DEBUG(HILOG_MODULE_INIT, fmt, ##__VA_ARGS__)
#define STARTUP_LOGI(domain, tag, fmt, ...) \
    HILOG_INFO(HILOG_MODULE_INIT, fmt, ##__VA_ARGS__)
#define STARTUP_LOGW(domain, tag, fmt, ...) \
    HILOG_WARN(HILOG_MODULE_INIT, fmt, ##__VA_ARGS__)
#define STARTUP_LOGE(domain, tag, fmt, ...) \
    HILOG_ERROR(HILOG_MODULE_INIT, fmt, ##__VA_ARGS__)
#define STARTUP_LOGF(domain, tag, fmt, ...) \
    HILOG_FATAL(HILOG_MODULE_INIT, fmt, ##__VA_ARGS__)
#endif
#endif

#define BASE_DOMAIN 0xD002C00
#ifndef BEGET_DOMAIN
#define BEGET_DOMAIN (BASE_DOMAIN + 0xb)
#endif
#define BEGET_LABEL "BEGET"
#define BEGET_LOGI(fmt, ...) STARTUP_LOGI(BEGET_DOMAIN, BEGET_LABEL, fmt, ##__VA_ARGS__)
#define BEGET_LOGE(fmt, ...) STARTUP_LOGE(BEGET_DOMAIN, BEGET_LABEL, fmt, ##__VA_ARGS__)
#define BEGET_LOGV(fmt, ...) STARTUP_LOGV(BEGET_DOMAIN, BEGET_LABEL, fmt, ##__VA_ARGS__)
#define BEGET_LOGW(fmt, ...) STARTUP_LOGW(BEGET_DOMAIN, BEGET_LABEL, fmt, ##__VA_ARGS__)

#define InitLogPrint(outFileName, logLevel, kLevel, fmt, ...) \
    StartupLog(logLevel, BEGET_DOMAIN, kLevel, fmt, ##__VA_ARGS__)

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
