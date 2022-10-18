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
#include "init_log.h"
#include "init_socket.h"
#include "securec.h"
#define MAX_BUFFER_SIZE 1024

int main(int argc, char* argv[])
{
    int sockFd = GetControlSocket("serversock");
    if (sockFd < 0) {
        INIT_LOGE("Failed to get server socket");
        return -1;
    }
    char buffer[MAX_BUFFER_SIZE] = { 0 };
    while (1) {
        if (read(sockFd, buffer, sizeof(buffer) - 1) > 0) {
            INIT_LOGI("Read message: %s", buffer);
            (void)memset_s(buffer, MAX_BUFFER_SIZE, 0, MAX_BUFFER_SIZE);
        }
    }
    return 0;
}
