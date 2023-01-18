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

#include "beget_ext.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#ifndef INIT_LOG_TAG
#define INIT_LOG_TAG "Init"
#endif

#ifndef INIT_LOG_DOMAIN
#define INIT_LOG_DOMAIN (BASE_DOMAIN + 1)
#endif

typedef void (*InitCommLog)(int logLevel, uint32_t domain, const char *tag, const char *fmt, va_list vargs);

INIT_LOCAL_API void OpenLogDevice(void);
INIT_LOCAL_API void InitLog(int logLevel, unsigned int domain, const char *tag, const char *fmt, va_list vargs);
INIT_LOCAL_API void SetInitCommLog(InitCommLog logFunc);
INIT_LOCAL_API void EnableInitLog(InitLogLevel level);
INIT_LOCAL_API void EnableInitLogFromCmdline(void);

#if defined(INIT_NO_LOG) || defined(PARAM_BASE)
#define INIT_LOGV(fmt, ...)
#define INIT_LOGI(fmt, ...)
#define INIT_LOGW(fmt, ...)
#define INIT_LOGE(fmt, ...)
#define INIT_LOGF(fmt, ...)
#else
#define INIT_LOGV(fmt, ...) \
    StartupLog(INIT_DEBUG, INIT_LOG_DOMAIN, INIT_LOG_TAG, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define INIT_LOGI(fmt, ...) \
    StartupLog(INIT_INFO, INIT_LOG_DOMAIN, INIT_LOG_TAG, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define INIT_LOGW(fmt, ...) \
    StartupLog(INIT_WARN, INIT_LOG_DOMAIN, INIT_LOG_TAG, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define INIT_LOGE(fmt, ...) \
    StartupLog(INIT_ERROR, INIT_LOG_DOMAIN, INIT_LOG_TAG, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define INIT_LOGF(fmt, ...) \
    StartupLog(INIT_FATAL, INIT_LOG_DOMAIN, INIT_LOG_TAG, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#endif

#ifndef UNLIKELY
#define UNLIKELY(x)    __builtin_expect(!!(x), 0)
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
