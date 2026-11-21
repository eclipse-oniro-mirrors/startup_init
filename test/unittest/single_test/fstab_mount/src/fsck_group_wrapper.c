/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "fsck_group_wrapper.h"

static GetgrnamFunc g_getgrnam = NULL;
void UpdateGetgrnamFunc(GetgrnamFunc func)
{
    g_getgrnam = func;
}

struct group *__wrap_getgrnam(const char *name)
{
    if (g_getgrnam) {
        return g_getgrnam(name);
    } else {
        return __real_getgrnam(name);
    }
}

static GetgroupsFunc g_getgroups = NULL;
void UpdateGetgroupsFunc(GetgroupsFunc func)
{
    g_getgroups = func;
}

int __wrap_getgroups(int size, gid_t list[])
{
    if (g_getgroups) {
        return g_getgroups(size, list);
    } else {
        return __real_getgroups(size, list);
    }
}

static SetgroupsFunc g_setgroups = NULL;
void UpdateSetgroupsFunc(SetgroupsFunc func)
{
    g_setgroups = func;
}

int __wrap_setgroups(size_t size, const gid_t *list)
{
    if (g_setgroups) {
        return g_setgroups(size, list);
    } else {
        return __real_setgroups(size, list);
    }
}
