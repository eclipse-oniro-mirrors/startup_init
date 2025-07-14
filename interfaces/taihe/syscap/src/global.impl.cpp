/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "global.proj.hpp"
#include "global.impl.hpp"
#include "taihe/runtime.hpp"
#include "stdexcept"
#include "systemcapability.h"
#include "beget_ext.h"

using namespace taihe;
namespace {
// To be implemented.

bool canIUse(string_view syscap)
{
    bool ret = HasSystemCapability(std::string(syscap).c_str());
    return ret;
}
}  // namespace

// Since these macros are auto-generate, lint will cause false positive.
// NOLINTBEGIN
TH_EXPORT_CPP_API_canIUse(canIUse);
// NOLINTEND
