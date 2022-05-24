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

#include "parameter.h"

#include <stdint.h>
#include <stdlib.h>

#include "param_comm.h"
#include "init_param.h"
#include "sysparam_errno.h"
#include "securec.h"
#include "sysversion.h"

int WaitParameter(const char *key, const char *value, int timeout)
{
    if ((key == NULL) || (value == NULL)) {
        return EC_INVALID;
    }
    return SystemWaitParameter(key, value, timeout);
}

uint32_t FindParameter(const char *key)
{
    if (key == NULL) {
        return (uint32_t)(-1);
    }
    uint32_t handle = 0;
    int ret = SystemFindParameter(key, &handle);
    if (ret != 0) {
        return (uint32_t)(-1);
    }
    return handle;
}

uint32_t GetParameterCommitId(uint32_t handle)
{
    uint32_t commitId = 0;
    int ret = SystemGetParameterCommitId(handle, &commitId);
    if (ret != 0) {
        return (uint32_t)(-1);
    }
    return commitId;
}

int GetParameterName(uint32_t handle, char *name, uint32_t len)
{
    if (name == NULL) {
        return EC_INVALID;
    }
    int ret = SystemGetParameterName(handle, name, len);
    return (ret != 0) ? EC_FAILURE : strlen(name);
}

int GetParameterValue(uint32_t handle, char *value, uint32_t len)
{
    if (value == NULL) {
        return EC_INVALID;
    }
    uint32_t size = len;
    int ret = SystemGetParameterValue(handle, value, &size);
    return (ret != 0) ? EC_FAILURE : strlen(value);
}

int GetParameter(const char *key, const char *def, char *value, uint32_t len)
{
    if ((key == NULL) || (value == NULL)) {
        return EC_INVALID;
    }
    int ret = HalGetParameter(key, def, value, len);
    return (ret != 0) ? EC_INVALID : strlen(value);
}

int GetIntParameter(const char *key, int def)
{
    int out = 0;
    char value[32] = {0}; // 32 max for int
    int ret = GetParameter(key, "0", value, sizeof(value));
    if (ret != 0) {
        return out;
    }
    long long int result = 0;
    if (StringToLL(value, &result) != 0) {
        return def;
    }
    return (int32_t)result;
}

int SetParameter(const char *key, const char *value)
{
    if ((key == NULL) || (value == NULL)) {
        return EC_INVALID;
    }
    int ret = SystemSetParameter(key, value);
    return (ret == 0) ? EC_SUCCESS : EC_FAILURE;
}

const char *GetDeviceType(void)
{
    static const char *productType = NULL;
    return GetProperty("const.build.characteristics", &productType);
}

const char *GetProductModel(void)
{
    return GetProductModel_();
}

const char *GetManufacture(void)
{
    return GetManufacture_();
}

const char *GetBrand(void)
{
    static const char *productBrand = NULL;
    return GetProperty("const.product.brand", &productBrand);
}

const char *GetMarketName(void)
{
    static const char *marketName = NULL;
    return GetProperty("const.product.name", &marketName);
}

const char *GetProductSeries(void)
{
    static const char *productSeries = NULL;
    return GetProperty("const.build.product", &productSeries);
}

const char *GetSoftwareModel(void)
{
    static const char *softwareModel = NULL;
    return GetProperty("const.software.model", &softwareModel);
}

const char *GetHardwareModel(void)
{
    static const char *hardwareModel = NULL;
    return GetProperty("const.product.hardwareversion", &hardwareModel);
}

const char *GetHardwareProfile(void)
{
    static const char *hardwareProfile = NULL;
    return GetProperty("const.product.hardwareprofile", &hardwareProfile);
}

const char *GetAbiList(void)
{
    static const char *productAbiList = NULL;
    return GetProperty("const.product.cpu.abilist", &productAbiList);
}

const char *GetBootloaderVersion(void)
{
    static const char *productBootloader = NULL;
    return GetProperty("const.product.bootloader.version", &productBootloader);
}

int GetFirstApiVersion(void)
{
    static const char *firstApiVersion = NULL;
    GetProperty("const.product.firstapiversion", &firstApiVersion);
    if (firstApiVersion == NULL) {
        return 0;
    }
    return atoi(firstApiVersion);
}

const char *GetDisplayVersion(void)
{
    static const char *displayVersion = NULL;
    return GetProperty("const.product.software.version", &displayVersion);
}

const char *GetIncrementalVersion(void)
{
    static const char *incrementalVersion = NULL;
    return GetProperty("const.product.incremental.version", &incrementalVersion);
}

const char *HalGetOsReleaseType(void)
{
    static const char *osReleaseType = NULL;
    return GetProperty("const.ohos.releasetype", &osReleaseType);
}

const char *HalGetSdkApiVersion(void)
{
    static const char *sdkApiVersion = NULL;
    return GetProperty("const.ohos.apiversion", &sdkApiVersion);
}

const char *GetBuildType(void)
{
    static const char *buildType = NULL;
    return GetProperty("const.product.build.type", &buildType);
}

const char *GetBuildUser(void)
{
    static const char *buildUser = NULL;
    return GetProperty("const.product.build.user", &buildUser);
}

const char *GetBuildHost(void)
{
    static const char *buildHost = NULL;
    return GetProperty("const.product.build.host", &buildHost);
}

const char *GetBuildTime(void)
{
    static const char *buildTime = NULL;
    return GetProperty("const.product.build.date", &buildTime);
}

const char *GetSerial(void)
{
    static char ohos_serial[PARAM_VALUE_LEN_MAX]  = {0};
    uint32_t len = PARAM_VALUE_LEN_MAX;
    int ret = SystemGetParameter("ohos.boot.sn", ohos_serial, &len);
    if (ret != 0) {
        return NULL;
    }
    return ohos_serial;
}

int GetDevUdid(char *udid, int size)
{
    return GetDevUdid_(udid, size);
}

static const char *GetOSName(void)
{
    static const char *osName = NULL;
    return GetProperty("const.ohos.name", &osName);
}

static const char *BuildOSFullName(void)
{
    const char release[] = "Release";
    char value[OS_FULL_NAME_LEN];
    const char *releaseType = GetOsReleaseType();
    int length;
    if ((releaseType == NULL) || (strncmp(releaseType, release, sizeof(release) - 1) == 0)) {
        length = sprintf_s(value, OS_FULL_NAME_LEN, "%s-%d.%d.%d.%d",
            GetOSName(), GetMajorVersion(), GetSeniorVersion(), GetFeatureVersion(), GetBuildVersion());
    } else {
        length = sprintf_s(value, OS_FULL_NAME_LEN, "%s-%d.%d.%d.%d(%s)",
            GetOSName(), GetMajorVersion(), GetSeniorVersion(), GetFeatureVersion(), GetBuildVersion(), releaseType);
    }
    if (length < 0) {
        return EMPTY_STR;
    }
    const char *osFullName = strdup(value);
    return osFullName;
}

const char *GetOSFullName(void)
{
    static const char *osFullName = NULL;
    if (osFullName != NULL) {
        return osFullName;
    }
    osFullName = BuildOSFullName();
    if (osFullName == NULL) {
        return EMPTY_STR;
    }
    return osFullName;
}

static int GetSdkApiLevel(void)
{
    static const char *sdkApiLevel = NULL;
    GetProperty("const.ohos.sdkapilevel", &sdkApiLevel);
    if (sdkApiLevel == NULL) {
        return 0;
    }
    return atoi(sdkApiLevel);
}

static const char *BuildVersionId(void)
{
    char value[VERSION_ID_MAX_LEN];

    int len = sprintf_s(value, VERSION_ID_MAX_LEN, "%s/%s/%s/%s/%s/%s/%s/%d/%s/%s",
        GetDeviceType(), GetManufacture(), GetBrand(), GetProductSeries(),
        GetOSFullName(), GetProductModel(), GetSoftwareModel(),
        GetSdkApiLevel(), GetIncrementalVersion(), GetBuildType());
    if (len <= 0) {
        return EMPTY_STR;
    }
    const char *versionId = strdup(value);
    return versionId;
}

const char *GetVersionId(void)
{
    static const char *ohosVersionId = NULL;
    if (ohosVersionId != NULL) {
        return ohosVersionId;
    }
    ohosVersionId = BuildVersionId();
    if (ohosVersionId == NULL) {
        return EMPTY_STR;
    }
    return ohosVersionId;
}

const char *GetOsReleaseType(void)
{
    static const char *osReleaseType = NULL;
    return GetProperty("const.ohos.releasetype", &osReleaseType);
}

int GetSdkApiVersion(void)
{
    static const char *sdkApiVersion = NULL;
    GetProperty("const.ohos.apiversion", &sdkApiVersion);
    if (sdkApiVersion == NULL) {
        return 0;
    }
    return atoi(sdkApiVersion);
}

const char *GetSecurityPatchTag(void)
{
    static const char *securityPatchTag = NULL;
    return GetProperty("const.ohos.version.security_patch", &securityPatchTag);
}

const char *GetBuildRootHash(void)
{
    static const char *buildRootHash = NULL;
    return GetProperty("const.ohos.buildroothash", &buildRootHash);
}