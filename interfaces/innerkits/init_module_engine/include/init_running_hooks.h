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

#ifndef BASE_STARTUP_BOOT_RUNNING_HOOKS_H
#define BASE_STARTUP_BOOT_RUNNING_HOOKS_H

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief Running hook stage definition for init
 */
/* Hook stage for setting parameters */
#define INIT_PARAM_SET_HOOK_STAGE 0

/**
 * @brief parameter setting context information
 */
typedef struct tagPARAM_SET_CTX {
    const char *name;   /* Parameter name */
    const char *value;  /* Parameter value */

    /* Skip setting parameter if true
     *   When setting parameters, parameter service will save the the value by default
     *   If set skipParamSet in the hook, parameter service will not save
     */
    int skipParamSet;
} PARAM_SET_CTX;

/**
 * @brief set parameter hook function prototype
 *
 * @param paramSetCtx parameter setting context information
 * @return None
 */
typedef void (*ParamSetHook)(PARAM_SET_CTX *paramSetCtx);

/**
 * @brief Register a hook for setting parameters
 *
 * @param hook parameter setting hook
 *   in the hook, we can match the parameters with special patterns to do extra controls.
 *   For example, ohos.ctl.start will control services besides the normal parameter saving.
 * @return return 0 if succeed; other values if failed.
 */
int ParamSetHookAdd(ParamSetHook hook);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif
