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
#include "ohos.boardInfo.impl.hpp"

#include "beget_ext.h"
#include "parameter.h"
#include "init_error.h"
#include "securec.h"
#include "sysparam_errno.h"
#include <taihe/runtime_ani.hpp>

#ifndef DEVICEINFO_JS_DOMAIN
#define DEVICEINFO_JS_DOMAIN (BASE_DOMAIN + 8)
#endif

#ifndef BINFO_TAG
#define BINFO_TAG "BOARDINFO_TAIHE"
#endif

#define BINFO_LOGV(fmt, ...) STARTUP_LOGV(DEVICEINFO_JS_DOMAIN, BINFO_TAG, fmt, ##__VA_ARGS__)
#define BINFO_LOGE(fmt, ...) STARTUP_LOGE(DEVICEINFO_JS_DOMAIN, BINFO_TAG, fmt, ##__VA_ARGS__)

namespace {

using AclBoardInfoFunc = const char* (*)();

static ::taihe::string CallAclBoardInfo(const char* name, AclBoardInfoFunc acl)
{
    const char* value = acl();
    if (value == nullptr) {
        value = "";
    }
    BINFO_LOGV("%s done", name);
    return value;
}

::taihe::string getcpuId()
{
    return CallAclBoardInfo("getcpuId", AclGetCpuId);
}

::taihe::string getcpuArch()
{
    return CallAclBoardInfo("getcpuArch", AclGetCpuArchitecture);
}

::taihe::string getcpuVendor()
{
    return CallAclBoardInfo("getcpuVendor", AclGetCpuVendor);
}

::taihe::string getboardSn()
{
    char buf[256] = {0};
    int ret = AclGetBoardSerial(buf, sizeof(buf));
    if (ret == SYSPARAM_PERMISSION_DENIED) {
        taihe::set_business_error(BOARD_INFO_PERMISSION_DENIED_CODE, BOARD_INFO_PERMISSION_DENIED_MSG);
    }
    return buf;
}

::taihe::string getboardVendor()
{
    return CallAclBoardInfo("getboardVendor", AclGetBoardVendor);
}

::taihe::string getboardName()
{
    return CallAclBoardInfo("getboardName", AclGetBoardProductName);
}

::taihe::string getbiosVendor()
{
    return CallAclBoardInfo("getbiosVendor", AclGetBiosVendor);
}

::taihe::string getbiosVersion()
{
    return CallAclBoardInfo("getbiosVersion", AclGetBiosVersion);
}

::taihe::string getbiosDate()
{
    return CallAclBoardInfo("getbiosDate", AclGetBiosReleaseDate);
}
}  // namespace

TH_EXPORT_CPP_API_getcpuId(getcpuId);
TH_EXPORT_CPP_API_getcpuArch(getcpuArch);
TH_EXPORT_CPP_API_getcpuVendor(getcpuVendor);
TH_EXPORT_CPP_API_getboardSn(getboardSn);
TH_EXPORT_CPP_API_getboardVendor(getboardVendor);
TH_EXPORT_CPP_API_getboardName(getboardName);
TH_EXPORT_CPP_API_getbiosVendor(getbiosVendor);
TH_EXPORT_CPP_API_getbiosVersion(getbiosVersion);
TH_EXPORT_CPP_API_getbiosDate(getbiosDate);
