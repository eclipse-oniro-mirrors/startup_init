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

#ifndef BASE_STARTUP_PARAM_BASE_H
#define BASE_STARTUP_PARAM_BASE_H
#include "sys_param.h"
#include "beget_ext.h"
#ifndef PARAM_BASE
#include "securec.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

INIT_LOCAL_API void CloseParamWorkSpace(void);
INIT_LOCAL_API int ParamSprintf(char *buffer, size_t buffSize, const char *format, ...);
INIT_LOCAL_API int ParamMemcpy(void *dest, size_t destMax, const void *src, size_t count);
INIT_LOCAL_API int ParamStrCpy(char *strDest, size_t destMax, const char *strSrc);

typedef struct CachedParameter_ {
    struct WorkSpace_ *workspace;
    long long spaceCommitId;
    uint32_t dataCommitId;
    uint32_t dataIndex;
    uint32_t bufferLen;
    uint32_t nameLen;
    char *paramValue;
    char data[0];
} CachedParameter;

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif // BASE_STARTUP_PARAM_BASE_H