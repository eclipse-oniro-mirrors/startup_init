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
#include <ctype.h>
#include <limits.h>

#include "param_manager.h"
#include "param_trie.h"
#include "param_base.h"

INIT_LOCAL_API uint32_t GetWorkSpaceIndex(const char *name)
{
#ifdef PARAM_SUPPORT_SELINUX
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return (uint32_t)-1, "Invalid paramSpace");
    return (paramSpace->selinuxSpace.getParamLabelIndex != NULL) ?
        paramSpace->selinuxSpace.getParamLabelIndex(name) + WORKSPACE_INDEX_BASE : (uint32_t)-1;
#else
    return 0;
#endif
}

INIT_LOCAL_API WorkSpace *GetWorkSpace(uint32_t labelIndex)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return NULL, "Invalid paramSpace");
    PARAM_CHECK(paramSpace->workSpace != NULL, return NULL, "Invalid paramSpace->workSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return NULL, "Invalid space");
#ifdef PARAM_SUPPORT_SELINUX
    if (labelIndex == 0) {
        return paramSpace->workSpace[0];
    }
    WorkSpace *workSpace = NULL;
    if (labelIndex < paramSpace->maxLabelIndex) {
        workSpace = paramSpace->workSpace[labelIndex];
    }
    if (workSpace == NULL) {
        return NULL;
    }
    uint32_t rwSpaceLock = ATOMIC_LOAD_EXPLICIT(&workSpace->rwSpaceLock, memory_order_acquire);
    if (rwSpaceLock == 1) {
        return NULL;
    }
    if (workSpace->area != NULL) {
        return workSpace;
    }
    return NULL;
#else
    return paramSpace->workSpace[0];
#endif
}

INIT_LOCAL_API ParamSecurityOps *GetParamSecurityOps(int type)
{
    PARAM_CHECK(type < PARAM_SECURITY_MAX, return NULL, "Invalid type");
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return NULL, "Invalid paramSpace");
    return &paramSpace->paramSecurityOps[type];
}

INIT_LOCAL_API ParamSecurityLabel *GetParamSecurityLabel()
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return NULL, "Invalid paramSpace");
    return &paramSpace->securityLabel;
}

INIT_LOCAL_API int SplitParamString(char *line, const char *exclude[], uint32_t count,
    int (*result)(const uint32_t *context, const char *name, const char *value), const uint32_t *context)
{
    // Skip spaces
    char *name = line;
    while (isspace(*name) && (*name != '\0')) {
        name++;
    }
    // Empty line or Comment line
    if (*name == '\0' || *name == '#') {
        return 0;
    }

    char *value = name;
    // find the first delimiter '='
    while (*value != '\0') {
        if (*value == '=') {
            (*value) = '\0';
            value = value + 1;
            break;
        }
        value++;
    }

    // Skip spaces
    char *tmp = name;
    while ((tmp < value) && (*tmp != '\0')) {
        if (isspace(*tmp)) {
            (*tmp) = '\0';
            break;
        }
        tmp++;
    }

    // empty name, just ignore this line
    if (*value == '\0') {
        return 0;
    }

    // Filter excluded parameters
    for (uint32_t i = 0; i < count; i++) {
        if (strncmp(name, exclude[i], strlen(exclude[i])) == 0) {
            return 0;
        }
    }

    // Skip spaces for value
    while (isspace(*value) && (*value != '\0')) {
        value++;
    }

    // Trim the ending spaces of value
    char *pos = value + strlen(value);
    pos--;
    while (isspace(*pos) && pos > value) {
        (*pos) = '\0';
        pos--;
    }

    // Strip starting and ending " for value
    if ((*value == '"') && (pos > value) && (*pos == '"')) {
        value = value + 1;
        *pos = '\0';
    }
    return result(context, name, value);
}

INIT_LOCAL_API int ParamSprintf(char *buffer, size_t buffSize, const char *format, ...)
{
    int len = -1;
    va_list vargs;
    va_start(vargs, format);
#ifdef PARAM_BASE
    len = vsnprintf(buffer, buffSize - 1, format, vargs);
#else
    len = vsnprintf_s(buffer, buffSize, buffSize - 1, format, vargs);
#endif
    va_end(vargs);
    return len;
}

INIT_LOCAL_API int ParamMemcpy(void *dest, size_t destMax, const void *src, size_t count)
{
    int ret = 0;
#ifdef PARAM_BASE
    memcpy(dest, src, count);
#else
    ret = memcpy_s(dest, destMax, src, count);
#endif
    return ret;
}

INIT_LOCAL_API int ParamStrCpy(char *strDest, size_t destMax, const char *strSrc)
{
    int ret = 0;
#ifdef PARAM_BASE
    if (strlen(strSrc) >= destMax) {
        return -1;
    }
    size_t i = 0;
    while ((i < destMax) && *strSrc != '\0') {
        *strDest = *strSrc;
        strDest++;
        strSrc++;
        i++;
    }
    *strDest = '\0';
#else
    ret = strcpy_s(strDest, destMax, strSrc);
#endif
    return ret;
}