/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include "watcher.h"
#include "watcher_utils.h"

namespace OHOS {
namespace init_param {
int32_t Watcher::OnParameterChange(const std::string &prefix, const std::string &name, const std::string &value)
{
    UNUSED(name);
    UNUSED(value);
    return 0;
}
} // namespace init_param
} // namespace OHOS
