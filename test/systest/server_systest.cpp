/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include <cstdio>
#include <string>
#include <iostream>

#include "loop_event.h"
#include "le_socket.h"


int main()
{
    printf("请输入创建socket的类型：(pipe, tcp)\n");
    std::string socket_type;
    std::cin >> socket_type;

    int type;
    std::string path;
    if (socket_type == "pipe") {
        type = TASK_STREAM | TASK_PIPE |TASK_SERVER | TASK_TEST;
        path = "/data/testpipe";
    } else if (socket_type == "tcp") {
        type = TASK_STREAM | TASK_TCP |TASK_SERVER | TASK_TEST;
        path = "127.0.0.1:7777";
    } else {
        printf("输入有误，请输入pipe或者tcp!");
        system("pause");
        return 0;
    }

    char *server = path.c_str();
    int fd = CreateSocket(type, server);
    if (fd == -1) {
        printf("Create socket failed!\n");
        system("pause");
        return 0;
    }

    int ret = listenSocket(fd, type, server);
    if (ret != 0) {
        printf("Failed to listen socket %s\n", server);
        system("pause");
        return 0;
    }

    while (true) {
        int clientfd = AcceptSocket(fd, type);
        if (clientfd == -1) {
            printf("Failed to accept socket %s\n", server);
            continue;
        }

        std::cout << "Accept connection from %d" << clientfd << std::endl;
        break;
    }
    return 0;
}