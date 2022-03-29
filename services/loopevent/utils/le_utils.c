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
#include "le_utils.h"

#include <fcntl.h>

void SetNoBlock(int fd)
{
    int option = fcntl(fd, F_GETFD);
    if (option < 0) {
        return;
    }

    option = option | O_NONBLOCK | FD_CLOEXEC;
    option = fcntl(fd, F_SETFD, option);
    if (option < 0) {
        return;
    }
    return;
}