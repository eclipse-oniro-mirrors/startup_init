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
#include "device_info_proxy.h"

#include "beget_ext.h"
#include "idevice_info.h"
#include "parcel.h"
#include "string_ex.h"

namespace OHOS {
namespace device_info {
int32_t DeviceInfoProxy::GetUdid(std::string& result)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option { MessageOption::TF_SYNC };
    data.WriteInterfaceToken(DeviceInfoProxy::GetDescriptor());
    int32_t ret = Remote()->SendRequest(COMMAND_GET_UDID, data, reply, option);
    DINFO_CHECK(ret == ERR_NONE, return ret, "getUdid failed, error code is %d", ret);
    result = Str16ToStr8(reply.ReadString16());
    return ERR_OK;
}

int32_t DeviceInfoProxy::GetSerialID(std::string& result)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option { MessageOption::TF_SYNC };

    data.WriteInterfaceToken(DeviceInfoProxy::GetDescriptor());
    int32_t ret = Remote()->SendRequest(COMMAND_GET_SERIAL_ID, data, reply, option);
    DINFO_CHECK(ret == ERR_NONE, return ret, "GetSerial failed, error code is %d", ret);
    result = Str16ToStr8(reply.ReadString16());
    return ERR_OK;
}
} // namespace device_info
} // namespace OHOS
