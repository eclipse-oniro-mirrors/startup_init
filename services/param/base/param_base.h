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

#ifndef BASE_STARTUP_PARAM_BASE_H
#define BASE_STARTUP_PARAM_BASE_H

#include "param_osadp.h"
#include "param_trie.h"

#ifndef PARAM_BASE
#include "securec.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define WORKSPACE_STATUS_IN_PROCESS    0x01
#define WORKSPACE_STATUS_VALID       0x02

#ifndef PARAM_BASE
#define PARAM_SPRINTF(buffer, buffSize, format, ...) \
    snprintf_s((buffer), (buffSize), (buffSize) - 1, (format), ##__VA_ARGS__)
#define PARAM_MEMCPY(dest, destMax, src, count)  memcpy_s((dest), (destMax), (src), (count))
#define PARAM_STRCPY(strDest, destMax, strSrc)   strcpy_s((strDest), (destMax), (strSrc))
#else
#define PARAM_SPRINTF(buffer, buffSize, format, ...) snprintf((buffer), (buffSize), (format), ##__VA_ARGS__)
#define PARAM_MEMCPY(dest, destMax, src, count) (memcpy((dest), (src), (count)) != NULL) ? 0 : 1
#define PARAM_STRCPY(strDest, destMax, strSrc) (strcpy((strDest), (strSrc)) != NULL) ? 0 : 1
#endif

static inline uint32_t ReadCommitId(ParamNode *entry)
{
    uint32_t commitId = ATOMIC_LOAD_EXPLICIT(&entry->commitId, MEMORY_ORDER_ACQUIRE);
    while (commitId & PARAM_FLAGS_MODIFY) {
        futex_wait(&entry->commitId, commitId);
        commitId = ATOMIC_LOAD_EXPLICIT(&entry->commitId, MEMORY_ORDER_ACQUIRE);
    }
    return commitId & PARAM_FLAGS_COMMITID;
}

static inline int ReadParamValue_(ParamNode *entry, uint32_t *commitId, char *value, uint32_t *length)
{
    uint32_t id = *commitId;
    do {
        *commitId = id;
        int ret = PARAM_MEMCPY(value, *length, entry->data + entry->keyLength + 1, entry->valueLength);
        PARAM_CHECK(ret == 0, return -1, "Failed to copy value");
        value[entry->valueLength] = '\0';
        *length = entry->valueLength;
        id = ReadCommitId(entry);
    } while (*commitId != id); // if change,must read
    return 0;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif // BASE_STARTUP_PARAM_BASE_H