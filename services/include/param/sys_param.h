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

#ifndef BASE_STARTUP_INIT_SYS_PARAM_H
#define BASE_STARTUP_INIT_SYS_PARAM_H
#include <stdarg.h>
#include <stdint.h>
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef uint32_t ParamHandle;
typedef void *CachedHandle;

typedef struct {
    uint8_t updaterMode;
    void (*logFunc)(int logLevel, uint32_t domain, const char *tag, const char *fmt, va_list vargs);
    int (*setfilecon)(const char *name, const char *content);
} PARAM_WORKSPACE_OPS;

/**
 * parameter service初始化接口 仅供init调用
 */
int InitParamWorkSpace(int onlyRead, const PARAM_WORKSPACE_OPS *ops);

/**
 * Init 接口
 * 查询参数。
 *
 */
int SystemReadParam(const char *name, char *value, uint32_t *len);

/**
 * parameter client初始化接口 供服务调用
 */
void InitParameterClient(void);

/**
 * 对外接口
 * 查询参数，主要用于其他进程使用，找到对应属性的handle。
 *
 */
int SystemFindParameter(const char *name, ParamHandle *handle);

/**
 * 对外接口
 * 根据handle获取对应数据的修改标识。
 * commitId 获取计数变化
 *
 */
int SystemGetParameterCommitId(ParamHandle handle, uint32_t *commitId);

/**
 * 外部接口
 * 获取参数值。
 *
 */
int SystemGetParameterValue(ParamHandle handle, char *value, unsigned int *len);

long long GetSystemCommitId(void);

/**
 * 外部接口
 * 保存相关的parameter信息，包括workspace，和各层的commit。
 *
 */
CachedHandle CachedParameterCreate(const char *name, const char *defValue);

/**
 * 外部接口
 * 如果获取到value，返回对应的 paramValue的指针，否则返回上面定义的defValue
 *
 */
const char *CachedParameterGet(CachedHandle handle);

/**
 * 外部接口
 * 释放handle内存
 *
 */
void CachedParameterDestroy(CachedHandle handle);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif