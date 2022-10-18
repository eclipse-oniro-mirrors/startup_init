/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <cstdlib>
#include <cstdint>

#include "begetctl.h"
#include "shell.h"
#include "shell_utils.h"
#include "parameter.h"
#include "parameters.h"
#include "sysversion.h"

using SysParaInfoItem = struct {
    char *infoName;
    const char *(*getInfoValue)(void);
};

static const SysParaInfoItem SYSPARA_LIST[] = {
    {(char *)"DeviceType", GetDeviceType},
    {(char *)"Manufacture", GetManufacture},
    {(char *)"Brand", GetBrand},
    {(char *)"MarketName", GetMarketName},
    {(char *)"ProductSeries", GetProductSeries},
    {(char *)"ProductModel", GetProductModel},
    {(char *)"SoftwareModel", GetSoftwareModel},
    {(char *)"HardwareModel", GetHardwareModel},
    {(char *)"Serial", GetSerial},
    {(char *)"OSFullName", GetOSFullName},
    {(char *)"DisplayVersion", GetDisplayVersion},
    {(char *)"BootloaderVersion", GetBootloaderVersion},
    {(char *)"GetSecurityPatchTag", GetSecurityPatchTag},
    {(char *)"AbiList", GetAbiList},
    {(char *)"IncrementalVersion", GetIncrementalVersion},
    {(char *)"VersionId", GetVersionId},
    {(char *)"BuildType", GetBuildType},
    {(char *)"BuildUser", GetBuildUser},
    {(char *)"BuildHost", GetBuildHost},
    {(char *)"BuildTime", GetBuildTime},
    {(char *)"BuildRootHash", GetBuildRootHash},
    {(char *)"GetOsReleaseType", GetOsReleaseType},
    {(char *)"GetHardwareProfile", GetHardwareProfile},
};

static int32_t SysParaApiDumpCmd(BShellHandle shell, int32_t argc, char *argv[])
{
    int index = 0;
    int dumpInfoItemNum = (sizeof(SYSPARA_LIST) / sizeof(SYSPARA_LIST[0]));
    const char *temp = nullptr;
    BShellEnvOutput(shell, const_cast<char *>("Begin dump syspara\r\n"));
    BShellEnvOutput(shell, const_cast<char *>("=======================\r\n"));
    while (index < dumpInfoItemNum) {
        temp = SYSPARA_LIST[index].getInfoValue();
        BShellEnvOutput(shell, const_cast<char *>("%s:%s\r\n"), SYSPARA_LIST[index].infoName, temp);
        index++;
    }
    BShellEnvOutput(shell, const_cast<char *>("FirstApiVersion:%d\r\n"), GetFirstApiVersion());
    BShellEnvOutput(shell, const_cast<char *>("GetSerial:%s\r\n"), GetSerial());
#ifndef OHOS_LITE
    BShellEnvOutput(shell, const_cast<char *>("acl serial:%s\r\n"), AclGetSerial());
#endif
    char udid[65] = {0};
    GetDevUdid(udid, sizeof(udid));
    BShellEnvOutput(shell, const_cast<char *>("GetDevUdid:%s\r\n"), udid);
#ifndef OHOS_LITE
    AclGetDevUdid(udid, sizeof(udid));
    BShellEnvOutput(shell, const_cast<char *>("Acl devUdid:%s\r\n"), udid);
#endif
    BShellEnvOutput(shell, const_cast<char *>("Version:%d.%d.%d.%d\r\n"),
        GetMajorVersion(), GetSeniorVersion(), GetFeatureVersion(), GetBuildVersion());
    BShellEnvOutput(shell, const_cast<char *>("GetSdkApiVersion:%d\r\n"), GetSdkApiVersion());
    BShellEnvOutput(shell, const_cast<char *>("GetSystemCommitId:%lld\r\n"), GetSystemCommitId());
    BShellEnvOutput(shell, const_cast<char *>("=======================\r\n"));
    BShellEnvOutput(shell, const_cast<char *>("End dump syspara\r\n"));
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    const CmdInfo infos[] = {
        {
            const_cast<char *>("dump"), SysParaApiDumpCmd, const_cast<char *>("dump api"),
            const_cast<char *>("dump api"), const_cast<char *>("dump api")
        },
    };
    for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
        BShellEnvRegisterCmd(GetShellHandle(), &infos[i]);
    }
}
