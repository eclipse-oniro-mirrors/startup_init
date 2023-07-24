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

#ifndef BASE_STARTUP_PARAM_PERSIST_H
#define BASE_STARTUP_PARAM_PERSIST_H
#include <stdint.h>
#include <sys/types.h>
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef int (*PersistParamGetPtr)(const char *name, const char *value, void *context);

typedef void *PERSIST_SAVE_HANDLE;
typedef struct {
    int (*load)(PersistParamGetPtr persistParamGet, void *context);
    int (*save)(const char *name, const char *value);
    int (*batchSaveBegin)(PERSIST_SAVE_HANDLE *handle);
    int (*batchSave)(PERSIST_SAVE_HANDLE handle, const char *name, const char *value);
    void (*batchSaveEnd)(PERSIST_SAVE_HANDLE handle);
} PersistParamOps;

#ifndef PARAM_SUPPORT_SAVE_PERSIST
#define PARAM_SUPPORT_SAVE_PERSIST 1 // default
#endif

#ifdef PARAM_SUPPORT_SAVE_PERSIST
int RegisterPersistParamOps(PersistParamOps *ops);
#endif

#ifndef STARTUP_INIT_TEST
#define PARAM_MUST_SAVE_PARAM_DIFF 1 // 1s
#else
#define PARAM_MUST_SAVE_PARAM_DIFF 1
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif