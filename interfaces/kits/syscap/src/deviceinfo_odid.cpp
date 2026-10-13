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
 
#include <string>
 
#include "beget_ext.h"
#include "deviceinfo.h"
#include "securec.h"
#ifdef DEPENDENT_APPEXECFWK_BASE
#include "bundlemgr/bundle_mgr_proxy.h"
#endif
#include "iservice_registry.h"
#include "if_system_ability_manager.h"
#include "system_ability_definition.h"
 
#define ODID_LEN 37
 
typedef enum {
    DEV_INFO_OK,
    DEV_INFO_ENULLPTR,
    DEV_INFO_EGETODID,
    DEV_INFO_ESTRCOPY
} DevInfoError;
 
static DevInfoError AclGetDevOdid(char *odid, int size)
{
    DevInfoError ret = DEV_INFO_OK;
    if (odid[0] != '\0') {
        return DEV_INFO_OK;
    }
    auto systemAbilityManager = OHOS::SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (!systemAbilityManager) {
        return DEV_INFO_ENULLPTR;
    }
 
    auto remoteObject = systemAbilityManager->GetSystemAbility(OHOS::BUNDLE_MGR_SERVICE_SYS_ABILITY_ID);
    if (!remoteObject) {
        return DEV_INFO_ENULLPTR;
    }
 
#ifdef DEPENDENT_APPEXECFWK_BASE
    auto bundleMgr = OHOS::iface_cast<OHOS::AppExecFwk::IBundleMgr>(remoteObject);
    if (!bundleMgr) {
        return DEV_INFO_ENULLPTR;
    }
 
    std::string odidStr;
    if (bundleMgr->GetOdid(odidStr) != 0) {
        return DEV_INFO_EGETODID;
    }
 
    if (strcpy_s(odid, size, odidStr.c_str()) != EOK) {
        return DEV_INFO_ESTRCOPY;
    }
#else
    BEGET_LOGE("DEPENDENT_APPEXECFWK_BASE does not exist, The ODID could not be obtained");
    ret = DEV_INFO_EGETODID;
#endif
 
    return ret;
}
 
const char *OH_GetOdid(void)
{
    static char odid[ODID_LEN] = {0};
    DevInfoError ret = AclGetDevOdid(odid, ODID_LEN);
    if (ret != DEV_INFO_OK) {
        BEGET_LOGE("GetODID ret:%d", ret);
    }
    return odid;
}