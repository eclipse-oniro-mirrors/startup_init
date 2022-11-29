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
#define FILTER_LIB_PATH_FORMAT "/system/lib64/lib%s_filter.z.so"
#define FILTER_LIB_PATH_HEAD "/system/lib64/lib"
#else
#define FILTER_LIB_PATH_FORMAT "/system/lib/lib%s_filter.z.so"
#define FILTER_LIB_PATH_HEAD "/system/lib/lib"
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

static char *GetFilterFileByName(const char *filterName)
{
    size_t maxFilterNameLen = PATH_MAX - strlen(FILTER_LIB_PATH_FORMAT) + strlen("%s") - 1;
    if (filterName == NULL || strlen(filterName) > maxFilterNameLen) {
        return NULL;
    }

    char filterLibPath[PATH_MAX] = {0};

    int rc = snprintf_s(filterLibPath, sizeof(filterLibPath), \
                            strlen(filterName) + strlen(FILTER_LIB_PATH_FORMAT) - strlen("%s"), \
                            FILTER_LIB_PATH_FORMAT, filterName);
    if (rc == -1) {
        return NULL;
    }

    return realpath(filterLibPath, NULL);
}

static int GetSeccompPolicy(const char *filterName, int **handler,
                            char *filterLibRealPath, struct sock_fprog *prog)
{
    if (filterName == NULL || filterLibRealPath == NULL || \
        handler == NULL || prog == NULL) {
        return INPUT_ERROR;
    }

    if (strncmp(filterLibRealPath, FILTER_LIB_PATH_HEAD, strlen(FILTER_LIB_PATH_HEAD))) {
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
            ret = RETURN_ERROR;
            break;
        }

        policyHanlder = dlopen(filterLibRealPath, RTLD_LAZY);
        if (policyHanlder == NULL) {
            ret = RETURN_NULL;
            break;
        }

        filter = (struct sock_filter *)dlsym(policyHanlder, filterVaribleName);
        if (filter == NULL) {
            ret = RETURN_NULL;
            break;
        }

        rc = strcat_s(filterVaribleName, strlen(filterVaribleName) + \
                      strlen(FILTER_SIZE_STRING) + 1, FILTER_SIZE_STRING);
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

bool SetSeccompPolicyWithName(const char *filterName)
{
    if (filterName == NULL) {
        return false;
    }

    void *handler = NULL;
    char *filterLibRealPath = NULL;
    struct sock_fprog prog;
    bool ret = false;

    filterLibRealPath = GetFilterFileByName(filterName);
    PLUGIN_CHECK(filterLibRealPath != NULL, return false, "get filter file name faield");

    int retCode = GetSeccompPolicy(filterName, (int **)&handler, filterLibRealPath, &prog);
    if (retCode == SECCOMP_SUCCESS) {
        ret = InstallSeccompPolicy(prog.filter, prog.len, SECCOMP_FILTER_FLAG_LOG);
    } else {
        PLUGIN_LOGE("GetSeccompPolicy failed return is %d", retCode);
    }

    if (handler != NULL) {
        dlclose(handler);
    }

    if (filterLibRealPath != NULL) {
        free(filterLibRealPath);
    }

    return ret;
}
