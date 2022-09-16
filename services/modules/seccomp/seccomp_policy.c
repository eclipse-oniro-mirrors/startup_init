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

#ifndef SECCOMP_SET_MODE_FILTER
#define SECCOMP_SET_MODE_FILTER  (1)
#endif

#ifdef __aarch64__
#define FILTER_LIB_PATH_FORMAT "/system/lib64/lib%s_filter.z.so"
#else
#define FILTER_LIB_PATH_FORMAT "/system/lib/lib%s_filter.z.so"
#endif
#define FILTER_NAME_FORMAT "g_%sSeccompFilter"
#define FILTER_SIZE_STRING "Size"

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

bool SetSeccompPolicyWithName(const char *filterName)
{
    char filterLibPath[512] = {0};
    char filterVaribleName[512] = {0};
    struct sock_filter *filterPtr = NULL;
    size_t *filterSize = NULL;

    int rc = snprintf_s(filterLibPath, sizeof(filterLibPath), \
                        strlen(filterName) + strlen(FILTER_LIB_PATH_FORMAT) - strlen("%s"), \
                        FILTER_LIB_PATH_FORMAT, filterName);
    PLUGIN_CHECK(rc != -1, return false, "snprintf_s filterLibPath failed");

    rc = snprintf_s(filterVaribleName, sizeof(filterVaribleName), \
                    strlen(filterName) + strlen(FILTER_NAME_FORMAT) - strlen("%s"), \
                    FILTER_NAME_FORMAT, filterName);
    PLUGIN_CHECK(rc != -1, return false, "snprintf_s  faiVribleName failed");

    void *handler = dlopen(filterLibPath, RTLD_LAZY);
    PLUGIN_CHECK(handler != NULL, return false, "dlopen %s failed", filterLibPath);

    filterPtr = (struct sock_filter *)dlsym(handler, filterVaribleName);
    PLUGIN_CHECK(filterPtr != NULL, dlclose(handler);
        return false, "dlsym %s failed", filterVaribleName);

    rc = strcat_s(filterVaribleName, strlen(filterVaribleName) + strlen(FILTER_SIZE_STRING) + 1, FILTER_SIZE_STRING);
    PLUGIN_CHECK(rc == 0, dlclose(handler);
        return false, "strcat_s filterVaribleName failed");

    filterSize = (size_t *)dlsym(handler, filterVaribleName);
    PLUGIN_CHECK(filterSize != NULL, dlclose(handler);
        return false, "dlsym %s failed", filterVaribleName);

    bool ret = InstallSeccompPolicy(filterPtr, *filterSize, SECCOMP_FILTER_FLAG_LOG);

    dlclose(handler);

    return ret;
}
