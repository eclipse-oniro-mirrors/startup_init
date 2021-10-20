/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined OHOS_LITE && !defined __LINUX__
#include <sys/capability.h>
#else
#include <linux/capability.h>
#endif
#include <sys/types.h>
#include <unistd.h>

#include "init_log.h"
#include "init_service.h"
#include "init_service_manager.h"
#include "init_utils.h"

#define MAX_CAPS_CNT_FOR_ONE_SERVICE 100

static unsigned int GetCapByString(const char *capStr)
{
    static const CapStrCapNum capStrCapNum[] = {
        { "CHOWN", CAP_CHOWN },
        { "DAC_OVERRIDE", CAP_DAC_OVERRIDE },
        { "DAC_READ_SEARCH", CAP_DAC_READ_SEARCH },
        { "FOWNER", CAP_FOWNER },
        { "FSETID", CAP_FSETID },
        { "KILL", CAP_KILL },
        { "SETGID", CAP_SETGID },
        { "SETUID", CAP_SETUID },
        { "SETPCAP", CAP_SETPCAP },
        { "LINUX_IMMUTABLE", CAP_LINUX_IMMUTABLE },
        { "NET_BIND_SERVICE", CAP_NET_BIND_SERVICE },
        { "NET_BROADCAST", CAP_NET_BROADCAST },
        { "NET_ADMIN", CAP_NET_ADMIN },
        { "NET_RAW", CAP_NET_RAW },
        { "IPC_LOCK", CAP_IPC_LOCK },
        { "IPC_OWNER", CAP_IPC_OWNER },
        { "SYS_MODULE", CAP_SYS_MODULE },
        { "SYS_RAWIO", CAP_SYS_RAWIO },
        { "SYS_CHROOT", CAP_SYS_CHROOT },
        { "SYS_PTRACE", CAP_SYS_PTRACE },
        { "SYS_PACCT", CAP_SYS_PACCT },
        { "SYS_ADMIN", CAP_SYS_ADMIN },
        { "SYS_BOOT", CAP_SYS_BOOT },
        { "SYS_NICE", CAP_SYS_NICE },
        { "SYS_RESOURCE", CAP_SYS_RESOURCE },
        { "SYS_TIME", CAP_SYS_TIME },
        { "SYS_TTY_CONFIG", CAP_SYS_TTY_CONFIG },
        { "MKNOD", CAP_MKNOD },
        { "LEASE", CAP_LEASE },
        { "AUDIT_WRITE", CAP_AUDIT_WRITE },
        { "AUDIT_CONTROL", CAP_AUDIT_CONTROL },
        { "SETFCAP", CAP_SETFCAP },
        { "MAC_OVERRIDE", CAP_MAC_OVERRIDE },
        { "MAC_ADMIN", CAP_MAC_ADMIN },
        { "SYSLOG", CAP_SYSLOG },
        { "WAKE_ALARM", CAP_WAKE_ALARM },
        { "BLOCK_SUSPEND", CAP_BLOCK_SUSPEND },
        { "AUDIT_READ", CAP_AUDIT_READ },
    };
    int mapSize = (int)ARRAY_LENGTH(capStrCapNum);
    for (int j = 0; j < mapSize; j++) {
        if (strcmp(capStr, capStrCapNum[j].capStr) == 0) {
            return capStrCapNum[j].CapNum;
        }
    }
    return -1;
}

int GetServiceCaps(const cJSON *curArrItem, Service *service)
{
    INIT_ERROR_CHECK(service != NULL, return SERVICE_FAILURE, "service is null ptr.");
    INIT_ERROR_CHECK(curArrItem != NULL, return SERVICE_FAILURE, "json is null ptr.");
    service->servPerm.capsCnt = 0;
    service->servPerm.caps = NULL;
    int capsCnt = 0;
    cJSON *filedJ = GetArrayItem(curArrItem, &capsCnt, "caps");
    if (filedJ == NULL) {
        return SERVICE_SUCCESS;
    }
    if (capsCnt > MAX_CAPS_CNT_FOR_ONE_SERVICE) {
        INIT_LOGE("service=%s, too many caps[cnt %d] for one service", service->name, capsCnt);
        return SERVICE_FAILURE;
    }
    service->servPerm.caps = (unsigned int *)calloc(1, sizeof(unsigned int) * capsCnt);
    if (service->servPerm.caps == NULL) {
        INIT_LOGE("Failed to malloc for service %s", service->name);
        return SERVICE_FAILURE;
    }
    service->servPerm.capsCnt = capsCnt;
    unsigned int caps = FULL_CAP;
    for (int i = 0; i < capsCnt; ++i) { // number form
        cJSON *capJson = cJSON_GetArrayItem(filedJ, i);
        if (cJSON_IsNumber(capJson)) { // for number
            caps = (unsigned int)cJSON_GetNumberValue(capJson);
        } else if (cJSON_IsString(capJson)) {
            char *capStr = cJSON_GetStringValue(capJson);
            if (capStr == NULL || strlen(capStr) <= 0) { // check all errors
                INIT_LOGE("service=%s, parse item[%d] as string, error.", service->name, i);
                break;
            }
            caps = GetCapByString(capStr);
        }
        if (caps < 0) {
            return SERVICE_FAILURE;
        }
        if ((caps > CAP_LAST_CAP) && (caps != (unsigned int)FULL_CAP)) {
            INIT_LOGE("service=%s, caps = %d, error.", service->name, caps);
            return SERVICE_FAILURE;
        }
        service->servPerm.caps[i] = (unsigned int)caps;
    }
    return 0;
}
