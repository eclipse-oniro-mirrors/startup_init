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

#ifndef START_WATCHER_H
#define START_WATCHER_H

#include <iostream>
#include "watcher_stub.h"

namespace OHOS {
namespace init_param {
class Watcher : public WatcherStub {
public:
    Watcher() = default;
    virtual ~Watcher() = default;

    void OnParameterChange(const std::string &prefix, const std::string &name, const std::string &value) override;
};
} // namespace init_param
} // namespace OHOS
#endif // START_WATCHER_H