/*
 * Copyright (c) 2020 Huawei Device Co., Ltd.
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

#ifndef BASE_STARTUP_PROPERTY_SERVICE_H
#define BASE_STARTUP_PROPERTY_SERVICE_H
#include <stdio.h>
#include "property.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * Init 接口
 * 初始化属性服务
 *
 */
void InitPropertyService();

/**
 * Init 接口
 * 加载默认的属性值
 *
 */
int LoadDefaultProperty(const char *fileName);

/**
 * Init 接口
 * 安全使用，加载属性的信息，包括selinux label 等
 *
 */
int LoadPropertyInfo(const char *fileName);

/**
 * Init 接口
 * 启动属性服务，在main启动的最后调用，阻赛当前线程
 *
 */
int StartPropertyService();

/**
 * Init 接口
 * 设置属性，主要用于其他进程使用，通过管道修改属性
 *
 */
int SystemWriteParameter(const char *name, const char *value);

/**
 * Init 接口
 * 查询属性。
 *
 */
int SystemReadParameter(const char *name, char *value, unsigned int *len);

/**
 * Init 接口
 * 遍历属性。
 *
 */
int SystemTraversalParameters(void (*traversalParameter)(PropertyHandle handle, void* cookie), void* cookie);

/**
 * Init 接口
 * 加载默认属性。
 *
 */
int LoadPersistProperties();

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif