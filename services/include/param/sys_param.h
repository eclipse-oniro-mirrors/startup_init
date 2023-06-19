/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef BASE_STARTUP_INIT_SYS_PARAM_H
#define BASE_STARTUP_INIT_SYS_PARAM_H
#include "param_common.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * parameter client init
 */
void InitParameterClient(void);

/**
 * by name and default value，save parameter info in handle。
 *
 */
CachedHandle CachedParameterCreate(const char *name, const char *defValue);

/**
 * destroy handle
 *
 */
void CachedParameterDestroy(CachedHandle handle);

/**
 * if name exist，return value else return default value
 *
 */
static inline const char *CachedParameterGet(CachedHandle handle)
{
    struct CachedParameter_ *param = (struct CachedParameter_ *)handle;
    if (param == NULL) {
        return NULL;
    }

    // no change, do not to find
    long long spaceCommitId = ATOMIC_UINT64_LOAD_EXPLICIT(&param->workspace->area->commitId, MEMORY_ORDER_ACQUIRE);
    if (param->spaceCommitId == spaceCommitId) {
        return param->paramValue;
    }
    param->spaceCommitId = spaceCommitId;
    int changed = 0;
    if (param->cachedParameterCheck == NULL) {
        return param->paramValue;
    }
    return param->cachedParameterCheck(param, &changed);
}

static inline const char *CachedParameterGetChanged(CachedHandle handle, int *changed)
{
    struct CachedParameter_ *param = (struct CachedParameter_ *)handle;
    if (param == NULL) {
        return NULL;
    }
    // no change, do not to find
    long long spaceCommitId = ATOMIC_UINT64_LOAD_EXPLICIT(&param->workspace->area->commitId, MEMORY_ORDER_ACQUIRE);
    if (param->spaceCommitId == spaceCommitId) {
        return param->paramValue;
    }
    param->spaceCommitId = spaceCommitId;
    if ((changed == NULL) || (param->cachedParameterCheck == NULL)) {
        return param->paramValue;
    }
    return param->cachedParameterCheck(param, changed);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif