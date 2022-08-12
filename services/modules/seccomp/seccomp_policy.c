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
#include "seccomp_filters.h"
#include "plugin_adapter.h"
#ifdef SECCOMP_PLUGIN
#include "init_module_engine.h"
#endif

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

static bool IsSupportFilterFlag(unsigned int filterFlag)
{
    errno = 0;
    int ret = syscall(__NR_seccomp, SECCOMP_SET_MODE_FILTER, filterFlag, NULL);
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

bool SetSeccompPolicy(PolicyType policy)
{
    bool ret = false;
    switch (policy) {
        case SYSTEM:
            ret = InstallSeccompPolicy(g_systemSeccompFilter, g_systemSeccompFilterSize, SECCOMP_FILTER_FLAG_LOG);
            break;
        case APPSPAWN:
            ret = InstallSeccompPolicy(g_appspawnSeccompFilter, g_appspawnSeccompFilterSize, SECCOMP_FILTER_FLAG_LOG);
            break;
        case NWEBSPAWN:
            ret = InstallSeccompPolicy(g_nwebspawnSeccompFilter, g_nwebspawnSeccompFilterSize, SECCOMP_FILTER_FLAG_LOG);
            break;
        default:
            ret = false;
    }

    return ret;
}

#ifdef SECCOMP_PLUGIN
static int DoSetSeccompPolicyStart(void)
{
    bool ret = false;
    ret = SetSeccompPolicy(SYSTEM);
    PLUGIN_CHECK(ret == true, return -1, "SetSeccompPolicy failed");

    return 0;
}

static int DoSetSeccompPolicyCmd(int id, const char *name, int argc, const char **argv)
{
    PLUGIN_LOGI("DoBootchartCmd argc %d %s", argc, name);
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");
    if (strcmp(argv[0], "start") == 0) {
        return DoSetSeccompPolicyStart();
    }
    return 0;
}

static int32_t g_executorId = -1;
static int SetSeccompPolicyInit(void)
{
    if (g_executorId == -1) {
        g_executorId = AddCmdExecutor("SetSeccompPolicy", DoSetSeccompPolicyCmd);
        PLUGIN_LOGI("SetSeccompPolicy executorId %d", g_executorId);
    }
    return 0;
}

static void SetSeccompPolicyExit(void)
{
    PLUGIN_LOGI("SetSeccompPolicy executorId %d", g_executorId);
    if (g_executorId != -1) {
        RemoveCmdExecutor("SetSeccompPolicy", g_executorId);
    }
}

MODULE_CONSTRUCTOR(void)
{
    PLUGIN_LOGI("DoSetSeccompPolicyStart now ...");
    SetSeccompPolicyInit();
}

MODULE_DESTRUCTOR(void)
{
    PLUGIN_LOGI("DoSetSeccompPolicyStop now ...");
    SetSeccompPolicyExit();
}
#endif
