/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

/*
 * Stub for OH_HiSysEvent_Write — records call count, domain, eventName,
 * and returns a configurable result so tests can assert behaviour.
 *
 * hisysevent_c.h defines OH_HiSysEvent_Write as a macro, so command-line
 * -D would trigger -Werror=redefine. Instead, init_hisysevent.c and
 * sys_event.c #undef OH_HiSysEvent_Write and #define it to
 * StubOhHiSysEventWrite under #ifdef STARTUP_INIT_TEST (after including
 * hisysevent_c.h). This stub provides the redirect target function.
 *
 * The stub uses opaque pointer types for params to avoid depending on
 * the external hisysevent_c.h header.
 */

#include <cstddef>
#include <cstdint>

#include "securec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Records visible to test code via accessor functions below. */
static int32_t g_hisyseventWriteCallCount = 0;
static char g_lastDomain[64] = {0};
static char g_lastEventName[64] = {0};
static int g_hisyseventWriteRet = 0;

void ResetHiSysEventStub(void)
{
    g_hisyseventWriteCallCount = 0;
    g_lastDomain[0] = '\0';
    g_lastEventName[0] = '\0';
    g_hisyseventWriteRet = 0;
}

int32_t GetHiSysEventWriteCallCount(void)
{
    return g_hisyseventWriteCallCount;
}

const char *GetLastHiSysEventDomain(void)
{
    return g_lastDomain;
}

const char *GetLastHiSysEventName(void)
{
    return g_lastEventName;
}

void SetHiSysEventWriteRet(int ret)
{
    g_hisyseventWriteRet = ret;
}

/* StubOhHiSysEventWrite — replaces OH_HiSysEvent_Write via #undef/#define
 * in init_hisysevent.c and sys_event.c under #ifdef STARTUP_INIT_TEST.
 * Signature matches the real OH_HiSysEvent_Write from hisysevent_c.h:
 *   int OH_HiSysEvent_Write(const char *domain, const char *eventName,
 *       HiSysEventType type, const HiSysEventParam *params, size_t paramCount);
 * Uses opaque types to avoid depending on the external header.
 */
typedef int HiSysEventTypeStub;
int StubOhHiSysEventWrite(const char *domain, const char *eventName,
    HiSysEventTypeStub type, const void *params, size_t paramCount)
{
    (void)type;
    (void)params;
    (void)paramCount;
    g_hisyseventWriteCallCount++;
    if (domain != nullptr) {
        if (strncpy_s(g_lastDomain, sizeof(g_lastDomain), domain, sizeof(g_lastDomain) - 1) != 0) {
            g_lastDomain[0] = '\0';
        }
    }
    if (eventName != nullptr) {
        if (strncpy_s(g_lastEventName, sizeof(g_lastEventName), eventName,
            sizeof(g_lastEventName) - 1) != 0) {
            g_lastEventName[0] = '\0';
        }
    }
    return g_hisyseventWriteRet;
}

#ifdef __cplusplus
}
#endif
