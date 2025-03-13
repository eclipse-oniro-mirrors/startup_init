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

#include <ani.h>
#include <array>
#include <iostream>

#include "beget_ext.h"
#include "parameter.h"
#include "bundlemgr/bundle_mgr_proxy.h"
#include "iservice_registry.h"
#include "if_system_ability_manager.h"
#include "system_ability_definition.h"
#include "init_error.h"
#include "securec.h"

#ifndef DEVICEINFO_JS_DOMAIN
#define DEVICEINFO_JS_DOMAIN (BASE_DOMAIN + 8)
#endif

#ifndef DINFO_TAG
#define DINFO_TAG "DEVICEINFO_JS"
#endif

#define DEVINFO_LOGV(fmt, ...) STARTUP_LOGV(DEVICEINFO_JS_DOMAIN, DINFO_TAG, fmt, ##__VA_ARGS__)
#define DEVINFO_LOGI(fmt, ...) STARTUP_LOGI(DEVICEINFO_JS_DOMAIN, DINFO_TAG, fmt, ##__VA_ARGS__)
#define DEVINFO_LOGW(fmt, ...) STARTUP_LOGW(DEVICEINFO_JS_DOMAIN, DINFO_TAG, fmt, ##__VA_ARGS__)
#define DEVINFO_LOGE(fmt, ...) STARTUP_LOGE(DEVICEINFO_JS_DOMAIN, DINFO_TAG, fmt, ##__VA_ARGS__)

constexpr int ODID_LEN = 37;

typedef enum {
    DEV_INFO_OK,
    DEV_INFO_ENULLPTR,
    DEV_INFO_EGETODID,
    DEV_INFO_ESTRCOPY
} DevInfoError;

static ani_string getBrand([[maybe_unused]] ani_env *env, [[maybe_unused]] ani_object object)
{
    ani_string brand = nullptr;
    const char *value = GetBrand();
    if (value == nullptr) {
        value = "";
    }
    env->String_NewUTF8(value, strlen(value), &brand);
    return brand;
}

static ani_string getDeviceType([[maybe_unused]] ani_env *env, [[maybe_unused]] ani_object object)
{
    ani_string devicetype = nullptr;
    const char *value = GetDeviceType();
    if (value == nullptr) {
        value = "";
    }
    env->String_NewUTF8(value, strlen(value), &devicetype);
    return devicetype;
}

static ani_string getProductSeries([[maybe_unused]] ani_env *env, [[maybe_unused]] ani_object object)
{
    ani_string productSeries = nullptr;
    const char *value = GetProductSeries();
    if (value == nullptr) {
        value = "";
    }
    env->String_NewUTF8(value, strlen(value), &productSeries);
    return productSeries;
}

static ani_string getProductModel([[maybe_unused]] ani_env *env, [[maybe_unused]] ani_object object)
{
    ani_string productModel = nullptr;
    const char *value = GetProductModel();
    if (value == nullptr) {
        value = "";
    }
    env->String_NewUTF8(value, strlen(value), &productModel);
    return productModel;
}

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
    auto bundleMgrProxy = OHOS::iface_cast<OHOS::AppExecFwk::BundleMgrProxy>(remoteObject);
    if (!bundleMgrProxy) {
        return DEV_INFO_ENULLPTR;
    }

    std::string odidStr;
    if (bundleMgrProxy->GetOdid(odidStr) != 0) {
        return DEV_INFO_EGETODID;
    }

    if (strcpy_s(odid, size, odidStr.c_str()) != EOK) {
        return DEV_INFO_ESTRCOPY;
    }
#else
    DEVINFO_LOGE("DEPENDENT_APPEXECFWK_BASE does not exist, The ODID could not be obtained");
    ret = DEV_INFO_EGETODID;
#endif

    return ret;
}

static ani_string getOdid([[maybe_unused]] ani_env *env, [[maybe_unused]] ani_object object)
{
    ani_string odid = nullptr;
    static char devOdid[ODID_LEN] = {0};
    int ret = AclGetDevOdid(devOdid, ODID_LEN);
    if (ret != 0) {
        DEVINFO_LOGE("GetDevOdid ret:%d", ret);
    }
    env->String_NewUTF8(devOdid, strlen(devOdid), &odid);
    return odid;
}

static ani_int getSdkApiVersion([[maybe_unused]] ani_env *env, [[maybe_unused]] ani_object object)
{
    int sdkApiVersion = GetSdkApiVersion();
    return sdkApiVersion;
}

ANI_EXPORT ani_status ANI_Constructor(ani_vm *vm, uint32_t *result)
{
    DEVINFO_LOGI("Enter deviceinfo ANI_Constructor");
    ani_env *env;
    if (ANI_OK != vm->GetEnv(ANI_VERSION_1, &env)) {
        DEVINFO_LOGE("Unsupported ANI_VERSION_1");
        return ANI_ERROR;
    }
    ani_class ns;
    static const char *className = "L@ohos/deviceinfo/deviceInfo;";
    if (ANI_OK != env->FindClass(className, &ns)) {
        DEVINFO_LOGE("not found class");
        return ANI_ERROR;
    }

    std::array methods = {
        ani_native_function {"GetBrand", ":Lstd/core/String;", reinterpret_cast<void *>(getBrand)},
        ani_native_function {"GetDeviceType", ":Lstd/core/String;", reinterpret_cast<void *>(getDeviceType)},
        ani_native_function {"GetProductSeries", ":Lstd/core/String;", reinterpret_cast<void *>(getProductSeries)},
        ani_native_function {"GetProductModel", ":Lstd/core/String;", reinterpret_cast<void *>(getProductModel)},
        ani_native_function {"GetOdid", ":Lstd/core/String;", reinterpret_cast<void *>(getOdid)},
        ani_native_function {"GetSdkApiVersion", ":I", reinterpret_cast<void *>(getSdkApiVersion)},
    };

    if (ANI_OK != env->Class_BindNativeMethods(ns, methods.data(), methods.size())) {
        DEVINFO_LOGE("Cannot bind native methods to %s", className);
        return ANI_ERROR;
    };

    *result = ANI_VERSION_1;
    return ANI_OK;
}
