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

#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>

#include "beget_ext.h"
#include "config_policy_utils.h"
#include "init_utils.h"
#include "list.h"
#include "securec.h"
#include "modulemgr.h"

#define MODULE_SUFFIX_D ".z.so"
#ifdef SUPPORT_64BIT
#define MODULE_LIB_NAME "lib64"
#else
#define MODULE_LIB_NAME "lib"
#endif

struct tagMODULE_MGR {
    ListNode modules;
    const char *name;
    MODULE_INSTALL_ARGS installArgs;
};

MODULE_MGR *ModuleMgrCreate(const char *name)
{
    MODULE_MGR *moduleMgr;

    BEGET_CHECK(name != NULL, return NULL);

    moduleMgr = (MODULE_MGR *)malloc(sizeof(MODULE_MGR));
    BEGET_CHECK(moduleMgr != NULL, return NULL);
    OH_ListInit(&(moduleMgr->modules));
    moduleMgr->name = strdup(name);
    if (moduleMgr->name == NULL) {
        free((void *)moduleMgr);
        return NULL;
    }
    moduleMgr->installArgs.argc = 0;
    moduleMgr->installArgs.argv = NULL;

    return moduleMgr;
}

void ModuleMgrDestroy(MODULE_MGR *moduleMgr)
{
    BEGET_CHECK(moduleMgr != NULL, return);

    ModuleMgrUninstall(moduleMgr, NULL);
    BEGET_CHECK(moduleMgr->name == NULL, free((void *)moduleMgr->name));
    free((void *)moduleMgr);
}

/*
 * Module Item related api
 */


typedef struct tagMODULE_ITEM {
    ListNode node;
    MODULE_MGR *moduleMgr;
    const char *name;
    void *handle;
} MODULE_ITEM;

static void moduleDestroy(ListNode *node)
{
    MODULE_ITEM *module;

    BEGET_CHECK(node != NULL, return);

    module = (MODULE_ITEM *)node;
    BEGET_CHECK(module->name == NULL, free((void *)module->name));
    BEGET_CHECK(module->handle == NULL, dlclose(module->handle));
    free((void *)module);
}

static MODULE_INSTALL_ARGS *currentInstallArgs = NULL;

static void *moduleInstall(MODULE_ITEM *module, int argc, const char *argv[])
{
    void *handle;
    char path[PATH_MAX];

    module->moduleMgr->installArgs.argc = argc;
    module->moduleMgr->installArgs.argv = argv;

    if (module->moduleMgr->name[0] == '/') {
        if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "%s/%s" MODULE_SUFFIX_D,
            module->moduleMgr->name, module->name) < 0) {
            return NULL;
        }
    } else {
        const char *fmt = (InUpdaterMode() == 0) ? "/system/" MODULE_LIB_NAME : "/" MODULE_LIB_NAME;
        if (snprintf_s(path, sizeof(path), sizeof(path) - 1,
            "%s/%s/lib%s" MODULE_SUFFIX_D, fmt, module->moduleMgr->name, module->name) < 0) {
            return NULL;
        }
    }
    BEGET_LOGV("moduleInstall path %s", path);
    currentInstallArgs = &(module->moduleMgr->installArgs);
    handle = dlopen(path, RTLD_LAZY | RTLD_GLOBAL);
    currentInstallArgs = NULL;
    BEGET_CHECK_ONLY_ELOG(handle != NULL, "moduleInstall path %s fail %d", path, errno);
    return handle;
}

/*
 * 用于扫描安装指定目录下所有的插件。
 */
int ModuleMgrInstall(MODULE_MGR *moduleMgr, const char *moduleName,
                     int argc, const char *argv[])
{
    MODULE_ITEM *module;
    BEGET_LOGV("ModuleMgrInstall moduleName %s", moduleName);
    // Get module manager
    BEGET_CHECK(!(moduleMgr == NULL || moduleName == NULL), return -1);
    // Create module item
    module = (MODULE_ITEM *)malloc(sizeof(MODULE_ITEM));
    BEGET_CHECK(module != NULL, return -1);

    module->handle = NULL;
    module->moduleMgr = moduleMgr;

    module->name = strdup(moduleName);
    if (module->name == NULL) {
        moduleDestroy((ListNode *)module);
        return -1;
    }

    // Install
    module->handle = moduleInstall(module, argc, argv);
    if (module->handle == NULL) {
        BEGET_LOGE("Failed to install module %s", moduleName);
        moduleDestroy((ListNode *)module);
        return -1;
    }

    // Add to list
    OH_ListAddTail(&(moduleMgr->modules), (ListNode *)module);

    return 0;
}

const MODULE_INSTALL_ARGS *ModuleMgrGetArgs(void)
{
    return currentInstallArgs;
}

static int stringEndsWith(const char *srcStr, const char *endStr)
{
    int srcStrLen = strlen(srcStr);
    int endStrLen = strlen(endStr);

    BEGET_CHECK(!(srcStrLen < endStrLen), return -1);

    srcStr += (srcStrLen - endStrLen);
    BEGET_CHECK(strcmp(srcStr, endStr) != 0, return (srcStrLen - endStrLen));
    return -1;
}

static void scanModules(MODULE_MGR *moduleMgr, const char *path)
{
    int end;
    int ret;
    DIR *dir;
    struct dirent *file;

    dir = opendir(path);
    BEGET_CHECK(dir != NULL, return);

    while (1) {
        file = readdir(dir);
        if (file == NULL) {
            break;
        }
        if ((file->d_type != DT_REG) && (file->d_type != DT_LNK)) {
            continue;
        }

        // Must be ended with MODULE_SUFFIX_D
        end = stringEndsWith(file->d_name, MODULE_SUFFIX_D);
        if (end <= 0) {
            continue;
        }

        file->d_name[end] = '\0';
        BEGET_LOGV("scanModules module %s", file->d_name);
        if (strncmp(file->d_name, "lib", strlen("lib")) == 0) {
            ret = ModuleMgrInstall(moduleMgr, file->d_name + strlen("lib"), 0, NULL);
        } else {
            ret = ModuleMgrInstall(moduleMgr, file->d_name, 0, NULL);
        }
    }
    BEGET_CHECK_ONLY_ELOG(ret == 0, "module manager install error");
    closedir(dir);
}

/*
 * 用于扫描安装指定目录下所有的插件。
 */
MODULE_MGR *ModuleMgrScan(const char *modulePath)
{
    MODULE_MGR *moduleMgr;
    char path[PATH_MAX];

    moduleMgr = ModuleMgrCreate(modulePath);
    BEGET_CHECK(moduleMgr != NULL, return NULL);

    if (modulePath[0] == '/') {
        scanModules(moduleMgr, modulePath);
    } else if (InUpdaterMode() == 1) {
        BEGET_CHECK(snprintf_s(path, sizeof(path), sizeof(path) - 1,
            "/%s/%s", MODULE_LIB_NAME, modulePath) > 0, return NULL);
        scanModules(moduleMgr, path);
    } else {
        BEGET_CHECK(snprintf_s(path, sizeof(path), sizeof(path) - 1,
            "%s/%s", MODULE_LIB_NAME, modulePath) > 0, return NULL);
        CfgFiles *files = GetCfgFiles(path);
        for (int i = MAX_CFG_POLICY_DIRS_CNT - 1; files && i >= 0; i--) {
            if (files->paths[i]) {
                scanModules(moduleMgr, files->paths[i]);
            }
        }
        FreeCfgFiles(files);
    }
    return moduleMgr;
}

static int moduleCompare(ListNode *node, void *data)
{
    MODULE_ITEM *module = (MODULE_ITEM *)node;

    return strcmp(module->name, (char *)data);
}

/*
 * 卸载指定插件。
 */
void ModuleMgrUninstall(MODULE_MGR *moduleMgr, const char *name)
{
    MODULE_ITEM *module;
    BEGET_CHECK(moduleMgr != NULL, return);
    // Uninstall all modules if no name specified
    if (name == NULL) {
        OH_ListRemoveAll(&(moduleMgr->modules), moduleDestroy);
        return;
    }
    BEGET_LOGV("ModuleMgrUninstall moduleName %s", name);
    // Find module by name
    module = (MODULE_ITEM *)OH_ListFind(&(moduleMgr->modules), (void *)name, moduleCompare);
    BEGET_ERROR_CHECK(module != NULL, return, "Can not find module %s", name);

    // Remove from the list
    OH_ListRemove((ListNode *)module);
    // Destroy the module
    moduleDestroy((ListNode *)module);
}

int ModuleMgrGetCnt(const MODULE_MGR *moduleMgr)
{
    BEGET_CHECK(moduleMgr != NULL, return 0);
    return OH_ListGetCnt(&(moduleMgr->modules));
}

typedef struct tagMODULE_TRAVERSAL_ARGS {
    void  *cookie;
    OhosModuleTraversal traversal;
} MODULE_TRAVERSAL_ARGS;

static int moduleTraversalProc(ListNode *node, void *cookie)
{
    MODULE_ITEM *module;
    MODULE_TRAVERSAL_ARGS *args;
    MODULE_INFO info;

    module = (MODULE_ITEM *)node;
    args = (MODULE_TRAVERSAL_ARGS *)cookie;

    info.cookie = args->cookie;
    info.handle = module->handle;
    info.name = module->name;
    args->traversal(&info);

    return 0;
}

/**
 * @brief Traversing all hooks in the HookManager
 *
 * @param moduleMgr HookManager handle.
 *                If hookMgr is NULL, it will use default HookManager
 * @param cookie traversal cookie.
 * @param traversal traversal function.
 * @return None.
 */
void ModuleMgrTraversal(const MODULE_MGR *moduleMgr, void *cookie, OhosModuleTraversal traversal)
{
    MODULE_TRAVERSAL_ARGS args;
    if (moduleMgr == NULL) {
        return;
    }

    args.cookie = cookie;
    args.traversal = traversal;
    OH_ListTraversal((ListNode *)(&(moduleMgr->modules)), (void *)(&args), moduleTraversalProc, 0);
}
