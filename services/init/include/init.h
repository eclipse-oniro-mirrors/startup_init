/*
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd.
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
#ifndef BASE_STARTUP_INIT_H
#define BASE_STARTUP_INIT_H

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#ifndef PARAM_VALUE_LEN_MAX
#define PARAM_VALUE_LEN_MAX 96
#endif

#define PROCESS_EXIT_CODE 0x7f // 0x7f: user specified
// kit framework
#define DEFAULT_UID_KIT_FRAMEWORK 3
// max length of one param/path
#define  MAX_ONE_ARG_LEN 200
#define  FD_HOLDER_BUFFER_SIZE 4096
#define INIT_PRIORITY_NICE (-20)

#define UNUSED(x) (void)(x)

#ifndef STARTUP_INIT_TEST
#ifndef INIT_STATIC
#define INIT_STATIC static
#endif
#else
#ifndef INIT_STATIC
#define INIT_STATIC
#endif
#endif

void SystemInit(void);
void LogInit(void);
void SystemPrepare(long long uptime);
void SystemConfig(const char *uptime);
void SystemRun(void);
#ifdef INIT_FEATURE_SUPPORT_SASPAWN
int DlopenSoLibrary(const char *configFile);
#endif
void SystemExecuteRcs(void);

int ParseCfgByPriority(const char *filePath);
int ParseInitCfg(const char *configFile, void *context);
void ReadConfig(void);
void SignalInit(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif // BASE_STARTUP_INIT_H
