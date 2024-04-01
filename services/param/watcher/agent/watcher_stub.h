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

#ifndef STARTUP_WATCHER_STUB_H_
#define STARTUP_WATCHER_STUB_H_

#include "iremote_stub.h"
#include "iwatcher.h"
#include "message_parcel.h"
#include "parcel.h"

namespace OHOS {
namespace init_param {
class WatcherStub : public IRemoteStub<IWatcher> {
public:
    explicit WatcherStub(bool serialInvokeFlag = true)
        : IRemoteStub(serialInvokeFlag), serialInvokeFlag_(serialInvokeFlag) {}
    int32_t OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option) override;
    bool serialInvokeFlag_ = { true };
};
} // namespace init_param
} // namespace OHOS
#endif // !defined(STARTUP_WATCHER_STUB_H_)
