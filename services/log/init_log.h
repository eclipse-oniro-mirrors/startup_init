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

#ifndef INIT_LOG_TAG
#define INIT_LOG_TAG "Init"
#endif

#ifdef OHOS_LITE
#include "hilog/log.h"

#undef LOG_DOMAIN
#define  LOG_DOMAIN    0xD000719

#define INIT_LOGD(fmt, ...) InitToHiLog(INIT_LOG_TAG, LOG_DEBUG, "%s : "fmt, (__FUNCTION__), ##__VA_ARGS__)
#define INIT_LOGI(fmt, ...) InitToHiLog(INIT_LOG_TAG, LOG_INFO, "%s : "fmt, (__FUNCTION__), ##__VA_ARGS__)
#define INIT_LOGW(fmt, ...) InitToHiLog(INIT_LOG_TAG, LOG_WARN, "%s : "fmt, (__FUNCTION__), ##__VA_ARGS__)
#define INIT_LOGE(fmt, ...) InitToHiLog(INIT_LOG_TAG, LOG_ERROR, "%s : "fmt, (__FUNCTION__), ##__VA_ARGS__)
#define INIT_LOGF(fmt, ...) InitToHiLog(INIT_LOG_TAG, LOG_FATAL, "%s : "fmt, (__FUNCTION__), ##__VA_ARGS__)

#define STARTUP_LOGD(LABEL, fmt, ...) InitToHiLog(LABEL, LOG_DEBUG, "%s : "fmt, (__FUNCTION__), ##__VA_ARGS__)
#define STARTUP_LOGI(LABEL, fmt, ...) InitToHiLog(LABEL, LOG_INFO, "%s : "fmt, (__FUNCTION__), ##__VA_ARGS__)
#define STARTUP_LOGE(LABEL, fmt, ...) InitToHiLog(LABEL, LOG_ERROR, "%s : "fmt, (__FUNCTION__), ##__VA_ARGS__)


void InitToHiLog(const char *tag, LogLevel logLevel, const char *fmt, ...);
void SetHiLogLevel(LogLevel logLevel);

#else
#define __FILE_NAME__   (strrchr((__FILE__), '/') ? strrchr((__FILE__), '/') + 1 : (__FILE__))
#define INIT_LOGD(fmt, ...) InitLog(INIT_LOG_TAG, INIT_DEBUG, (__FILE_NAME__), (__LINE__), fmt"\n", ##__VA_ARGS__)
#define INIT_LOGI(fmt, ...) InitLog(INIT_LOG_TAG, INIT_INFO, (__FILE_NAME__), (__LINE__), fmt"\n", ##__VA_ARGS__)
#define INIT_LOGW(fmt, ...) InitLog(INIT_LOG_TAG, INIT_WARN, (__FILE_NAME__), (__LINE__), fmt"\n", ##__VA_ARGS__)
#define INIT_LOGE(fmt, ...) InitLog(INIT_LOG_TAG, INIT_ERROR, (__FILE_NAME__), (__LINE__), fmt"\n", ##__VA_ARGS__)
#define INIT_LOGF(fmt, ...) InitLog(INIT_LOG_TAG, INIT_FATAL, (__FILE_NAME__), (__LINE__), fmt"\n", ##__VA_ARGS__)

#define STARTUP_LOGD(LABEL, fmt, ...) InitLog(LABEL, INIT_DEBUG, (__FILE_NAME__),  (__LINE__), fmt "\n", ##__VA_ARGS__)
#define STARTUP_LOGI(LABEL, fmt, ...) InitLog(LABEL, INIT_INFO, (__FILE_NAME__),  (__LINE__), fmt "\n", ##__VA_ARGS__)
#define STARTUP_LOGE(LABEL, fmt, ...) InitLog(LABEL, INIT_ERROR, (__FILE_NAME__),  (__LINE__), fmt "\n", ##__VA_ARGS__)

void InitLog(const char *tag, InitLogLevel logLevel, const char *fileName, int line, const char *fmt, ...);
void SetLogLevel(InitLogLevel logLevel);
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

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // INIT_LOG_H
