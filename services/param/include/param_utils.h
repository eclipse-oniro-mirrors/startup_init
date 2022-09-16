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

#include "beget_ext.h"
#include "init_param.h"
#include "init_utils.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef enum {
    PARAM_CODE_NOT_INIT = PARAM_CODE_MAX + 1,
    PARAM_CODE_ERROR_MAP_FILE,
} PARAM_INNER_CODE;

#ifndef PARAM_BUFFER_MAX
#define PARAM_BUFFER_MAX (0x01 << 16)
#endif

struct CmdLineEntry {
    char *key;
    int set;
};

typedef struct cmdLineInfo {
    const char *name;
    int (*processor)(const char *name, const char *value, int);
} cmdLineInfo;

#define FILENAME_LEN_MAX 255
#define MS_UNIT 1000
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif
#define PARAM_ALIGN(len) (((len) + 0x03) & (~0x03))
#define PARAM_IS_ALIGNED(x) (((x) & 0x03) == 0)
#define PARAM_ENTRY(ptr, type, member) (type *)((char *)(ptr)-offsetof(type, member))

#define IS_READY_ONLY(name) \
    ((strncmp((name), "const.", strlen("const.")) == 0) || (strncmp((name), "ro.", strlen("ro.")) == 0))
#define PARAM_PERSIST_PREFIX "persist."

#define SYS_POWER_CTRL "ohos.startup.powerctrl="
#define OHOS_CTRL_START "ohos.ctl.start="
#define OHOS_CTRL_STOP "ohos.ctl.stop="
#define OHOS_SERVICE_CTRL_PREFIX "ohos.servicectrl."
#define OHOS_BOOT "ohos.boot."

#ifdef STARTUP_INIT_TEST
#define PARAM_STATIC
#else
#define PARAM_STATIC static
#endif

#ifdef __LITEOS_M__
#ifndef DATA_PATH
#define DATA_PATH          "/"
#endif
#elif defined __LITEOS_A__
#define DATA_PATH          STARTUP_INIT_UT_PATH"/storage/data/system/param/"
#elif defined __LINUX__
#define DATA_PATH          STARTUP_INIT_UT_PATH"/storage/data/system/param/"
#else
#define DATA_PATH          STARTUP_INIT_UT_PATH"/data/service/el1/startup/parameters/"
#endif
#define PARAM_AREA_SIZE_CFG STARTUP_INIT_UT_PATH"/etc/param/ohos.para.size"

#define CLIENT_PIPE_NAME "/dev/unix/socket/paramservice"
#define PIPE_NAME STARTUP_INIT_UT_PATH "/dev/unix/socket/paramservice"
#define PARAM_STORAGE_PATH STARTUP_INIT_UT_PATH "/dev/__parameters__"
#define PARAM_PERSIST_SAVE_PATH DATA_PATH "persist_parameters"
#define PARAM_PERSIST_SAVE_TMP_PATH DATA_PATH "tmp_persist_parameters"

#define WORKSPACE_FLAGS_INIT 0x01
#define WORKSPACE_FLAGS_LOADED 0x02
#define WORKSPACE_FLAGS_UPDATE 0x04
#define WORKSPACE_FLAGS_LABEL_LOADED 0x08

#define PARAM_SET_FLAG(node, flag) ((node) |= (flag))
#define PARAM_CLEAR_FLAG(node, flag) ((node) &= ~(flag))
#define PARAM_TEST_FLAG(node, flag) (((node) & (flag)) == (flag))

#ifndef PARAN_DOMAIN
#define PARAN_DOMAIN (BASE_DOMAIN + 2)
#endif
#define PARAN_LABEL "PARAM"
#ifdef PARAM_BASE
INIT_LOCAL_API void ParamWorBaseLog(InitLogLevel logLevel, uint32_t domain, const char *tag, const char *fmt, ...);
#define PARAM_LOGV(fmt, ...) \
    ParamWorBaseLog(INIT_DEBUG, PARAN_DOMAIN, PARAN_LABEL, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define PARAM_LOGI(fmt, ...) \
    ParamWorBaseLog(INIT_INFO, PARAN_DOMAIN, PARAN_LABEL, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define PARAM_LOGW(fmt, ...) \
    ParamWorBaseLog(INIT_WARN, PARAN_DOMAIN, PARAN_LABEL, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#define PARAM_LOGE(fmt, ...) \
    ParamWorBaseLog(INIT_ERROR, PARAN_DOMAIN, PARAN_LABEL, "[%s:%d]" fmt, (FILE_NAME), (__LINE__), ##__VA_ARGS__)
#else
#define PARAM_LOGI(fmt, ...) STARTUP_LOGI(PARAN_DOMAIN, PARAN_LABEL, fmt, ##__VA_ARGS__)
#define PARAM_LOGE(fmt, ...) STARTUP_LOGE(PARAN_DOMAIN, PARAN_LABEL, fmt, ##__VA_ARGS__)
#define PARAM_LOGV(fmt, ...) STARTUP_LOGV(PARAN_DOMAIN, PARAN_LABEL, fmt, ##__VA_ARGS__)
#define PARAM_LOGW(fmt, ...) STARTUP_LOGW(PARAN_DOMAIN, PARAN_LABEL, fmt, ##__VA_ARGS__)
#endif

#define PARAM_CHECK(retCode, exper, ...) \
    if (!(retCode)) {                \
        PARAM_LOGE(__VA_ARGS__);     \
        exper;                       \
    }

#define PARAM_ONLY_CHECK(retCode, exper) \
    if (!(retCode)) {                \
        exper;                       \
    }

typedef int (*DUMP_PRINTF)(const char *fmt, ...);
#define PARAM_DUMP g_printf
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

INIT_LOCAL_API int SplitParamString(char *line, const char *exclude[], uint32_t count,
    int (*result)(const uint32_t *context, const char *name, const char *value), const uint32_t *context);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif