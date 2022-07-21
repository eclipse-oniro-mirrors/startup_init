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

#ifndef OHOS_MODULES_MANAGER_H__
#define OHOS_MODULES_MANAGER_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief Module constructor function
 *
 * For static modules, this code is executed before main().
 * For dynamic modules, this code is executed when ModuleMgrInstall().
 *
 * Usage example:
 *   MODULE_CONSTRUCTOR(void)
 *   {
 *       ...
 *   }
 */
#define MODULE_CONSTRUCTOR(void) static void _init(void) __attribute__((constructor)); static void _init(void)

/**
 * @brief Module destructor function
 *
 * For static modules, this code will not be executed.
 * For dynamic modules, this code is executed when ModuleMgrUninstall().
 *
 * Usage example:
 *   MODULE_DESTRUCTOR(void)
 *   {
 *       ...
 *   }
 */
#define MODULE_DESTRUCTOR(void) static void _destroy(void) __attribute__((destructor)); static void _destroy(void)

// Forward declaration
typedef struct tagMODULE_MGR MODULE_MGR;

/**
 * @brief Create dynamic module manager
 *
 * This dynamic module manager will manager modules
 * in the directory: /system/lib/{name}/
 * @param name module manager name
 * @return return module manager handle if succeed; return NULL if failed.
 */
MODULE_MGR *ModuleMgrCreate(const char *name);

/**
 * @brief Destroy dynamic module manager
 *
 * It will uninstall all modules managed by this moduleMgr
 * @param moduleMgr module manager handle
 * @return None
 */
void ModuleMgrDestroy(MODULE_MGR *moduleMgr);

/**
 * @brief Install a module
 * 
 * The final module path is: /system/lib/{moduleMgrPath}/{moduleName}.z.so
 *
 * @param moduleMgr module manager handle
 * @param moduleName module name
 * @param argc argument counts for installing
 * @param argv arguments for installing, the last argument is NULL.
 * @return module handle returned by dlopen
 */
int ModuleMgrInstall(MODULE_MGR *moduleMgr, const char *moduleName,
                     int argc, const char *argv[]);

/**
 * @brief Module install arguments
 */
typedef struct {
    int argc;
    const char **argv;
} MODULE_INSTALL_ARGS;

/**
 * @brief Get module install arguments
 * 
 * This function is available only in MODULE_CONSTRUCTOR.
 *
 * @return install args if succeed; return NULL if failed.
 */
const MODULE_INSTALL_ARGS *ModuleMgrGetArgs(void);

/**
 * @brief Scan and install all modules in specified directory
 *
 * @param modulePath path for modules to be installed
 * @return install args if succeed; return NULL if failed.
 */
MODULE_MGR *ModuleMgrScan(const char *modulePath);

/**
 * @brief Uninstall module
 *
 * @param moduleMgr module manager handle
 * @param name module name. If name is NULL, it will uninstall all modules.
 * @return install args if succeed; return NULL if failed.
 */
void ModuleMgrUninstall(MODULE_MGR *moduleMgr, const char *name);

/**
 * @brief Get number of modules in module manager
 *
 * @param hookMgr module manager handle
 * @return number of modules, return 0 if none
 */
int ModuleMgrGetCnt(const MODULE_MGR *moduleMgr);

/**
 * @brief Module information for traversing modules
 */
typedef struct tagMODULE_INFO {
    const char *name;     /* module name */
    void *handle;         /* module handler */
    void *cookie;         /* hook execution cookie */
} MODULE_INFO;

/**
 * @brief Module traversal function prototype
 *
 * @param moduleInfo MODULE_INFO for traversing each module.
 * @return None
 */
typedef void (*OhosModuleTraversal)(const MODULE_INFO *moduleInfo);

/**
 * @brief Traversing all modules in the ModuleManager
 *
 * @param moduleMgr module manager handle
 * @param cookie traversal cookie.
 * @param traversal traversal function.
 * @return None.
 */
void ModuleMgrTraversal(const MODULE_MGR *moduleMgr, void *cookie, OhosModuleTraversal traversal);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif
