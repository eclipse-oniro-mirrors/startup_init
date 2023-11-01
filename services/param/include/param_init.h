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

#ifndef BASE_STARTUP_PARAM_INIT_H
#define BASE_STARTUP_PARAM_INIT_H
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#ifndef BASE_STARTUP_INIT_PARAM_H
typedef uint32_t ParamHandle;
#endif

/**
 * 外部接口
 * 查询参数，主要用于其他进程使用，需要给定足够的内存保存参数。
 * 如果 value == null，获取value的长度
 * 否则value的大小认为是len
 *
 */
int SystemGetParameterName(ParamHandle handle, char *name, unsigned int len);

/**
 * 外部接口
 * 遍历参数。
 *
 */
int SystemTraversalParameter(const char *prefix,
    void (*traversalParameter)(ParamHandle handle, void *cookie), void *cookie);

long long GetSystemCommitId(void);

/**
 * 外部接口
 * 获取参数值。
 *
 */
int SystemGetParameterValue(ParamHandle handle, char *value, unsigned int *len);

/**
 * 对外接口
 * 根据handle获取对应数据的修改标识。
 * commitId 获取计数变化
 *
 */
int SystemGetParameterCommitId(ParamHandle handle, uint32_t *commitId);

/**
 * 对外接口
 * 查询参数，主要用于其他进程使用，找到对应属性的handle。
 *
 */
int SystemFindParameter(const char *name, ParamHandle *handle);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif