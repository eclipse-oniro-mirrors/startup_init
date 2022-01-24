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

#ifndef BASE_STARTUP_INIT_PARAM_H
#define BASE_STARTUP_INIT_PARAM_H
#include <stdint.h>
#include <stdio.h>

#include "cJSON.h"
#include "param.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif
/**
 * Init 接口
 * 初始化参数服务
 *
 */
void InitParamService(void);

/**
 * Init 接口
 * 启动参数服务，在main启动的最后调用，阻赛当前线程
 *
 */
int StartParamService(void);

/**
 * Init 接口
 * 停止参数服务
 *
 */
void StopParamService(void);

/**
 * Init 接口
 * 加载默认的参数值
 *
 */
int LoadDefaultParams(const char *fileName, unsigned int mode);

/**
 * Init 接口
 * 加载默认参数。
 *
 */
int LoadPersistParams(void);

/**
 * Init 接口
 * 设置参数，主要用于其他进程使用，通过管道修改参数
 *
 */
int SystemWriteParam(const char *name, const char *value);

/**
 * Init 接口
 * 查询参数。
 *
 */
int SystemReadParam(const char *name, char *value, unsigned int *len);

/**
 * 对Init接口
 * 触发一个trigger操作。
 *
 */
void PostTrigger(EventType type, const char *content, uint32_t contentLen);

/**
 * 对Init接口
 * 解析trigger文件。
 *
 */
int ParseTriggerConfig(const cJSON *fileRoot, int (*checkJobValid)(const char *jobName));

/**
 * 对Init接口
 * 按名字执行对应的trigger。
 *
 */
void DoTriggerExec(const char *triggerName);
void DoJobExecNow(const char *triggerName);

/**
 * 对Init接口
 * 按名字添加一个job，用于group支持。
 *
 */
int AddCompleteJob(const char *name, const char *condition, const char *cmdContent);

/**
 * 对Init接口
 * dump 参数和trigger信息
 *
 */
void DumpParametersAndTriggers(void);

void RegisterBootStateChange(void (*bootStateChange)(const char *));
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif