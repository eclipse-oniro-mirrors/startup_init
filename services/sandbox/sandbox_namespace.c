/*
* Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <string.h>
#include <unistd.h>
#include "beget_ext.h"

static int g_defaultNs;

int GetNamespaceFd(const char *nsPath)
{
    BEGET_CHECK(nsPath != NULL, return -1);
    int ns = open(nsPath, O_RDONLY | O_CLOEXEC);
    if (ns < 0) {
        BEGET_LOGE("Open default namespace failed, err=%d", errno);
        return -1;
    }
    return ns;
}

int UnshareNamespace(int nsType)
{
    if (nsType == CLONE_NEWNS) {
        if (unshare(nsType) < 0) {
            BEGET_LOGE("Unshare namespace failed, err=%d", errno);
            return -1;
        } else {
            return 0;
        }
    } else {
        BEGET_LOGE("Namespace type is not support");
        return -1;
    }
}

int SetNamespace(int nsFd, int nsType)
{
    if (nsFd < 0) {
        BEGET_LOGE("Namespace fd is invalid");
        return -1;
    }
    if (nsType != CLONE_NEWNS) {
        BEGET_LOGE("Namespace type is not support");
        return -1;
    }
    return setns(nsFd, nsType);
}

void InitDefaultNamespace(void)
{
    BEGET_CHECK(!(g_defaultNs > 0), (void)close(g_defaultNs));
    g_defaultNs = GetNamespaceFd("/proc/self/ns/mnt");
    return;
}

int EnterDefaultNamespace(void)
{
    BEGET_CHECK(!(g_defaultNs < 0), return -1);
    return SetNamespace(g_defaultNs, CLONE_NEWNS);
}

void CloseDefaultNamespace(void)
{
    if (g_defaultNs > 0) {
        (void)close(g_defaultNs);
        g_defaultNs = -1;
    }
    return;
}
