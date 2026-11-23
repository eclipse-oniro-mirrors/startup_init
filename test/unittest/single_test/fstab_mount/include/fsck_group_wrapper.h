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
#ifndef FSCK_GROUP_WRAPPER_H
#define FSCK_GROUP_WRAPPER_H
#include <grp.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

struct group *__real_getgrnam(const char *name);
typedef struct group *(*GetgrnamFunc)(const char *name);
void UpdateGetgrnamFunc(GetgrnamFunc func);

int __real_getgroups(int size, gid_t list[]);
typedef int (*GetgroupsFunc)(int size, gid_t list[]);
void UpdateGetgroupsFunc(GetgroupsFunc func);

int __real_setgroups(size_t size, const gid_t *list);
typedef int (*SetgroupsFunc)(size_t size, const gid_t *list);
void UpdateSetgroupsFunc(SetgroupsFunc func);

#ifdef __cplusplus
}
#endif
#endif // FSCK_GROUP_WRAPPER_H
