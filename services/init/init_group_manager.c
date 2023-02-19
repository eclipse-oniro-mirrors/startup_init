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
#include "init_group_manager.h"

#include "init_jobs_internal.h"
#include "init_log.h"
#include "init_utils.h"
#include "securec.h"
#include "init_service_manager.h"

static InitWorkspace g_initWorkspace = {0, 0, {0}, {0}, {0}};
int GenerateHashCode(const char *key)
{
    int code = 0;
    size_t keyLen = strlen(key);
    for (size_t i = 0; i < keyLen; i++) {
        code += key[i] - 'A';
    }
    return code;
}

static int GetBootGroupMode(void)
{
    static const char *groupModes[] = {
        "device.boot.group",
        "device.charge.group"
    };
    for (size_t i = 0; i < ARRAY_LENGTH(groupModes); i++) {
        if (strcmp(g_initWorkspace.groupModeStr, groupModes[i]) == 0) {
            return i;
        }
    }
    return (int)GROUP_UNKNOW;
}

static int ParseGroupCfgItem(cJSON *root, int type, const char *itemName)
{
    int itemNumber = 0;
    cJSON *json = GetArrayItem(root, &itemNumber, itemName);
    if (json == NULL) {
        return 0;
    }

    for (int i = 0; i < itemNumber; ++i) {
        cJSON *item = cJSON_GetArrayItem(json, i);
        char *strValue = cJSON_GetStringValue(item);
        if (strValue != NULL) {
            AddGroupNode(type, strValue);
        }
    }
    return 0;
}

static int InitParseGroupCfg_(const char *groupCfg)
{
    INIT_LOGI("Parse group config %s", groupCfg);
    char *fileBuf = ReadFileData(groupCfg);
    INIT_ERROR_CHECK(fileBuf != NULL, return -1, "Failed to read file content %s", groupCfg);
    cJSON *fileRoot = cJSON_Parse(fileBuf);
    INIT_ERROR_CHECK(fileRoot != NULL, free(fileBuf);
        return -1, "Failed to parse json file %s", groupCfg);

    ParseGroupCfgItem(fileRoot, NODE_TYPE_JOBS, "jobs");
    ParseGroupCfgItem(fileRoot, NODE_TYPE_SERVICES, "services");
    ParseGroupCfgItem(fileRoot, NODE_TYPE_GROUPS, "groups");
    cJSON_Delete(fileRoot);
    free(fileBuf);
    return 0;
}

static int InitImportGroupCfg_(InitGroupNode *groupRoot)
{
    InitGroupNode *groupNode = groupRoot;
    while (groupNode != NULL) {
        groupRoot = groupNode->next;
        InitParseGroupCfg_(groupNode->name);
        free(groupNode);
        groupNode = groupRoot;
    }
    return 0;
}

static int InitFreeGroupNodes_(InitGroupNode *groupRoot)
{
    InitGroupNode *groupNode = groupRoot;
    while (groupNode != NULL) {
        groupRoot = groupNode->next;
        if (groupNode->type < NODE_TYPE_GROUPS) { // remove from hashmap
            OH_HashMapRemove(g_initWorkspace.hashMap[groupNode->type], groupNode->name);
        }
        free(groupNode);
        groupNode = groupRoot;
    }
    return 0;
}

static char *GetAbsolutePath(const char *path, const char *cfgName, char *buffer, uint32_t buffSize)
{
    int len = 0;
    size_t cfgNameLen = strlen(cfgName);
    int ext = 0;
    if (cfgNameLen > strlen(".cfg")) {
        ext = strcmp(cfgName + cfgNameLen - strlen(".cfg"), ".cfg") == 0;
    }
    if (cfgName[0] != '/') {
        const char *format = ((ext != 0) ? "%s/%s" : "%s/%s.cfg");
        len = sprintf_s(buffer, buffSize, format, path, cfgName);
    } else {
        const char *format = ((ext != 0) ? "%s" : "%s.cfg");
        len = sprintf_s(buffer, buffSize, format, cfgName);
    }
    if (len <= 0) {
        return NULL;
    }
    buffer[len] = '\0';
    return buffer;
}

static int GroupNodeNodeCompare(const HashNode *node1, const HashNode *node2)
{
    InitGroupNode *groupNode1 = HASHMAP_ENTRY(node1, InitGroupNode, hashNode);
    InitGroupNode *groupNode2 = HASHMAP_ENTRY(node2, InitGroupNode, hashNode);
    return strcmp(groupNode1->name, groupNode2->name);
}

static int GroupNodeKeyCompare(const HashNode *node1, const void *key)
{
    InitGroupNode *groupNode1 = HASHMAP_ENTRY(node1, InitGroupNode, hashNode);
    return strcmp(groupNode1->name, (char *)key);
}

static int GroupNodeGetKeyHashCode(const void *key)
{
    return GenerateHashCode((const char *)key);
}

static int GroupNodeGetNodeHashCode(const HashNode *node)
{
    InitGroupNode *groupNode = HASHMAP_ENTRY(node, InitGroupNode, hashNode);
    return GenerateHashCode((const char *)groupNode->name);
}

static void GroupNodeFree(const HashNode *node)
{
    InitGroupNode *groupNode = HASHMAP_ENTRY(node, InitGroupNode, hashNode);
    free(groupNode);
}

void InitServiceSpace(void)
{
    if (g_initWorkspace.initFlags != 0) {
        return;
    }
    HashInfo info = {
        GroupNodeNodeCompare,
        GroupNodeKeyCompare,
        GroupNodeGetNodeHashCode,
        GroupNodeGetKeyHashCode,
        GroupNodeFree,
        GROUP_HASHMAP_BUCKET
    };
    for (size_t i = 0; i < ARRAY_LENGTH(g_initWorkspace.hashMap); i++) {
        int ret = OH_HashMapCreate(&g_initWorkspace.hashMap[i], &info);
        if (ret != 0) {
            INIT_LOGE("%s", "Failed to create hash map");
        }
    }

    for (int i = 0; i < NODE_TYPE_MAX; i++) {
        g_initWorkspace.groupNodes[i] = NULL;
    }
    // get boot mode, set default mode
    strcpy_s(g_initWorkspace.groupModeStr, sizeof(g_initWorkspace.groupModeStr), BOOT_GROUP_DEFAULT);
    char *data = ReadFileData(BOOT_CMD_LINE);
    if (data != NULL) {
        int ret = GetProcCmdlineValue(BOOT_GROUP_NAME, data,
            g_initWorkspace.groupModeStr, sizeof(g_initWorkspace.groupModeStr));
        if (ret != 0) {
            INIT_LOGV("Failed to get boot group");
            if (GetBootModeFromMisc() == GROUP_CHARGE) {
                strcpy_s(g_initWorkspace.groupModeStr, sizeof(g_initWorkspace.groupModeStr), "device.charge.group");
            }
        }
        free(data);
    }
    INIT_LOGI("boot start %s", g_initWorkspace.groupModeStr);
    g_initWorkspace.groupMode = GetBootGroupMode();
    g_initWorkspace.initFlags = 1;
}

int InitParseGroupCfg(void)
{
    char buffer[128] = {0}; // 128 buffer size
    char *realPath = GetAbsolutePath(GROUP_DEFAULT_PATH,
        g_initWorkspace.groupModeStr, buffer, sizeof(buffer));
    INIT_ERROR_CHECK(realPath != NULL, return -1,
        "Failed to get path for %s", g_initWorkspace.groupModeStr);
    InitParseGroupCfg_(realPath);
    InitGroupNode *groupRoot = g_initWorkspace.groupNodes[NODE_TYPE_GROUPS];
    int level = 0;
    while ((groupRoot != NULL) && (level < GROUP_IMPORT_MAX_LEVEL)) { // for more import
        g_initWorkspace.groupNodes[NODE_TYPE_GROUPS] = NULL;
        InitImportGroupCfg_(groupRoot);
        groupRoot = g_initWorkspace.groupNodes[NODE_TYPE_GROUPS];
        level++;
    }
    InitFreeGroupNodes_(g_initWorkspace.groupNodes[NODE_TYPE_GROUPS]);
    g_initWorkspace.groupNodes[NODE_TYPE_GROUPS] = NULL;
    return 0;
}

InitGroupNode *AddGroupNode(int type, const char *name)
{
    INIT_ERROR_CHECK(type <= NODE_TYPE_MAX, return NULL, "Invalid type");
    INIT_ERROR_CHECK(name != NULL, return NULL, "Invalid name");
    InitGroupNode *groupNode = GetGroupNode(type, name);
    if (groupNode != NULL) {
        return groupNode;
    }
    INIT_LOGV("AddGroupNode type %d name %s", type, name);
    uint32_t nameLen = (uint32_t)strlen(name);
    groupNode = (InitGroupNode *)calloc(1, sizeof(InitGroupNode) + nameLen + 1);
    INIT_ERROR_CHECK(groupNode != NULL, return NULL, "Failed to alloc for group %s", name);
    int ret = memcpy_s(groupNode->name, nameLen + 1, name, nameLen + 1);
    INIT_ERROR_CHECK(ret == 0, free(groupNode);
        return NULL, "Failed to alloc for group %s", name);
    groupNode->type = type;
    groupNode->next = g_initWorkspace.groupNodes[type];
    g_initWorkspace.groupNodes[type] = groupNode;

    if (type < NODE_TYPE_GROUPS) { // add hashmap
        OH_HashMapAdd(g_initWorkspace.hashMap[type], &groupNode->hashNode);
    }
    return groupNode;
}

InitGroupNode *GetGroupNode(int type, const char *name)
{
    if (type >= NODE_TYPE_GROUPS) {
        return NULL;
    }
    INIT_LOGV("GetGroupNode type %d name %s", type, name);
    HashNode *node = OH_HashMapGet(g_initWorkspace.hashMap[type], name);
    if (node == NULL) {
        return NULL;
    }
    return HASHMAP_ENTRY(node, InitGroupNode, hashNode);
}

InitGroupNode *GetNextGroupNode(int type, const InitGroupNode *curr)
{
    INIT_ERROR_CHECK(type <= NODE_TYPE_MAX, return NULL, "Invalid type");
    if (curr == NULL) {
        return g_initWorkspace.groupNodes[type];
    }
    return curr->next;
}

void DelGroupNode(int type, const char *name)
{
    if (type >= NODE_TYPE_GROUPS) {
        return;
    }
    INIT_LOGV("DelGroupNode type %d name %s", type, name);
    OH_HashMapRemove(g_initWorkspace.hashMap[type], name);
    InitGroupNode *groupNode = g_initWorkspace.groupNodes[type];
    InitGroupNode *preNode = groupNode;
    while (groupNode != NULL) {
        if (strcmp(groupNode->name, name) != 0) {
            preNode = groupNode;
            groupNode = groupNode->next;
            continue;
        }
        if (groupNode == g_initWorkspace.groupNodes[type]) {
            g_initWorkspace.groupNodes[type] = groupNode->next;
        } else {
            preNode->next = groupNode->next;
        }
        free(groupNode);
        break;
    }
}

int CheckNodeValid(int type, const char *name)
{
    if (type >= NODE_TYPE_GROUPS) {
        return -1;
    }
    HashNode *node = OH_HashMapGet(g_initWorkspace.hashMap[type], name);
    if (node != NULL) {
        INIT_LOGI("Found %s in %s group", name, type == NODE_TYPE_JOBS ? "job" : "service");
        return 0;
    }
    if (g_initWorkspace.groupMode == GROUP_BOOT) {
        // for boot start, can not start charger service
        if (strcmp(name, "charger") == 0) {
            return -1;
        }
        return 0;
    }
    return -1;
}

HashMapHandle GetGroupHashMap(int type)
{
    if (type >= NODE_TYPE_GROUPS) {
        return NULL;
    }
    return g_initWorkspace.hashMap[type];
}

#ifdef STARTUP_INIT_TEST
InitWorkspace *GetInitWorkspace(void)
{
    return &g_initWorkspace;
}
#endif