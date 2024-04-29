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

#include "seccomp_policy.h"
#include "plugin_adapter.h"
#include "securec.h"
#include "config_policy_utils.h"

#ifdef WITH_SECCOMP_DEBUG
#include "init_utils.h"
#include "init_param.h"
#endif

#include <dlfcn.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <linux/audit.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <limits.h>

#ifndef SECCOMP_SET_MODE_FILTER
#define SECCOMP_SET_MODE_FILTER  (1)
#endif

#ifdef __aarch64__
#define FILTER_LIB_PATH_FORMAT "lib64/seccomp/lib%s_filter.z.so"
#define FILTER_LIB_PATH_PART "lib64/seccomp/lib"
#else
#define FILTER_LIB_PATH_FORMAT "lib/seccomp/lib%s_filter.z.so"
#define FILTER_LIB_PATH_PART "lib/seccomp/lib"
#endif
#define FILTER_NAME_FORMAT "g_%sSeccompFilter"
#define FILTER_SIZE_STRING "Size"

typedef enum {
    SECCOMP_SUCCESS,
    INPUT_ERROR,
    RETURN_NULL,
    RETURN_ERROR
} SeccompErrorCode;

static bool IsSupportFilterFlag(unsigned int filterFlag)
{
    errno = 0;
    long ret = syscall(__NR_seccomp, SECCOMP_SET_MODE_FILTER, filterFlag, NULL);
    if (ret != -1 || errno != EFAULT) {
        PLUGIN_LOGE("not support  seccomp flag %u", filterFlag);
        return false;
    }

    return true;
}

static bool InstallSeccompPolicy(const struct sock_filter* filter, size_t filterSize, unsigned int filterFlag)
{
    if (filter == NULL) {
        return false;
    }

    unsigned int flag = 0;
    struct sock_fprog prog = {
        (unsigned short)filterSize,
        (struct sock_filter*)filter
    };

    if (IsSupportFilterFlag(SECCOMP_FILTER_FLAG_TSYNC) && (filterFlag & SECCOMP_FILTER_FLAG_TSYNC)) {
        flag |= SECCOMP_FILTER_FLAG_TSYNC;
    }

    if (IsSupportFilterFlag(SECCOMP_FILTER_FLAG_LOG) && (filterFlag & SECCOMP_FILTER_FLAG_LOG)) {
        flag |= SECCOMP_FILTER_FLAG_LOG;
    }

    if (syscall(__NR_seccomp, SECCOMP_SET_MODE_FILTER, flag, &prog) != 0) {
        PLUGIN_LOGE("SetSeccompFilter failed");
        return false;
    }

    return true;
}

static bool GetFilterFileByName(const char *filterName, char *filterLibRealPath, unsigned int pathSize)
{
    size_t maxFilterNameLen = PATH_MAX - strlen(FILTER_LIB_PATH_FORMAT) + strlen("%s") - 1;
    if (filterName == NULL || strlen(filterName) > maxFilterNameLen) {
        return false;
    }

    bool flag = false;
    char filterLibPath[PATH_MAX] = {0};

    int rc = snprintf_s(filterLibPath, sizeof(filterLibPath), \
                            strlen(filterName) + strlen(FILTER_LIB_PATH_FORMAT) - strlen("%s"), \
                            FILTER_LIB_PATH_FORMAT, filterName);
    if (rc == -1) {
        return false;
    }

    int seccompPathNum = 0;
    CfgFiles *files = GetCfgFiles(filterLibPath);
    for (int i = MAX_CFG_POLICY_DIRS_CNT - 1; files && i >= 0; i--) {
        if (files->paths[i]) {
            seccompPathNum++;
        }
    }

    // allow only one path to a seccomp shared library to avoid shared library replaced
    if (seccompPathNum == 1 && files && files->paths[0]) {
        if (memcpy_s(filterLibRealPath, pathSize, files->paths[0], strlen(files->paths[0]) + 1) == EOK) {
            flag = true;
        }
    }
    FreeCfgFiles(files);

    return flag;
}

static int GetSeccompPolicy(const char *filterName, int **handler,
                            const char *filterLibRealPath, struct sock_fprog *prog)
{
    if (filterName == NULL || filterLibRealPath == NULL || handler == NULL || prog == NULL) {
        return INPUT_ERROR;
    }

    if (strstr(filterLibRealPath, FILTER_LIB_PATH_PART) == NULL) {
        return INPUT_ERROR;
    }

    char filterVaribleName[PATH_MAX] = {0};
    struct sock_filter *filter = NULL;
    size_t *filterSize = NULL;
    void *policyHanlder = NULL;
    int ret = SECCOMP_SUCCESS;
    do {
        int rc = snprintf_s(filterVaribleName, sizeof(filterVaribleName), \
                    strlen(filterName) + strlen(FILTER_NAME_FORMAT) - strlen("%s"), \
                    FILTER_NAME_FORMAT, filterName);
        if (rc == -1) {
            return RETURN_ERROR;
        }
        char realPath[PATH_MAX] = { 0 };
        realpath(filterLibRealPath, realPath);
        policyHanlder = dlopen(realPath, RTLD_LAZY);
        if (policyHanlder == NULL) {
            return RETURN_NULL;
        }

        filter = (struct sock_filter *)dlsym(policyHanlder, filterVaribleName);
        if (filter == NULL) {
            ret = RETURN_NULL;
            break;
        }

        size_t filterVaribleNameLen = strlen(filterVaribleName) + \
                      strlen(FILTER_SIZE_STRING) + 1;
        if (filterVaribleNameLen > sizeof(filterVaribleName)) {
            ret = RETURN_ERROR;
            break;
        }
        rc = strcat_s(filterVaribleName, filterVaribleNameLen, FILTER_SIZE_STRING);
        if (rc != 0) {
            ret = RETURN_ERROR;
            break;
        }

        filterSize = (size_t *)dlsym(policyHanlder, filterVaribleName);
        if (filterSize == NULL) {
            ret = RETURN_NULL;
            break;
        }
    } while (0);

    *handler = (int *)policyHanlder;
    prog->filter = filter;
    if (filterSize != NULL) {
        prog->len = (unsigned short)(*filterSize);
    }

    return ret;
}


bool IsEnableSeccomp(void)
{
    bool isEnableSeccompFlag = true;
#ifdef WITH_SECCOMP_DEBUG
    char value[MAX_BUFFER_LEN] = {0};
    unsigned int len = MAX_BUFFER_LEN;
    if (SystemReadParam("persist.init.debug.seccomp.enable", value, &len) == 0) {
        if (strncmp(value, "0", len) == 0) {
            isEnableSeccompFlag = false;
        }
    }
#endif
    return isEnableSeccompFlag;
}

bool SetSeccompPolicyWithName(SeccompFilterType type, const char *filterName)
{
    if (filterName == NULL) {
        return false;
    }

#ifdef WITH_SECCOMP_DEBUG
    if (!IsEnableSeccomp()) {
        return true;
    }
#endif

    void *handler = NULL;
    char filterLibRealPath[PATH_MAX] = {0};
    struct sock_fprog prog;
    bool ret = false;
    const char *filterNamePtr = filterName;

    bool flag = GetFilterFileByName(filterNamePtr, filterLibRealPath, sizeof(filterLibRealPath));
    if (!flag) {
        if (type == SYSTEM_SA) {
            filterNamePtr = SYSTEM_NAME;
            flag = GetFilterFileByName(filterNamePtr, filterLibRealPath, sizeof(filterLibRealPath));
            PLUGIN_CHECK(flag == true, return ret, "get filter name failed");
        } else if (type == SYSTEM_OTHERS) {
            return true;
        } else {
            PLUGIN_LOGE("get filter name failed");
            return ret;
        }
    }

    int retCode = GetSeccompPolicy(filterNamePtr, (int **)&handler, filterLibRealPath, &prog);
    if (retCode == SECCOMP_SUCCESS) {
        ret = InstallSeccompPolicy(prog.filter, prog.len, SECCOMP_FILTER_FLAG_LOG);
    } else {
        PLUGIN_LOGE("get seccomp policy failed return is %d and path is %s", retCode, filterLibRealPath);
    }
#ifndef COVERAGE_TEST
    if (handler != NULL) {
        dlclose(handler);
    }
#endif
    return ret;
}
