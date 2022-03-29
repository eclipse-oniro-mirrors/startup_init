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

typedef enum InitLogLevel {
    INIT_DEBUG = 0,
    INIT_INFO,
    INIT_WARN,
    INIT_ERROR,
    INIT_FATAL
} InitLogLevel;

#define FILE_NAME   (strrchr((__FILE__), '/') ? strrchr((__FILE__), '/') + 1 : (__FILE__))
void SetInitLogLevel(InitLogLevel logLevel);
void InitLog(InitLogLevel logLevel, const char *domain, const char *fileName, int line, const char *fmt, ...);

#define STARTUP_LOGV(domain, fmt, ...) InitLog(INIT_DEBUG, domain, (FILE_NAME), (__LINE__), fmt, ##__VA_ARGS__)
#define STARTUP_LOGI(domain, fmt, ...) InitLog(INIT_INFO, domain, (FILE_NAME), (__LINE__), fmt, ##__VA_ARGS__)
#define STARTUP_LOGW(domain, fmt, ...) InitLog(INIT_WARN, domain, (FILE_NAME), (__LINE__), fmt, ##__VA_ARGS__)
#define STARTUP_LOGE(domain, fmt, ...) InitLog(INIT_ERROR, domain, (FILE_NAME), (__LINE__), fmt, ##__VA_ARGS__)
#define STARTUP_LOGF(domain, fmt, ...) InitLog(INIT_FATAL, domain, (FILE_NAME), (__LINE__), fmt, ##__VA_ARGS__)

#define BEGET_LABEL "BEGET"
#define BEGET_LOGI(fmt, ...) STARTUP_LOGI(BEGET_LABEL, fmt, ##__VA_ARGS__)
#define BEGET_LOGE(fmt, ...) STARTUP_LOGE(BEGET_LABEL, fmt, ##__VA_ARGS__)
#define BEGET_LOGV(fmt, ...) STARTUP_LOGV(BEGET_LABEL, fmt, ##__VA_ARGS__)
#define BEGET_LOGW(fmt, ...) STARTUP_LOGW(BEGET_LABEL, fmt, ##__VA_ARGS__)

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