/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

static InitCommLog g_initCommLog = NULL;
INIT_LOCAL_API void SetInitCommLog(InitCommLog logFunc)
{
    g_initCommLog = logFunc;
}

INIT_PUBLIC_API void StartupLog(InitLogLevel logLevel, uint32_t domain, const char *tag, const char *fmt, ...)
{
    if (g_initCommLog != NULL) {
        va_list vargs;
        va_start(vargs, fmt);
        g_initCommLog(logLevel, domain, tag, fmt, vargs);
        va_end(vargs);
    }
}