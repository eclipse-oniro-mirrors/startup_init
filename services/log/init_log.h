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

#define __FILE_NAME__   (strrchr((__FILE__), '/') ? strrchr((__FILE__), '/') + 1 : (__FILE__))

#ifndef INIT_LOG_TAG
#define INIT_LOG_TAG "[Init]"
#endif

#ifdef OHOS_LITE
#define INIT_LOGE(format, ...) printf("%s %d:  "format, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#define INIT_LOGW(format, ...) printf("%s %d:  "format, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#define INIT_LOGI(format, ...) printf("%s %d:  "format, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#define INIT_LOGD(format, ...) printf("%s %d:  "format, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#else
#define INIT_LOGD(format, ...) InitLog(INIT_LOG_TAG, INIT_DEBUG, (__FILE_NAME__), (__LINE__), format, ##__VA_ARGS__)
#define INIT_LOGI(format, ...) InitLog(INIT_LOG_TAG, INIT_INFO, (__FILE_NAME__), (__LINE__), format, ##__VA_ARGS__)
#define INIT_LOGW(format, ...) InitLog(INIT_LOG_TAG, INIT_WARN, (__FILE_NAME__), (__LINE__), format, ##__VA_ARGS__)
#define INIT_LOGE(format, ...) InitLog(INIT_LOG_TAG, INIT_ERROR, (__FILE_NAME__), (__LINE__), format, ##__VA_ARGS__)
#define INIT_LOGF(format, ...) InitLog(INIT_LOG_TAG, INIT_FATAL, (__FILE_NAME__), (__LINE__), format, ##__VA_ARGS__)

void InitLog(const char *tag, InitLogLevel logLevel, const char *fileName, int line, const char *fmt, ...);
void SetLogLevel(InitLogLevel logLevel);

void Logger(InitLogLevel level, const char *format, ...);
#endif

#define INIT_ERROR_CHECK(ret, statement, format, ...)  \
    if (!(ret)) {                                 \
        INIT_LOGE(format, ##__VA_ARGS__);                        \
        statement;                                \
    }

#define INIT_CHECK_ONLY_RETURN(ret, statement)         \
    if (!(ret)) {                                       \
        statement;                                      \
    }

#ifdef SUPPORT_HILOG
#include "hilog/log.h"

static constexpr OHOS::HiviewDFX::HiLogLabel STARTUP_LABEL = {LOG_CORE, 0, "STARTUP"};

InitLogLevel level_;
int JudgeLevel(const InitLogLevel level) { return return; }

#define STARTUP_LOG(LEVEL, LABEL, Level, fmt, ...) \
    InitLog("[Init]", LEVEL, __FILE_NAME__,  (__LINE__), fmt, ##__VA_ARGS__); \
    if (JudgeLevel(InitLogLevel::LEVEL)) \
        OHOS::HiviewDFX::HiLog::Level(STARTUP_LABEL, "[%{public}s(%{public}d)] " fmt, \
        __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#else
#define STARTUP_LOG(LEVEL, LABEL, Level, fmt, ...) \
    printf("[%s:%d][%s:%d]  " fmt "\n", LABEL, getpid(), __FILE_NAME__, __LINE__, ##__VA_ARGS__);
#endif

#define STARTUP_LOGI(LABEL, fmt, ...) STARTUP_LOG(INIT_INFO, LABEL, Info, fmt, ##__VA_ARGS__)
#define STARTUP_LOGE(LABEL, fmt, ...) STARTUP_LOG(INIT_ERROR, LABEL, Error, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // INIT_LOG_H
