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

#ifndef LE_SOCKET_H
#define LE_SOCKET_H
#include "le_utils.h"
#include "loop_event.h"
#include "beget_ext.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define SOCKET_TIMEOUT 3

INIT_LOCAL_API
int CreateSocket(int flags, const char *server);
INIT_LOCAL_API
int AcceptSocket(int fd, int flags);
INIT_LOCAL_API
int listenSocket(int fd, int flags, const char *server);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif