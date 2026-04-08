/**
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

#include "shell_mock.h"
#include <memory>

namespace init_ut {
namespace mock {

// Thread-local Mock实例指针
thread_local static ShellMock* g_shellMockInstance = nullptr;

ShellMock& GetShellMock()
{
    if (g_shellMockInstance == nullptr) {
        // 创建NiceMock，避免未预期的调用导致测试失败
        static auto defaultMock = std::make_unique<::testing::NiceMock<ShellMock>>();
        g_shellMockInstance = defaultMock.get();
    }
    return *g_shellMockInstance;
}

void SetShellMock(ShellMock* mock)
{
    g_shellMockInstance = mock;
}

} // namespace mock
} // namespace init_ut
