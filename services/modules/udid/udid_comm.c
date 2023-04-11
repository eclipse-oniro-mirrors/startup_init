/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include "udid.h"

#ifdef OHOS_LITE
#include "hal_sys_param.h"
#endif
#include "init_param.h"
#include "param_comm.h"
#include "securec.h"
#include "sysparam_errno.h"

INIT_LOCAL_API const char *GetSerial_(void)
{
#ifdef OHOS_LITE
    return HalGetSerial();
#else
    static char *ohosSerial = NULL;
    if (ohosSerial == NULL) {
        BEGET_CHECK((ohosSerial = (char *)calloc(1, PARAM_VALUE_LEN_MAX)) != NULL, return NULL);
    }
    uint32_t len = PARAM_VALUE_LEN_MAX;
    int ret = SystemGetParameter("ohos.boot.sn", ohosSerial, &len);
    BEGET_CHECK(ret == 0, return NULL);
    return ohosSerial;
#endif
}

INIT_LOCAL_API int GetUdidFromParam(char *udid, uint32_t size)
{
    uint32_t len = size;
    int ret = SystemGetParameter("const.product.udid", udid, &len);
    BEGET_CHECK(ret != 0, return ret);

    len = size;
    ret = SystemGetParameter("const.product.devUdid", udid, &len);
    BEGET_CHECK(ret != 0, return ret);
    return ret;
}

INIT_LOCAL_API int GetDevUdid_(char *udid, int size)
{
    if (size < UDID_LEN || udid == NULL) {
        return EC_FAILURE;
    }

    int ret = GetUdidFromParam(udid, (uint32_t)size);
    BEGET_CHECK(ret != 0, return ret);

#ifdef OHOS_LITE
    ret = CalcDevUdid(udid, size);
#endif
    return ret;
}