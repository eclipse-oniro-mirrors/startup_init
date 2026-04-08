/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef BEGETCTL_UNITTEST_MOCK_SHELL_MOCK_H
#define BEGETCTL_UNITTEST_MOCK_SHELL_MOCK_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "shell.h"

#ifdef __cplusplus
extern "C" {
#endif

// Shell Handle Mock Interface
typedef struct ShellHandleMock {
    void *handle;
    int (*execute)(void *handle, int argc, char **argv);
    int (*setParam)(void *handle, const char *name, int type, void *value);
} ShellHandleMock;

#ifdef __cplusplus
}
#endif

namespace init_ut {
namespace mock {

/**
 * @brief Shell操作Mock类
 * 用于模拟Shell环境中的命令执行和参数设置操作
 */
class ShellMock {
public:
    virtual ~ShellMock() = default;

    // Mock方法：注册参数命令
    MOCK_METHOD(int32_t, RegisterParamCmd, (BShellHandle shell, int execMode));

    // Mock方法：直接执行命令
    MOCK_METHOD(int, DirectExecute, (BShellHandle shell, int argc, char **argv));

    // Mock方法：设置Shell参数
    MOCK_METHOD(int, SetParam, (BShellHandle shell, const char *name, int type, void *value));

    // Mock方法：获取Shell句柄
    MOCK_METHOD(BShellHandle, GetShellHandle, ());
};

/**
 * @brief 获取全局Mock实例
 */
ShellMock& GetShellMock();

/**
 * @brief 设置全局Mock实例
 * @param mock Mock实例指针，nullptr表示使用真实实现
 */
void SetShellMock(ShellMock* mock);

} // namespace mock
} // namespace init_ut

#endif // BEGETCTL_UNITTEST_MOCK_SHELL_MOCK_H
