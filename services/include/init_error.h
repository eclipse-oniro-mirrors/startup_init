/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef INIT_ERROR_H
#define INIT_ERROR_H

#include "beget_ext.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/* Expand this list if necessary. */
#define INIT_ERRNO_MAP(XX)                  \
    XX(EPARAMETER, "Invalid parameter")     \
    XX(EFORMAT, "Format string fail")       \
    XX(ECFG, "cfg error")                   \
    XX(EPATH, "Invalid path")               \
    XX(EFORK, "Fork fail")                  \
    XX(ESANDBOX, "Create sandbox fail")     \
    XX(EACCESSTOKEN, "Set access token fail")   \
    XX(ESOCKET, "Create socket fail")       \
    XX(EFILE, "Create file fail")           \
    XX(ECONSOLE, "Open console fail")       \
    XX(EHOLDER, "Publish holder fail")      \
    XX(EBINDCORE, "Bind core fail")         \
    XX(EKEEPCAP, "Set keep capability fail")    \
    XX(EGIDSET, "Set gid fail")             \
    XX(ESECCOMP, "Set SECCOMP fail")        \
    XX(EUIDSET, "Set uid fail")             \
    XX(ECAP, "Set capability fail")         \
    XX(EWRITEPID, "Write pid fail")         \
    XX(ECONTENT, "Set sub content fail")    \
    XX(EPRIORITY, "Set priority fail")      \
    XX(EEXEC_CONTENT, "Set exec content fail")  \
    XX(EEXEC, "Exec fail")

typedef enum {
    INIT_OK,
#define XX(code, _) INIT_ ## code,
    INIT_ERRNO_MAP(XX)
#undef XX
} InitErrno;

struct InitErrMap {
    int code;
    const char *info;
} ;

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif // INIT_UTILS_H
