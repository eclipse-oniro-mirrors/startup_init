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

#ifndef BASE_STARTUP_PARAM_H
#define BASE_STARTUP_PARAM_H
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define PARAM_CONST_VALUE_LEN_MAX 4096
#define PARAM_VALUE_LEN_MAX  96
#define PARAM_NAME_LEN_MAX  96
typedef uint32_t ParamHandle;

typedef enum {
    PARAM_CODE_ERROR = -1,
    PARAM_CODE_SUCCESS = 0,
    PARAM_CODE_INVALID_PARAM = 100,
    PARAM_CODE_INVALID_NAME,
    PARAM_CODE_INVALID_VALUE,
    PARAM_CODE_REACHED_MAX,
    PARAM_CODE_NOT_SUPPORT,
    PARAM_CODE_TIMEOUT,
    PARAM_CODE_NOT_FOUND,
    PARAM_CODE_READ_ONLY,
    PARAM_CODE_FAIL_CONNECT,
    PARAM_CODE_NODE_EXIST,
    PARAM_CODE_MAX
} PARAM_CODE;

typedef enum {
    EVENT_TRIGGER_PARAM,
    EVENT_TRIGGER_BOOT,
    EVENT_TRIGGER_PARAM_WAIT,
    EVENT_TRIGGER_PARAM_WATCH
} EventType;

#define LOAD_PARAM_NORMAL 0x00
#define LOAD_PARAM_ONLY_ADD 0x01
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif