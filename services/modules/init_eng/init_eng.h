/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef _PLUGIN_INITENG_H
#define _PLUGIN_INITENG_H

typedef enum {
    TYPE_DIR = 0,
    TYPE_REG,
    TYPE_LINK,
    TYPE_ANY
} FileType;

#define MOUNT_CMD_MAX_LEN 128U

#ifdef STARTUP_INIT_TEST
#define ENG_STATIC
#else
#define ENG_STATIC static
#endif

#endif /* _PLUGIN_INITENG_H */
