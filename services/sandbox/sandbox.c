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

#include "sandbox.h"

#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <errno.h>
#include "beget_ext.h"
#include "config_policy_utils.h"
#include "init_utils.h"
#include "cJSON.h"
#include "list.h"
#include "sandbox_namespace.h"
#include "securec.h"

#define SANDBOX_ROOT_TAG "sandbox-root"
#define SANDBOX_MOUNT_PATH_TAG "mount-bind-paths"
#define SANDBOX_MOUNT_FILE_TAG "mount-bind-files"
#define SANDBOX_SOURCE "src-path"
#define SANDBOX_TARGET "sandbox-path"
#define SANDBOX_FLAGS "sandbox-flags"
#define SANDBOX_IGNORE_ERRORS "ignore"
#define SANDBOX_SYMLINK_TAG "symbol-links"
#define SANDBOX_SYMLINK_TARGET "target-name"
#define SANDBOX_SYMLINK_NAME "link-name"

#define SANDBOX_SYSTEM_CONFIG_FILE "etc/sandbox/system-sandbox.json"
#define SANDBOX_CHIPSET_CONFIG_FILE "etc/sandbox/chipset-sandbox.json"

#ifdef STARTUP_INIT_TEST
#define SANDBOX_TEST_CONFIG_FILE "/data/init_ut/test-sandbox.json"
#endif

#define SANDBOX_MOUNT_FLAGS_MS_BIND "bind"
#define SANDBOX_MOUNT_FLAGS_MS_PRIVATE "private"
#define SANDBOX_MOUNT_FLAGS_MS_REC "rec"
#define SANDBOX_MOUNT_FLAGS_MS_MOVE "move"

struct SandboxMountFlags {
    const char *flag;
    unsigned long value;
};

static const struct SandboxMountFlags FLAGS[] = {
    {
        .flag = "bind",
        .value = MS_BIND,
    },
    {
        .flag = "private",
        .value = MS_PRIVATE,
    },
    {
        .flag = "rec",
        .value = MS_REC,
    },
    {
        .flag = "move",
        .value = MS_MOVE,
    }
};

static sandbox_t g_systemSandbox = {};
static sandbox_t g_chipsetSandbox = {};
#ifdef STARTUP_INIT_TEST
static sandbox_t g_testSandbox = {};
#endif

struct SandboxMap {
    const char *name;
    sandbox_t *sandbox;
    const char *configfile;
};

static const struct SandboxMap MAP[] = {
    {
        .name = "system",
        .sandbox = &g_systemSandbox,
        .configfile = SANDBOX_SYSTEM_CONFIG_FILE,
    },
    {
        .name = "chipset",
        .sandbox = &g_chipsetSandbox,
        .configfile = SANDBOX_CHIPSET_CONFIG_FILE,
    },
#ifdef STARTUP_INIT_TEST
    {
        .name = "test",
        .sandbox = &g_testSandbox,
        .configfile = SANDBOX_TEST_CONFIG_FILE,
    }
#endif
};

static unsigned long GetSandboxMountFlags(cJSON *item)
{
    BEGET_ERROR_CHECK(item != NULL, return 0, "Invalid parameter.");
    char *str = cJSON_GetStringValue(item);
    BEGET_CHECK(str != NULL, return 0);
    for (size_t i = 0; i < ARRAY_LENGTH(FLAGS); i++) {
        if (strcmp(str, FLAGS[i].flag) == 0) {
            return FLAGS[i].value;
        }
    }
    return 0;
}

static void FreeSandboxMountInfo(ListNode *list)
{
    if (list == NULL) {
        return;
    }
    mountlist_t *info = ListEntry(list, mountlist_t, node);
    if (info == NULL) {
        return;
    }
    if (info->source != NULL) {
        free(info->source);
        info->source = NULL;
    }
    if (info->target != NULL) {
        free(info->target);
        info->target = NULL;
    }
    free(info);
    info = NULL;
    return;
}

static void FreeSandboxLinkInfo(ListNode *list)
{
    if (list == NULL) {
        return;
    }
    linklist_t *info = ListEntry(list, linklist_t, node);
    if (info == NULL) {
        return;
    }
    if (info->target != NULL) {
        free(info->target);
        info->target = NULL;
    }
    if (info->linkName != NULL) {
        free(info->linkName);
        info->linkName = NULL;
    }
    free(info);
    info = NULL;
    return;
}

static int CompareSandboxListForMountTarget(ListNode *list, void *data)
{
    if ((list == NULL) || (data == NULL)) {
        return -1;
    }
    mountlist_t *info = ListEntry(list, mountlist_t, node);
    if (info == NULL) {
        return -1;
    }
    const char *mountTarget = (const char *)data;
    return strcmp(info->target, mountTarget);
}

static void RemoveOldSandboxMountListNode(ListNode *head, const char *targetMount)
{
    if ((head == NULL) || (targetMount == NULL)) {
        return;
    }
    ListNode *node = OH_ListFind(head, (void *)targetMount, CompareSandboxListForMountTarget);
    if (node == NULL) {
        return;
    }
    OH_ListRemove(node);
    FreeSandboxMountInfo(node);
}

static int CompareSandboxListForLinkName(ListNode *list, void *data)
{
    if ((list == NULL) || (data == NULL)) {
        return -1;
    }
    linklist_t *info = ListEntry(list, linklist_t, node);
    if (info == NULL) {
        return -1;
    }
    const char *linkName = (const char *)data;
    return strcmp(info->linkName, linkName);
}

static void RemoveOldSandboxLinkListNode(ListNode *head, const char *linkName)
{
    if ((head == NULL) || (linkName == NULL)) {
        return;
    }
    ListNode *node = OH_ListFind(head, (void *)linkName, CompareSandboxListForLinkName);
    if (node == NULL) {
        return;
    }
    OH_ListRemove(node);
    FreeSandboxLinkInfo(node);
}

typedef int (*AddInfoToSandboxCallback)(sandbox_t *sandbox, cJSON *item, const char *type);

static int AddMountInfoToSandbox(sandbox_t *sandbox, cJSON *item, const char *type)
{
    BEGET_CHECK(sandbox != NULL && item != NULL && type != NULL, return -1);
    char *srcPath = cJSON_GetStringValue(cJSON_GetObjectItem(item, SANDBOX_SOURCE));
    BEGET_INFO_CHECK(srcPath != NULL, return 0, "Get src-path is null");
    char *dstPath = cJSON_GetStringValue(cJSON_GetObjectItem(item, SANDBOX_TARGET));
    BEGET_INFO_CHECK(dstPath != NULL, return 0, "Get sandbox-path is null");
    cJSON *obj = cJSON_GetObjectItem(item, SANDBOX_FLAGS);
    BEGET_INFO_CHECK(obj != NULL, return 0, "Get sandbox-flags is null");
    int ret = cJSON_IsArray(obj);
    BEGET_INFO_CHECK(ret, return 0, "Sandbox-flags is not array");
    int count = cJSON_GetArraySize(obj);
    BEGET_INFO_CHECK(count > 0, return 0, "Get sandbox-flags array size is zero");
    mountlist_t *tmpMount = (mountlist_t *)calloc(1, sizeof(mountlist_t));
    BEGET_ERROR_CHECK(tmpMount != NULL, return -1, "Failed calloc err=%d", errno);
    tmpMount->source = strdup(srcPath);
    tmpMount->target = strdup(dstPath);
    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(obj, i);
        tmpMount->flags |= GetSandboxMountFlags(item);
    }
    tmpMount->ignoreErrors = false;
    obj = cJSON_GetObjectItem(item, SANDBOX_IGNORE_ERRORS);
    if (obj != NULL) {
        if (cJSON_GetNumberValue(obj) == 1) {
            tmpMount->ignoreErrors = true;
        }
    }
    OH_ListInit(&tmpMount->node);
    if (strcmp(type, SANDBOX_MOUNT_PATH_TAG) == 0) {
        tmpMount->tag = SANDBOX_TAG_MOUNT_PATH;
        RemoveOldSandboxMountListNode(&sandbox->pathMountsHead, dstPath);
        OH_ListAddTail(&sandbox->pathMountsHead, &tmpMount->node);
    } else if (strcmp(type, SANDBOX_MOUNT_FILE_TAG) == 0) {
        tmpMount->tag = SANDBOX_TAG_MOUNT_FILE;
        RemoveOldSandboxMountListNode(&sandbox->fileMountsHead, dstPath);
        OH_ListAddTail(&sandbox->fileMountsHead, &tmpMount->node);
    }
    return 0;
}

static int AddSymbolLinksToSandbox(sandbox_t *sandbox, cJSON *item, const char *type)
{
    BEGET_CHECK(!(sandbox == NULL || item == NULL || type == NULL), return -1);
    BEGET_ERROR_CHECK(strcmp(type, SANDBOX_SYMLINK_TAG) == 0, return -1, "Type is not sandbox symbolLink.");
    char *target = cJSON_GetStringValue(cJSON_GetObjectItem(item, SANDBOX_SYMLINK_TARGET));
    BEGET_ERROR_CHECK(target != NULL, return 0, "Get target-name is null");
    char *name = cJSON_GetStringValue(cJSON_GetObjectItem(item, SANDBOX_SYMLINK_NAME));
    BEGET_ERROR_CHECK(name != NULL, return 0, "Get link-name is null");
    linklist_t *tmpLink = (linklist_t *)calloc(1, sizeof(linklist_t));
    BEGET_ERROR_CHECK(tmpLink != NULL, return -1, "Failed calloc err=%d", errno);
    tmpLink->target = strdup(target);
    tmpLink->linkName = strdup(name);
    OH_ListInit(&tmpLink->node);
    RemoveOldSandboxLinkListNode(&sandbox->linksHead, tmpLink->linkName);
    OH_ListAddTail(&sandbox->linksHead, &tmpLink->node);
    return 0;
}

static int GetSandboxInfo(sandbox_t *sandbox, cJSON *root, const char *itemName)
{
    BEGET_ERROR_CHECK(!(sandbox == NULL || root == NULL || itemName == NULL), return -1,
        "Get sandbox mount info with invalid argument");
    cJSON *obj = cJSON_GetObjectItem(root, itemName);
    BEGET_ERROR_CHECK(obj != NULL, return 0, "Cannot find item \' %s \' in sandbox config", itemName);
    BEGET_ERROR_CHECK(cJSON_IsArray(obj), return 0, "ItemName %s with invalid type, should be array", itemName);

    int counts = cJSON_GetArraySize(obj);
    BEGET_ERROR_CHECK(counts > 0, return 0, "Item %s array size is zero.", itemName);
    AddInfoToSandboxCallback func = NULL;
    if (strcmp(itemName, SANDBOX_MOUNT_PATH_TAG) == 0) {
        func = AddMountInfoToSandbox;
    } else if (strcmp(itemName, SANDBOX_MOUNT_FILE_TAG) == 0) {
        func = AddMountInfoToSandbox;
    } else if (strcmp(itemName, SANDBOX_SYMLINK_TAG) == 0) {
        func = AddSymbolLinksToSandbox;
    } else {
        BEGET_LOGE("Item %s is not support.", itemName);
        return -1;
    }
    for (int i = 0; i < counts; i++) {
        cJSON *item = cJSON_GetArrayItem(obj, i);
        BEGET_ERROR_CHECK(item != NULL, return -1, "Failed get json array item %d", i);
        BEGET_ERROR_CHECK(func(sandbox, item, itemName) == 0, return -1, "Failed add info to sandbox.");
    }
    return 0;
}

static int ParseSandboxConfig(cJSON *root, sandbox_t *sandbox)
{
    BEGET_ERROR_CHECK(!(root == NULL || sandbox == NULL), return -1, "Invalid parameter.");
    // sandbox rootpath must initialize according to the system configuration, and it can only be initialized once.
    if (sandbox->rootPath == NULL) {
        cJSON *sandboxRoot = cJSON_GetObjectItem(root, SANDBOX_ROOT_TAG);
        BEGET_ERROR_CHECK(sandboxRoot != NULL, return -1,
            "Cannot find item \' %s \' in sandbox config", SANDBOX_ROOT_TAG);

        char *rootdir = cJSON_GetStringValue(sandboxRoot);
        if (rootdir != NULL) {
            sandbox->rootPath = strdup(rootdir);
            BEGET_ERROR_CHECK(sandbox->rootPath != NULL, return -1,
                "Get sandbox root path out of memory");
        }
    }
    BEGET_ERROR_CHECK(GetSandboxInfo(sandbox, root, SANDBOX_MOUNT_PATH_TAG) == 0, return -1,
        "config info %s error", SANDBOX_MOUNT_PATH_TAG);
    BEGET_ERROR_CHECK(GetSandboxInfo(sandbox, root, SANDBOX_MOUNT_FILE_TAG) == 0, return -1,
        "config info %s error", SANDBOX_MOUNT_FILE_TAG);
    BEGET_ERROR_CHECK(GetSandboxInfo(sandbox, root, SANDBOX_SYMLINK_TAG) == 0, return -1,
        "config info %s error", SANDBOX_SYMLINK_TAG);
    return 0;
}

static const struct SandboxMap *GetSandboxMapByName(const char *name)
{
    BEGET_ERROR_CHECK(name != NULL, return NULL, "Sandbox map name is NULL.");
    int len = ARRAY_LENGTH(MAP);
    for (int i = 0; i < len; i++) {
        if (strcmp(MAP[i].name, name) == 0) {
            return &MAP[i];
        }
    }
    return NULL;
}

static int ParseInitSandboxConfigFile(sandbox_t *sandbox, const char *sandboxConfigFile, const char *name)
{
    char *contents = ReadFileToBuf(sandboxConfigFile);
    if (contents == NULL) {
        return 0;
    }
    cJSON *root = cJSON_Parse(contents);
    free(contents);
    BEGET_ERROR_CHECK(root != NULL, return -1, "Parse sandbox config \' %s \' failed", sandboxConfigFile);
    int ret = ParseSandboxConfig(root, sandbox);
    cJSON_Delete(root);
    if (ret < 0) {
        DestroySandbox(name);
        return -1;
    }
    return 0;
}

static void ParseInitSandboxConfigPath(sandbox_t *sandbox, const char *sandboxConfig, const char *name)
{
    CfgFiles *files = GetCfgFiles(sandboxConfig);
    for (int i = 0; files && i < MAX_CFG_POLICY_DIRS_CNT; i++) {
        if (files->paths[i]) {
            BEGET_LOGI("Parse sandbox cfg file is %s", files->paths[i]);
            if (ParseInitSandboxConfigFile(sandbox, files->paths[i], name) < 0) {
                break;
            }
        }
    }
    FreeCfgFiles(files);
}

static void InitSandbox(sandbox_t *sandbox, const char *sandboxConfig, const char *name)
{
    BEGET_ERROR_CHECK(!(sandbox == NULL || sandboxConfig == NULL || name == NULL), return,
        "Init sandbox with invalid arguments");
    if (sandbox->isCreated) {
        BEGET_LOGE("Sandbox %s has created.", name);
        return;
    }
    if (UnshareNamespace(CLONE_NEWNS) < 0) {
        return;
    }
    sandbox->ns = GetNamespaceFd("/proc/self/ns/mnt");
    BEGET_ERROR_CHECK(!(sandbox->ns < 0), return, "Get sandbox namespace fd is failed");

    BEGET_ERROR_CHECK(strcpy_s(sandbox->name, MAX_BUFFER_LEN - 1, name) == 0, return, "Failed to copy sandbox name");
    OH_ListInit(&sandbox->pathMountsHead);
    OH_ListInit(&sandbox->fileMountsHead);
    OH_ListInit(&sandbox->linksHead);
    // parse json config
#ifdef STARTUP_INIT_TEST
    (void)ParseInitSandboxConfigFile(sandbox, sandboxConfig, name);
#else
    ParseInitSandboxConfigPath(sandbox, sandboxConfig, name);
#endif
    return;
}

static int CheckAndMakeDir(const char *dir, mode_t mode)
{
    struct stat sb;

    if ((stat(dir, &sb) == 0) && S_ISDIR(sb.st_mode)) {
        BEGET_LOGI("Mount point \' %s \' already exist, no need to mkdir", dir);
        return 0;
    } else {
        if (errno == ENOENT) {
            BEGET_ERROR_CHECK(MakeDirRecursive(dir, mode) == 0, return -1,
                "Failed MakeDirRecursive %s, err=%d", dir, errno);
        } else {
            BEGET_LOGW("Failed to access mount point \' %s \', err = %d", dir, errno);
            return -1;
        }
    }
    return 0;
}

static int BindMount(const char *source, const char *target, unsigned long flags, SandboxTag tag)
{
    if (source == NULL || target == NULL) {
        BEGET_LOGE("Mount with invalid arguments");
        errno = EINVAL;
        return -1;
    }
    unsigned long tmpflags = flags;
    mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    if (tag == SANDBOX_TAG_MOUNT_PATH) {
        BEGET_ERROR_CHECK(CheckAndMakeDir(target, mode) == 0, return -1, "Failed make %s dir.", target);
    } else if (tag == SANDBOX_TAG_MOUNT_FILE) {
        BEGET_ERROR_CHECK(CheckAndCreatFile(target, mode) == 0, return -1, "Failed make %s file.", target);
    } else {
        BEGET_LOGE("Tag is error.");
        return -1;
    }

    BEGET_WARNING_CHECK((tmpflags & MS_BIND) != 0, tmpflags |= MS_BIND,
        "Not configure mount bind, must configure mount bind flag.");
    BEGET_WARNING_CHECK((tmpflags & MS_REC) != 0, tmpflags |= MS_REC,
        "Not configure mount rec, must configure mount rec flag.");

    // do mount
    if (mount(source, target, NULL, tmpflags, NULL) != 0) {
        BEGET_LOGE("Failed to bind mount \' %s \' to \' %s \', err = %d", source, target, errno);
        if (errno != ENOTDIR) {  // mount errno is 'Not a directory' can ignore
            return -1;
        }
    }

    return 0;
}

static bool IsValidSandbox(sandbox_t *sandbox)
{
    BEGET_ERROR_CHECK(sandbox != NULL, return false, "preparing sandbox with invalid argument");

    if (sandbox->rootPath == NULL) {
        return false;
    }

    return true;
}

static int MountSandboxNode(ListNode *list, void *data)
{
    if ((list == NULL) || (data == NULL)) {
        return 0;
    }
    const char *rootPath = (const char *)data;
    mountlist_t *info = ListEntry(list, mountlist_t, node);
    char target[PATH_MAX] = {};
    BEGET_ERROR_CHECK(snprintf_s(target, PATH_MAX, PATH_MAX - 1, "%s%s", rootPath, info->target) > 0,
        return -1, "Failed snprintf_s err=%d", errno);
    int rc = BindMount(info->source, target, info->flags, info->tag);
    if (rc != 0) {
        BEGET_LOGW("Failed bind mount %s to %s.", info->source, target);
        if (info->ignoreErrors == false) {
            return -1;
        }
    }
    return 0;
}

static int MountSandboxInfo(struct ListNode *head, const char *rootPath, SandboxTag tag)
{
    if ((head == NULL) || (rootPath == NULL)) {
        return 0;
    }
    int ret = OH_ListTraversal(head, (void *)rootPath, MountSandboxNode, 1);
    return ret;
}

static int LinkSandboxNode(ListNode *list, void *data)
{
    if ((list == NULL) || (data == NULL)) {
        return 0;
    }
    const char *rootPath = (const char *)data;
    linklist_t *info = ListEntry(list, linklist_t, node);
    char linkName[PATH_MAX] = {0};
    BEGET_ERROR_CHECK(!(snprintf_s(linkName, PATH_MAX, PATH_MAX - 1, "%s%s", rootPath, info->linkName) < 0),
        return -1, "snprintf_s failed, err=%d", errno);
    int rc = symlink(info->target, linkName);
    if (rc != 0) {
        if (errno == EEXIST) {
            BEGET_LOGW("symbol link name \' %s \' already exist", linkName);
        } else {
            BEGET_LOGE("Failed to link \' %s \' to \' %s \', err = %d", info->target, linkName, errno);
            return -1;
        }
    }
    return 0;
}

static int LinkSandboxInfo(struct ListNode *head, const char *rootPath)
{
    if ((head == NULL) || (rootPath == NULL)) {
        return 0;
    }
    int ret = OH_ListTraversal(head, (void *)rootPath, LinkSandboxNode, 1);
    return ret;
}

int PrepareSandbox(const char *name)
{
    BEGET_ERROR_CHECK(name != NULL, return -1, "Prepare sandbox name is NULL.");
    BEGET_ERROR_CHECK(getuid() == 0, return -1, "Current process uid is not root, exit.");
    const struct SandboxMap *map = GetSandboxMapByName(name);
    BEGET_ERROR_CHECK(map != NULL, return -1, "Cannot get sandbox map by name %s.", name);
    sandbox_t *sandbox = map->sandbox;
    BEGET_CHECK(IsValidSandbox(sandbox) == true, return -1);
    BEGET_INFO_CHECK(sandbox->isCreated == false, return 0, "Sandbox %s already created", sandbox->name);
    BEGET_CHECK(sandbox->rootPath != NULL, return -1);
    mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    BEGET_ERROR_CHECK(CheckAndMakeDir(sandbox->rootPath, mode) == 0, return -1, "Failed root %s.", sandbox->rootPath);
    int rc = mount(NULL, "/", NULL, MS_REC | MS_SLAVE, NULL);
    BEGET_ERROR_CHECK(rc == 0, return -1, "Failed set mount slave err = %d", errno);
    rc = BindMount(sandbox->rootPath, sandbox->rootPath, MS_BIND | MS_REC, SANDBOX_TAG_MOUNT_PATH);
    BEGET_ERROR_CHECK(rc == 0, return -1, "Failed to mount rootpath bind err = %d", errno);

    // 1) walk through all mounts and do bind mount
    rc = MountSandboxInfo(&sandbox->pathMountsHead, sandbox->rootPath, SANDBOX_TAG_MOUNT_PATH);
    BEGET_CHECK(rc == 0, return -1);

    rc = MountSandboxInfo(&sandbox->fileMountsHead, sandbox->rootPath, SANDBOX_TAG_MOUNT_FILE);
    BEGET_CHECK(rc == 0, return -1);

    // 2) walk through all links and do symbol link
    rc = LinkSandboxInfo(&sandbox->linksHead, sandbox->rootPath);
    BEGET_CHECK(rc == 0, return -1);

    BEGET_ERROR_CHECK(chdir(sandbox->rootPath) == 0, return -1, "Change to %s, err = %d", sandbox->rootPath, errno);
    BEGET_ERROR_CHECK(syscall(SYS_pivot_root, sandbox->rootPath, sandbox->rootPath) == 0, return -1,
        "Failed system call pivot root, err=%d", errno);
    BEGET_ERROR_CHECK(umount2(".", MNT_DETACH) == 0, return -1, "Failed umount2 MNT_DETACH, err=%d", errno);
    sandbox->isCreated = true;
    return 0;
}

bool InitSandboxWithName(const char *name)
{
    bool isFound = false;
    if (name == NULL) {
        BEGET_LOGE("Init sandbox name is NULL.");
        return isFound;
    }
    const struct SandboxMap *map = GetSandboxMapByName(name);
    if (map != NULL) {
        InitSandbox(map->sandbox, map->configfile, name);
        isFound = true;
    }

    if (!isFound) {
        BEGET_LOGE("Cannot find sandbox with name %s.", name);
    }
    return isFound;
}

void DestroySandbox(const char *name)
{
    if (name == NULL) {
        BEGET_LOGE("Destroy sandbox name is NULL.");
        return;
    }
    const struct SandboxMap *map = GetSandboxMapByName(name);
    if (map == NULL) {
        BEGET_LOGE("Cannot get sandbox map by name %s.", name);
        return;
    }
    sandbox_t *sandbox = map->sandbox;

    BEGET_CHECK(sandbox != NULL, return);

    if (sandbox->rootPath != NULL) {
        free(sandbox->rootPath);
        sandbox->rootPath = NULL;
    }
    OH_ListRemoveAll(&sandbox->linksHead, FreeSandboxLinkInfo);
    OH_ListRemoveAll(&sandbox->fileMountsHead, FreeSandboxMountInfo);
    OH_ListRemoveAll(&sandbox->pathMountsHead, FreeSandboxMountInfo);

    if (sandbox->ns > 0) {
        (void)close(sandbox->ns);
    }
    sandbox->isCreated = false;
    return;
}

int EnterSandbox(const char *name)
{
    if (name == NULL) {
        BEGET_LOGE("Sandbox name is NULL.");
        return -1;
    }
    const struct SandboxMap *map = GetSandboxMapByName(name);
    if (map == NULL) {
        BEGET_LOGE("Cannot get sandbox map by name %s.", name);
        return -1;
    }
    sandbox_t *sandbox = map->sandbox;

    BEGET_CHECK(sandbox != NULL, return -1);
    if (sandbox->isCreated == false) {
        BEGET_LOGE("Sandbox %s has not been created.", name);
        return -1;
    }
    if (sandbox->ns > 0) {
        BEGET_ERROR_CHECK(!(SetNamespace(sandbox->ns, CLONE_NEWNS) < 0), return -1,
            "Cannot enter mount namespace for sandbox \' %s \', err=%d.", name, errno);
    } else {
        BEGET_LOGE("Sandbox \' %s \' namespace fd is invalid.", name);
        return -1;
    }
    return 0;
}

static int DumpSandboxMountInfo(ListNode *list, void *data)
{
    if (list == NULL) {
        return -1;
    }
    mountlist_t *info = ListEntry(list, mountlist_t, node);
    if (info != NULL) {
        if (info->source != NULL) {
            printf("Sandbox mounts list source: %s \n", info->source);
        }
        if (info->target != NULL) {
            printf("Sandbox mounts list target: %s \n", info->target);
        }
    }
    return 0;
}

static int DumpSandboxLinkInfo(ListNode *list, void *data)
{
    if (list == NULL) {
        return -1;
    }
    linklist_t *info = ListEntry(list, linklist_t, node);
    if (info != NULL) {
        if (info->linkName != NULL) {
            printf("Sandbox link list name: %s \n", info->linkName);
        }
        if (info->target != NULL) {
            printf("Sandbox link list target: %s \n", info->target);
        }
    }
    return 0;
}

void DumpSandboxByName(const char *name)
{
    if (name == NULL) {
        BEGET_LOGE("Init sandbox name is NULL.");
        return;
    }
    const struct SandboxMap *map = GetSandboxMapByName(name);
    if (map == NULL) {
        return;
    }
    printf("Sandbox Map name: %s \n", map->name);
    printf("Sandbox Map config file: %s. \n", map->configfile);
    printf("Sandbox name: %s. \n", map->sandbox->name);
    printf("Sandbox root path is %s. \n", map->sandbox->rootPath);
    printf("Sandbox mounts info: \n");
    OH_ListTraversal(&map->sandbox->pathMountsHead, NULL, DumpSandboxMountInfo, 0);
    OH_ListTraversal(&map->sandbox->fileMountsHead, NULL, DumpSandboxMountInfo, 0);
    printf("Sandbox links info: \n");
    OH_ListTraversal(&map->sandbox->linksHead, NULL, DumpSandboxLinkInfo, 0);
    return;
}
