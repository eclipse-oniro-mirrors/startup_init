/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include "bootstage.h"

#define INIT_BOOTSTAGE_HOOK_NAME "bootstage"
static HOOK_MGR *bootStageHookMgr = NULL;

HOOK_MGR *GetBootStageHookMgr()
{
    if (bootStageHookMgr != NULL) {
        return bootStageHookMgr;
    }

    /*
     * Create bootstage hook manager for booting only.
     * When boot completed, this manager will be destroyed.
     */
    bootStageHookMgr = HookMgrCreate(INIT_BOOTSTAGE_HOOK_NAME);
    return bootStageHookMgr;
}
