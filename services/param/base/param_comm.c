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

INIT_LOCAL_API WorkSpace *GetWorkSpaceByName(const char *name)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return NULL, "Invalid paramSpace");
#ifdef PARAM_SUPPORT_SELINUX
    if (paramSpace->selinuxSpace.getParamLabelIndex == NULL) {
        return NULL;
    }
    uint32_t labelIndex = (uint32_t)paramSpace->selinuxSpace.getParamLabelIndex(name) + WORKSPACE_INDEX_BASE;
    if (labelIndex < paramSpace->maxSpaceCount) {
        return paramSpace->workSpace[labelIndex];
    }
    return NULL;
#else
    return paramSpace->workSpace[WORKSPACE_INDEX_DAC];
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
    if (labelIndex < paramSpace->maxSpaceCount) {
        workSpace = paramSpace->workSpace[labelIndex];
    }
    if (workSpace == NULL) {
        return NULL;
    }
    uint32_t rwSpaceLock = ATOMIC_LOAD_EXPLICIT(&workSpace->rwSpaceLock, MEMORY_ORDER_ACQUIRE);
    if (rwSpaceLock & WORKSPACE_STATUS_IN_PROCESS) {
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
