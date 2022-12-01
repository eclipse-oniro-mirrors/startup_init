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
#include <errno.h>
#include <unistd.h>

#include "init_log.h"
#include "init_param.h"
#include "init_utils.h"
#include "param_manager.h"
#ifdef PARAM_LOAD_CFG_FROM_CODE
#include "param_cfg.h"
#endif
#ifdef __LITEOS_M__
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "parameter.h"
#endif

#ifdef PARAM_LOAD_CFG_FROM_CODE
static const char *StringTrim(char *buffer, int size, const char *name)
{
    char *tmp = (char *)name;
    while (*tmp != '\0' && *tmp != '"') {
        tmp++;
    }
    if (*tmp == '\0') {
        return name;
    }
    // skip "
    tmp++;
    int i = 0;
    while (*tmp != '\0' && i < size) {
        buffer[i++] = *tmp;
        tmp++;
    }
    if (i >= size) {
        return name;
    }
    while (i > 0) {
        if (buffer[i] == '"') {
            buffer[i] = '\0';
            return buffer;
        }
        i--;
    }
    return name;
}
#endif

void InitParamService(void)
{
    PARAM_LOGI("InitParamService %s", DATA_PATH);
    CheckAndCreateDir(PARAM_STORAGE_PATH "/");
    CheckAndCreateDir(DATA_PATH);
    // param space
    int ret = InitParamWorkSpace(0, NULL);
    PARAM_CHECK(ret == 0, return, "Init parameter workspace fail");
    ret = InitPersistParamWorkSpace();
    PARAM_CHECK(ret == 0, return, "Init persist parameter workspace fail");

    // from build
    LoadParamFromBuild();
#ifdef PARAM_LOAD_CFG_FROM_CODE
    char *buffer = calloc(1, PARAM_VALUE_LEN_MAX);
    PARAM_CHECK(buffer != NULL, return, "Failed to malloc for buffer");
    for (size_t i = 0; i < ARRAY_LENGTH(g_paramDefCfgNodes); i++) {
        PARAM_LOGV("InitParamService name %s = %s", g_paramDefCfgNodes[i].name, g_paramDefCfgNodes[i].value);
        uint32_t dataIndex = 0;
        ret = WriteParam(g_paramDefCfgNodes[i].name,
            StringTrim(buffer, PARAM_VALUE_LEN_MAX, g_paramDefCfgNodes[i].value), &dataIndex, 0);
        PARAM_CHECK(ret == 0, continue, "Failed to set param %d name %s %s",
            ret, g_paramDefCfgNodes[i].name, g_paramDefCfgNodes[i].value);
    }
    free(buffer);
#endif
}

int StartParamService(void)
{
    return 0;
}

void StopParamService(void)
{
    PARAM_LOGI("StopParamService.");
    ClosePersistParamWorkSpace();
    CloseParamWorkSpace();
}

int SystemWriteParam(const char *name, const char *value)
{
    int ctrlService = 0;
    int ret = CheckParameterSet(name, value, GetParamSecurityLabel(), &ctrlService);
    PARAM_CHECK(ret == 0, return ret, "Forbid to set parameter %s", name);
    PARAM_LOGV("SystemWriteParam name %s value: %s ctrlService %d", name, value, ctrlService);
    if ((ctrlService & PARAM_CTRL_SERVICE) != PARAM_CTRL_SERVICE) { // ctrl param
        uint32_t dataIndex = 0;
        ret = WriteParam(name, value, &dataIndex, 0);
        PARAM_CHECK(ret == 0, return ret, "Failed to set param %d name %s %s", ret, name, value);
        ret = WritePersistParam(name, value);
        PARAM_CHECK(ret == 0, return ret, "Failed to set persist param name %s", name);
    } else {
        PARAM_LOGE("SystemWriteParam can not support service ctrl parameter name %s", name);
    }
    return ret;
}

#ifdef __LITEOS_M__
#define OS_DELAY 1000 // * 30 // 30s
#define STACK_SIZE 1024


typedef struct SysParaInfoItem_ {
    char *infoName;
    const char *(*getInfoValue)(void);
}SysParaInfoItem;

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

int32_t SysParaApiDumpCmd()
{
    int index = 0;
    int dumpInfoItemNum = (sizeof(SYSPARA_LIST) / sizeof(SYSPARA_LIST[0]));
    const char *temp = NULL;
    printf("Begin dump syspara\r\n");
    printf("=======================\r\n");
    while (index < dumpInfoItemNum) {
        temp = SYSPARA_LIST[index].getInfoValue();
        printf("%s:%s\r\n", SYSPARA_LIST[index].infoName, temp);
        index++;
    }
    printf("FirstApiVersion:%d\r\n", GetFirstApiVersion());
    printf("GetSerial:%s\r\n", GetSerial());
    char udid[65] = {0};
    GetDevUdid(udid, sizeof(udid));
    printf("GetDevUdid:%s\r\n", udid);
    printf("Version:%d.%d.%d.%d\r\n",
        GetMajorVersion(), GetSeniorVersion(), GetFeatureVersion(), GetBuildVersion());
    printf("GetSdkApiVersion:%d\r\n", GetSdkApiVersion());
    printf("GetSystemCommitId:%lld\r\n", GetSystemCommitId());
    printf("=======================\r\n");
    printf("End dump syspara\r\n");
    return 0;
}

static void ParamServiceTask(int *arg)
{
    (void)arg;
    PARAM_LOGI("ParamServiceTask start");
    SysParaApiDumpCmd();
    while (1) {
        CheckAndSavePersistParam();
        PARAM_LOGI("CheckAndSavePersistParam");
        printf("CheckAndSavePersistParam \n");
        osDelay(OS_DELAY);
    }
}

void LiteParamService(void)
{
    static init = 0;
    if (init) {
        printf("LiteParamService has been init \n");
        return;
    }
    init = 1;
    EnableInitLog(INIT_INFO);
    printf("LiteParamService \n");
    InitParamService();
    // get persist param
    LoadPersistParams();
    osThreadAttr_t attr;
    attr.name = "ParamServiceTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0;
    attr.priority = osPriorityBelowNormal;

    if (osThreadNew((osThreadFunc_t)ParamServiceTask, NULL, &attr) == NULL) {
        PARAM_LOGE("Failed to create ParamServiceTask! %d", errno);
    }
}
CORE_INIT(LiteParamService);
#endif