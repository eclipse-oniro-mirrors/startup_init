/*
 * Copyright (c) 2020 Huawei Device Co., Ltd.
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

#include "property_manager.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define LABEL "Manager"

#ifdef PROPERTY_SUPPORT_SELINUX
static int SelinuxAuditCallback(void *data,
    __attribute__((unused))security_class_t class, char *msgBuf, size_t msgSize)
{
    PropertyAuditData *auditData = (PropertyAuditData*)(data);
    PROPERTY_CHECK(auditData != NULL, return PROPERTY_CODE_INVALID_PARAM, "Invalid param");
    PROPERTY_CHECK(auditData->name != NULL, return PROPERTY_CODE_INVALID_PARAM, "Invalid param");
    PROPERTY_CHECK(auditData->cr != NULL, return PROPERTY_CODE_INVALID_PARAM, "Invalid param");
    snprintf(msgBuf, msgSize, "property=%s pid=%d uid=%d gid=%d",
        auditData->name, auditData->cr->pid, auditData->cr->uid, auditData->cr->gid);
    return 0;
}
#endif

int InitPropertyWorkSpace(PropertyWorkSpace *workSpace, int onlyRead, const char *context)
{
    u_int32_t flags = atomic_load_explicit(&workSpace->flags, memory_order_relaxed);
    PROPERTY_LOGI("InitPropertyWorkSpace flags %x", flags);
    if ((flags & WORKSPACE_FLAGS_INIT) == WORKSPACE_FLAGS_INIT) {
        return 0;
    }

#ifdef PROPERTY_SUPPORT_SELINUX
    union selinux_callback cb;
    cb.func_audit = SelinuxAuditCallback;
    selinux_set_callback(SELINUX_CB_AUDIT, cb);
#endif

#ifdef PROPERTY_SUPPORT_SELINUX
    if (context && fsetxattr(fd, XATTR_NAME_SELINUX, context, strlen(context) + 1, 0) != 0) {
        PROPERTY_LOGI("fsetxattr context %s fail", context);
    }
#endif
    PROPERTY_CHECK(workSpace != NULL, return PROPERTY_CODE_INVALID_NAME, "Invalid param");
    workSpace->propertySpace.compareTrieNode = CompareTrieDataNode;
    workSpace->propertySpace.allocTrieNode = AllocateTrieDataNode;
    int ret = InitWorkSpace(PROPERTY_STORAGE_PATH, &workSpace->propertySpace, onlyRead);

    // 保存selinux 使用的属性标签值
    workSpace->propertyLabelSpace.compareTrieNode = CompareTrieNode; // 必须先设置
    workSpace->propertyLabelSpace.allocTrieNode = AllocateTrieNode;
    ret |= InitWorkSpace(PROPERTY_INFO_PATH, &workSpace->propertyLabelSpace, onlyRead);
    atomic_store_explicit(&workSpace->flags, WORKSPACE_FLAGS_INIT, memory_order_release);
    return ret;
}

void ClosePropertyWorkSpace(PropertyWorkSpace *workSpace)
{
    CloseWorkSpace(&workSpace->propertySpace);
    CloseWorkSpace(&workSpace->propertyLabelSpace);
    atomic_store_explicit(&workSpace->flags, 0, memory_order_release);
}

int WritePropertyInfo(PropertyWorkSpace *workSpace, SubStringInfo *info, int subStrNumber)
{
    PROPERTY_CHECK(workSpace != NULL && info != NULL && subStrNumber > SUBSTR_INFO_NAME,
        return PROPERTY_CODE_INVALID_PARAM, "Failed to check param");
    const char *name = info[SUBSTR_INFO_NAME].value;
    char *label = NULL;
    char *type = NULL;
    if (subStrNumber >= SUBSTR_INFO_LABEL) {
        label = info[SUBSTR_INFO_LABEL].value;
    } else {
        label = "u:object_r:default_prop:s0";
    }
    if (subStrNumber >= SUBSTR_INFO_TYPE) {
        type = info[SUBSTR_INFO_TYPE].value;
    } else {
        type = "string";
    }

    int ret = CheckPropertyName(name, 1);
    PROPERTY_CHECK(ret == 0, return ret, "Illegal property name %s", name);

    // 先保存标签值
    TrieNode *node = AddTrieNode(&workSpace->propertyLabelSpace,
        workSpace->propertyLabelSpace.rootNode, label, strlen(label));
    PROPERTY_CHECK(node != NULL, return PROPERTY_CODE_REACHED_MAX, "Failed to add label");
    u_int32_t offset = GetTrieNodeOffset(&workSpace->propertyLabelSpace, node);

    TrieDataNode *dataNode = AddTrieDataNode(&workSpace->propertySpace, name, strlen(name));
    PROPERTY_CHECK(dataNode != NULL, return PROPERTY_CODE_REACHED_MAX, "Failed to add node %s", name);
    TrieNode *entry = (TrieNode *)GetTrieNode(&workSpace->propertyLabelSpace, &dataNode->labelIndex);
    if (entry != 0) { // 已经存在label
        PROPERTY_LOGE("Has been set label %s old label %s new label: %s", name, entry->key, label);
    }
    SaveIndex(&dataNode->labelIndex, offset);
    return 0;
}

int AddProperty(WorkSpace *workSpace, const char *name, const char *value)
{
    PROPERTY_CHECK(workSpace != NULL && name != NULL && value != NULL,
        return PROPERTY_CODE_INVALID_PARAM, "Failed to check param");

    TrieDataNode *node = AddTrieDataNode(workSpace, name, strlen(name));
    PROPERTY_CHECK(node != NULL, return PROPERTY_CODE_REACHED_MAX, "Failed to add node");
    DataEntry *entry = (DataEntry *)GetTrieNode(workSpace, &node->dataIndex);
    if (entry == NULL) {
        u_int32_t offset = AddData(workSpace, name, strlen(name), value, strlen(value));
        PROPERTY_CHECK(offset != 0, return PROPERTY_CODE_REACHED_MAX, "Failed to allocate name %s", name);
        SaveIndex(&node->dataIndex, offset);
    }
    PROPERTY_LOGI("AddProperty trie %p dataIndex %u name %s", node, node->dataIndex, name);
    atomic_store_explicit(&workSpace->area->serial,
                            atomic_load_explicit(&workSpace->area->serial, memory_order_relaxed) + 1,
                            memory_order_release);
    futex_wake(&workSpace->area->serial, INT_MAX);
    return 0;
}

int UpdateProperty(WorkSpace *workSpace, u_int32_t *dataIndex, const char *name, const char *value)
{
    PROPERTY_CHECK(workSpace != NULL && name != NULL && value != NULL,
        return PROPERTY_CODE_INVALID_PARAM, "Failed to check param");

    DataEntry *entry = (DataEntry *)GetTrieNode(workSpace, dataIndex);
    if (entry == NULL) {
        PROPERTY_LOGE("Failed to update property value %s %u", name, *dataIndex);
        return -1;
    }
    u_int32_t keyLen = DATA_ENTRY_KEY_LEN(entry);
    PROPERTY_CHECK(keyLen == strlen(name), return PROPERTY_CODE_INVALID_NAME, "Failed to check name len %s", name);

    u_int32_t serial = atomic_load_explicit(&entry->serial, memory_order_relaxed);
    serial |= 1;
    atomic_store_explicit(&entry->serial, serial | 1, memory_order_release);
    atomic_thread_fence(memory_order_release);

    int ret = UpdateDataValue(entry, value);
    if (ret != 0) {
        PROPERTY_LOGE("Failed to update property value %s %s", name, value);
    }
    atomic_store_explicit(&entry->serial, serial + 1, memory_order_release);
    futex_wake(&entry->serial, INT_MAX);

    atomic_store_explicit(&workSpace->area->serial,
        atomic_load_explicit(&workSpace->area->serial, memory_order_relaxed) + 1, memory_order_release);
    futex_wake(&workSpace->area->serial, INT_MAX);
    return ret;
}

DataEntry *FindProperty(WorkSpace *workSpace, const char *name)
{
    PROPERTY_CHECK(workSpace != NULL, return NULL, "Failed to check param");
    PROPERTY_CHECK(name != NULL, return NULL, "Invalid param size");

    TrieDataNode *node = FindTrieDataNode(workSpace, name, strlen(name), 0);
    if (node != NULL) {
        return (DataEntry *)GetTrieNode(workSpace, &node->dataIndex);
    }
    return NULL;
}

int WritePropertyWithCheck(PropertyWorkSpace *workSpace,
    const PropertySecurityLabel *srcLabel, const char *name, const char *value)
{
    PROPERTY_CHECK(workSpace != NULL && srcLabel != NULL, return PROPERTY_CODE_INVALID_PARAM, "Invalid param");
    PROPERTY_CHECK(value != NULL && name != NULL, return PROPERTY_CODE_INVALID_PARAM, "Invalid param");

    u_int32_t flags = atomic_load_explicit(&workSpace->flags, memory_order_relaxed);
    if ((flags & WORKSPACE_FLAGS_INIT) != WORKSPACE_FLAGS_INIT) {
        return PROPERTY_CODE_NOT_INIT;
    }

    int ret = CheckPropertyName(name, 0);
    PROPERTY_CHECK(ret == 0, return ret, "Illegal property name %s", name);

    // 取最长匹配的属性的label
    TrieDataNode *propertInfo = FindTrieDataNode(&workSpace->propertySpace, name, strlen(name), 1);
    ret = CanWriteProperty(workSpace, srcLabel, propertInfo, name, value);
    PROPERTY_CHECK(ret == 0, return ret, "Permission to write property %s", name);

    return WriteProperty(&workSpace->propertySpace, name, value);
}

int WriteProperty(WorkSpace *workSpace, const char *name, const char *value)
{
    PROPERTY_CHECK(workSpace != NULL, return PROPERTY_CODE_INVALID_PARAM, "Invalid param");
    PROPERTY_CHECK(value != NULL && name != NULL, return PROPERTY_CODE_INVALID_PARAM, "Invalid param");

    TrieDataNode *node = FindTrieDataNode(workSpace, name, strlen(name), 0);
    int ret = CheckPropertyValue(workSpace, node, name, value);
    PROPERTY_CHECK(ret == 0, return ret, "Invalid value %s %s", name, value);

    if (node != NULL) {
        return UpdateProperty(workSpace, &node->dataIndex, name, value);
    }
    return AddProperty(workSpace, name, value);
}

int ReadPropertyWithCheck(PropertyWorkSpace *workSpace, const char *name, PropertyHandle *handle)
{
    PROPERTY_CHECK(handle != NULL, return PROPERTY_CODE_INVALID_PARAM, "Invalid param");
    PROPERTY_CHECK(workSpace != NULL && name != NULL, return PROPERTY_CODE_INVALID_PARAM, "Invalid param");
    u_int32_t flags = atomic_load_explicit(&workSpace->flags, memory_order_relaxed);
    if ((flags & WORKSPACE_FLAGS_INIT) != WORKSPACE_FLAGS_INIT) {
        return PROPERTY_CODE_NOT_INIT;
    }

    *handle = 0;
    // 取最长匹配
    TrieDataNode *propertyInfo = FindTrieDataNode(&workSpace->propertySpace, name, strlen(name), 1);
    int ret = CanReadProperty(workSpace, propertyInfo == NULL ? 0 : propertyInfo->labelIndex, name);
    PROPERTY_CHECK(ret == 0, return ret, "Permission to read property %s", name);

    // 查找结点
    TrieDataNode *node = FindTrieDataNode(&workSpace->propertySpace, name, strlen(name), 0);
    if (node != NULL && node->dataIndex != 0) {
        PROPERTY_LOGI("ReadPropertyWithCheck trie %p dataIndex %u name %s", node, node->dataIndex, name);
        *handle = node->dataIndex;
        return 0;
    }
    return PROPERTY_CODE_NOT_FOUND_PROP;
}

int ReadPropertyValue(PropertyWorkSpace *workSpace, PropertyHandle handle, char *value, u_int32_t *len)
{
    PROPERTY_CHECK(workSpace != NULL, return PROPERTY_CODE_INVALID_PARAM, "Invalid param");
    u_int32_t flags = atomic_load_explicit(&workSpace->flags, memory_order_relaxed);
    if ((flags & WORKSPACE_FLAGS_INIT) != WORKSPACE_FLAGS_INIT) {
        return PROPERTY_CODE_NOT_INIT;
    }
    DataEntry *entry = (DataEntry *)GetTrieNode(&workSpace->propertySpace, &handle);
    if (entry == NULL) {
        return -1;
    }

    if (value == NULL) {
        *len = DATA_ENTRY_DATA_LEN(entry);;
        return 0;
    }

    while (1) {
        u_int32_t serial = GetDataSerial(entry);
        int ret = GetDataValue(entry, value, *len);
        PROPERTY_CHECK(ret == 0, return ret, "Failed to get value");
        atomic_thread_fence(memory_order_acquire);
        if (serial == atomic_load_explicit(&(entry->serial), memory_order_relaxed)) {
            return 0;
        }
    }
    return 0;
}

int ReadPropertyName(PropertyWorkSpace *workSpace, PropertyHandle handle, char *name, u_int32_t len)
{
    PROPERTY_CHECK(workSpace != NULL && name != NULL, return PROPERTY_CODE_INVALID_PARAM, "Invalid param");
    DataEntry *entry = (DataEntry *)GetTrieNode(&workSpace->propertySpace, &handle);
    if (entry == NULL) {
        return -1;
    }
    return GetDataValue(entry, name, len);
}

u_int32_t ReadPropertySerial(PropertyWorkSpace *workSpace, PropertyHandle handle)
{
    PROPERTY_CHECK(workSpace != NULL, return 0, "Invalid param");
    DataEntry *entry = (DataEntry *)GetTrieNode(&workSpace->propertySpace, &handle);
    if (entry == NULL) {
        return 0;
    }
    return GetDataSerial(entry);
}

int CheckControlPropertyPerms(PropertyWorkSpace *workSpace,
    const PropertySecurityLabel *srcLabel, const char *name, const char *value)
{
    PROPERTY_CHECK(srcLabel != NULL && name != NULL && value != NULL,
        return PROPERTY_CODE_INVALID_PARAM, "Invalid param");

    char * ctrlName[] = {
        "ctl.start",  "ctl.stop", "ctl.restart"
    };
    size_t size1 = strlen("ctl.") +  strlen(value);
    size_t size2 = strlen(name) +  strlen(value) + 1;
    size_t size = ((size1 > size2) ? size1 : size2) + 1;
    char *legacyName = (char*)malloc(size);
    PROPERTY_CHECK(legacyName != NULL, return PROPERTY_CODE_INVALID_PARAM, "Failed to alloc memory");

    // We check the legacy method first but these properties are dontaudit, so we only log an audit
    // if the newer method fails as well.  We only do this with the legacy ctl. properties.
    for (size_t i = 0; i < sizeof(ctrlName) / sizeof(char*); i++) {
        if (strcmp(name, ctrlName[i]) == 0) {
            // The legacy permissions model is that ctl. properties have their name ctl.<action> and
            // their value is the name of the service to apply that action to.  Permissions for these
            // actions are based on the service, so we must create a fake name of ctl.<service> to
            // check permissions.
            int n = snprintf_s(legacyName, size, size, "ctl.%s", value);
            PROPERTY_CHECK(n > 0, free(legacyName); return PROPERTY_CODE_INVALID_PARAM, "Failed to snprintf value");
            legacyName[n] = '\0';

            TrieDataNode *node = FindTrieDataNode(&workSpace->propertySpace, legacyName, strlen(legacyName), 1);
            int ret = CheckMacPerms(workSpace, srcLabel, legacyName, node == NULL ? 0 : node->labelIndex);
            if (ret == 0) {
                free(legacyName);
                return 0;
            }
            break;
        }
    }
    int n = snprintf_s(legacyName, size, size, "%s$%s", name, value);
    PROPERTY_CHECK(n > 0, free(legacyName); return PROPERTY_CODE_INVALID_PARAM, "Failed to snprintf value");

    TrieDataNode *node = FindTrieDataNode(&workSpace->propertySpace, name, strlen(name), 1);
    int ret = CheckMacPerms(workSpace, srcLabel, name, node == NULL ? 0 : node->labelIndex);
    free(legacyName);
    return ret;
}

int CheckPropertyName(const char *name, int propInfo)
{
    size_t nameLen = strlen(name);
    if (nameLen >= PROPERTY_VALUE_LEN_MAX) {
        return PROPERTY_CODE_INVALID_NAME;
    }

    if (nameLen < 1 || name[0] == '.' || (!propInfo && name[nameLen - 1] == '.')) {
        PROPERTY_LOGE("CheckPropertyName %s %d", name, propInfo);
        return PROPERTY_CODE_INVALID_NAME;
    }

    /* Only allow alphanumeric, plus '.', '-', '@', ':', or '_' */
    /* Don't allow ".." to appear in a property name */
    for (size_t i = 0; i < nameLen; i++) {
        if (name[i] == '.') {
            if (name[i - 1] == '.') {
                return PROPERTY_CODE_INVALID_NAME;
            }
            continue;
        }
        if (name[i] == '_' || name[i] == '-' || name[i] == '@' || name[i] == ':') {
            continue;
        }
        if (isalnum(name[i])) {
            continue;
        }
        return PROPERTY_CODE_INVALID_NAME;
    }
    return 0;
}

int CheckPropertyValue(WorkSpace *workSpace, const TrieDataNode *node, const char *name, const char *value)
{
    if (IS_READY_ONLY(name)) {
        if (node != NULL && node->dataIndex != 0) {
            PROPERTY_LOGE("Read-only property was already set %s", name);
            return PROPERTY_CODE_READ_ONLY_PROPERTY;
        }
    } else {
        // 限制非read only的属性，防止属性值修改后，原空间不能保存
        PROPERTY_CHECK(strlen(value) < PROPERTY_VALUE_LEN_MAX,
            return PROPERTY_CODE_INVALID_VALUE, "Illegal property value");
    }
    return 0;
}

int CheckMacPerms(PropertyWorkSpace *workSpace,
    const PropertySecurityLabel *srcLabel, const char *name, u_int32_t labelIndex)
{
#ifdef PROPERTY_SUPPORT_SELINUX
    PropertyAuditData auditData;
    auditData.name = name;
    auditData.cr = &srcLabel->cred;

    int ret = 0;
    TrieNode *node = (TrieNode *)GetTrieNode(&workSpace->propertyLabelSpace, &labelIndex);
    if (node != 0) { // 已经存在label
        ret = selinux_check_access(srcLabel, node->key, "property_service", "set", &auditData);
    } else {
        ret = selinux_check_access(srcLabel, "u:object_r:default_prop:s0", "property_service", "set", &auditData);
    }
    return ret == 0 ? 0 : PROPERTY_CODE_PERMISSION_DENIED;
#else
    return 0;
#endif
}

int CanWriteProperty(PropertyWorkSpace *workSpace,
    const PropertySecurityLabel *srcLabel, const TrieDataNode *node, const char *name, const char *value)
{
    PROPERTY_CHECK(workSpace != NULL && name != NULL && value != NULL && srcLabel != NULL,
        return PROPERTY_CODE_INVALID_PARAM, "Invalid param");

    if (strncmp(name, "ctl.", strlen("ctl.")) == 0) { // 处理ctrl TODO
        return CheckControlPropertyPerms(workSpace, srcLabel, name, value);
    }

    int ret = CheckMacPerms(workSpace, srcLabel, name, node == NULL ? 0 : node->labelIndex);
    PROPERTY_CHECK(ret == 0, return ret, "SELinux permission check failed");
    return 0;
}

int CanReadProperty(PropertyWorkSpace *workSpace, u_int32_t labelIndex, const char *name)
{
    PROPERTY_CHECK(workSpace != NULL, return PROPERTY_CODE_INVALID_PARAM, "Invalid param");
    PROPERTY_CHECK(name != NULL, return PROPERTY_CODE_INVALID_PARAM, "Invalid param");
#ifdef PROPERTY_SUPPORT_SELINUX
    PropertyAuditData auditData;
    auditData.name = name;
    UserCred cr = {.pid = 0, .uid = 0, .gid = 0};
    auditData.cr = &cr;

    int ret = 0;
    TrieNode *node = (TrieNode *)GetTrieNode(&workSpace->propertyLabelSpace, &labelIndex);
    if (node != 0) { // 已经存在label
        ret = selinux_check_access(&workSpace->context, node->key, "property_service", "read", &auditData);
    } else {
         ret = selinux_check_access(&workSpace->context, "selinux_check_access", "file", "read", &auditData);
    }
    return ret == 0 ? 0 : PROPERTY_CODE_PERMISSION_DENIED;
#else
    return 0;
#endif
}

int GetSubStringInfo(const char *buff, u_int32_t buffLen, char delimiter, SubStringInfo *info, int subStrNumber)
{
    size_t i = 0;
    // 去掉开始的空格
    for (; i < strlen(buff); i++) {
        if (!isspace(buff[i])) {
            break;
        }
    }
    // 过滤掉注释
    if (buff[i] == '#') {
        return -1;
    }
    // 分割字符串
    int curr = 0;
    int valueCurr = 0;
    for (; i < buffLen; i++) {
        if (buff[i] == '\n' || buff[i] == '\r') {
            break;
        }
        if (buff[i] == delimiter && valueCurr != 0) {
            info[curr].value[valueCurr] = '\0';
            valueCurr = 0;
            curr++;
        } else if (isspace(buff[i])) { // 无效字符，进行过滤
            continue;
        } else {
            if ((valueCurr + 1) >= (int)sizeof(info[curr].value)) {
                continue;
            }
            info[curr].value[valueCurr++] = buff[i];
        }
        if (curr >= subStrNumber) {
            break;
        }
    }
    if (valueCurr > 0) {
        info[curr].value[valueCurr] = '\0';
        valueCurr = 0;
        curr++;
    }
    return curr;
}

int BuildPropertyContent(char *content, u_int32_t contentSize, const char *name, const char *value)
{
    PROPERTY_CHECK(name != NULL && value != NULL && content != NULL, return -1, "Invalid param");
    u_int32_t nameLen = (u_int32_t)strlen(name);
    u_int32_t valueLen = (u_int32_t)strlen(value);
    PROPERTY_CHECK(contentSize >= (nameLen + valueLen + 2), return -1, "Invalid content size %u", contentSize);

    int offset = 0;
    int ret = memcpy_s(content + offset, contentSize - offset, name, nameLen);
    offset += nameLen;
    ret |= memcpy_s(content + offset, contentSize - offset, "=", 1);
    offset += 1;
    ret |= memcpy_s(content + offset, contentSize - offset, value, valueLen);
    offset += valueLen;
    content[offset] = '\0';
    PROPERTY_CHECK(ret == 0, return -1, "Failed to copy porperty");
    offset += 1;
    return offset;
}

int ProcessPropertTraversal(WorkSpace *workSpace, TrieNode *node, void *cookie)
{
    PropertyTraversalContext *context = (PropertyTraversalContext *)cookie;
    TrieDataNode *current = (TrieDataNode *)node;
    if (current == NULL) {
        return 0;
    }
    if (current->dataIndex == 0) {
        return 0;
    }
    context->traversalParamPtr(current->dataIndex, context->context);
    return 0;
}

int TraversalProperty(PropertyWorkSpace *workSpace, TraversalParamPtr walkFunc, void *cookie)
{
    PropertyTraversalContext context = {
        walkFunc, cookie
    };
    return TraversalTrieDataNode(&workSpace->propertySpace,
        (TrieDataNode *)workSpace->propertySpace.rootNode, ProcessPropertTraversal, &context);
}