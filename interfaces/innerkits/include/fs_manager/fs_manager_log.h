/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef STARTUP_FS_MANAGER_LOG_H
#define STARTUP_FS_MANAGER_LOG_H

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef enum LOGTARGET {
    LOG_TO_KERNEL,
    LOG_TO_FILE,
    LOG_TO_STDIO
} LogTarget;

typedef enum FsMgrLogLevel {
    FSMGR_VERBOSE,
    FSMGR_DEBUG,
    FSMGR_INFO,
    FSMGR_WARNING,
    FSMGR_ERROR,
    FSMGR_FATAL
} FsMgrLogLevel;

void FsManagerLogInit(LogTarget target, const char *fileName);
void FsManagerLogDeInit(void);
void FsManagerLogToStd(FsMgrLogLevel level, const char *fileName, int line, const char *fmt, ...);
typedef void (*FsManagerLogFunc)(FsMgrLogLevel level, const char *fileName, int line, const char *fmt, ...);
extern FsManagerLogFunc g_logFunc;

#define FILE_NAME   (strrchr((__FILE__), '/') ? strrchr((__FILE__), '/') + 1 : (__FILE__))
#define FSMGR_LOGV(fmt, ...) g_logFunc(FSMGR_VERBOSE, (FILE_NAME), (__LINE__), fmt"\n", ##__VA_ARGS__)
#define FSMGR_LOGD(fmt, ...) g_logFunc(FSMGR_DEBUG, (FILE_NAME), (__LINE__), fmt"\n", ##__VA_ARGS__)
#define FSMGR_LOGI(fmt, ...) g_logFunc(FSMGR_INFO, (FILE_NAME), (__LINE__), fmt"\n", ##__VA_ARGS__)
#define FSMGR_LOGW(fmt, ...) g_logFunc(FSMGR_WARNING, (FILE_NAME), (__LINE__), fmt"\n", ##__VA_ARGS__)
#define FSMGR_LOGE(fmt, ...) g_logFunc(FSMGR_ERROR, (FILE_NAME), (__LINE__), fmt"\n", ##__VA_ARGS__)
#define FSMGR_LOGF(fmt, ...) g_logFunc(FSMGR_FATAL, (FILE_NAME), (__LINE__), fmt"\n", ##__VA_ARGS__)
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // STARTUP_FS_MANAGER_LOG_H
