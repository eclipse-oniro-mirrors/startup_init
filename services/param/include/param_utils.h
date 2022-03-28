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

#ifndef BASE_STARTUP_PARAM_UTILS_H
#define BASE_STARTUP_PARAM_UTILS_H
#include <stddef.h>
#include <stdint.h>

#include "init_log.h"
#include "securec.h"
#include "sys_param.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef enum {
    PARAM_CODE_NOT_INIT = PARAM_CODE_MAX + 1,
    PARAM_CODE_ERROR_MAP_FILE,
} PARAM_INNER_CODE;

#define MS_UNIT 1000
#define UNUSED(x) (void)(x)
#define PARAM_ALIGN(len) (((len) + 0x03) & (~0x03))
#define PARAM_ENTRY(ptr, type, member) (type *)((char *)(ptr)-offsetof(type, member))

#define IS_READY_ONLY(name) \
    ((strncmp((name), "const.", strlen("const.")) == 0) || (strncmp((name), "ro.", strlen("ro.")) == 0))
#define PARAM_PERSIST_PREFIX "persist."

#define SYS_POWER_CTRL "ohos.startup.powerctrl="
#define OHOS_CTRL_START "ohos.ctl.start="
#define OHOS_CTRL_STOP "ohos.ctl.stop="
#define OHOS_SERVICE_CTRL_PREFIX "ohos.servicectrl."
#define OHOS_BOOT "ohos.boot."

#define CLIENT_PIPE_NAME "/dev/unix/socket/paramservice"
#define CLIENT_PARAM_STORAGE_PATH "/dev/__parameters__/param_storage"

#ifdef STARTUP_INIT_TEST
#define PARAM_STATIC
#define PARAM_DEFAULT_PATH "/data/init_ut"
#define PIPE_NAME PARAM_DEFAULT_PATH"/param/paramservice"
#define PARAM_STORAGE_PATH PARAM_DEFAULT_PATH "/__parameters__/param_storage"
#define PARAM_PERSIST_SAVE_PATH PARAM_DEFAULT_PATH "/param/persist_parameters"
#define PARAM_PERSIST_SAVE_TMP_PATH PARAM_DEFAULT_PATH "/param/tmp_persist_parameters"
#else
#define PARAM_DEFAULT_PATH ""
#define PARAM_STATIC static
#define PIPE_NAME "/dev/unix/socket/paramservice"
#define PARAM_STORAGE_PATH "/dev/__parameters__/param_storage"
#define PARAM_PERSIST_SAVE_PATH "/data/parameters/persist_parameters"
#define PARAM_PERSIST_SAVE_TMP_PATH "/data/parameters/tmp_persist_parameters"
#endif

#define PARAM_CMD_LINE "/proc/cmdline"
#define GROUP_FILE_PATH "/etc/group"
#define USER_FILE_PATH "/etc/passwd"

#define WORKSPACE_FLAGS_INIT 0x01
#define WORKSPACE_FLAGS_LOADED 0x02
#define WORKSPACE_FLAGS_UPDATE 0x04
#define WORKSPACE_FLAGS_LABEL_LOADED 0x08

#define PARAM_SET_FLAG(node, flag) ((node) |= (flag))
#define PARAM_CLEAR_FLAG(node, flag) ((node) &= ~(flag))
#define PARAM_TEST_FLAG(node, flag) (((node) & (flag)) == (flag))

#define PARAN_LOG_FILE "param.log"
#define PARAN_LABEL "PARAM"
#define PARAM_LOGI(fmt, ...) STARTUP_LOGI(PARAN_LOG_FILE, PARAN_LABEL, fmt, ##__VA_ARGS__)
#define PARAM_LOGE(fmt, ...) STARTUP_LOGE(PARAN_LOG_FILE, PARAN_LABEL, fmt, ##__VA_ARGS__)
#define PARAM_LOGV(fmt, ...) STARTUP_LOGV(PARAN_LOG_FILE, PARAN_LABEL, fmt, ##__VA_ARGS__)

#define PARAM_CHECK(retCode, exper, ...) \
    if (!(retCode)) {                \
        PARAM_LOGE(__VA_ARGS__);     \
        exper;                       \
    }

#ifdef INIT_AGENT
#define PARAM_DUMP printf
#else
#define PARAM_DUMP PARAM_LOGI
#endif

#define MAX_LABEL_LEN 256
#define PARAM_BUFFER_SIZE 256

#define SUBSTR_INFO_NAME 0
#define SUBSTR_INFO_VALUE 1
#ifdef PARAM_SUPPORT_SELINUX
#define SUBSTR_INFO_LABEL 1
#define SUBSTR_INFO_DAC 2
#else
#define SUBSTR_INFO_LABEL 1
#define SUBSTR_INFO_DAC 1
#endif

typedef struct {
    int length;
    char value[PARAM_BUFFER_SIZE];
} SubStringInfo;

void CheckAndCreateDir(const char *fileName);
int GetSubStringInfo(const char *buff, uint32_t buffLen, char delimiter, SubStringInfo *info, int subStrNumber);
int SpliteString(char *line, const char *exclude[], uint32_t count,
    int (*result)(const uint32_t *context, const char *name, const char *value), const uint32_t *context);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif