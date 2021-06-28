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

#ifndef BASE_STARTUP_PROPERTY_MANAGER_H
#define BASE_STARTUP_PROPERTY_MANAGER_H
#include <stdio.h>
#include <string.h>

#include "init_log.h"
#include "property.h"
#include "property_trie.h"
#include "securec.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef enum {
    PROPERTY_CODE_INVALID_PARAM = 100,
    PROPERTY_CODE_INVALID_NAME,
    PROPERTY_CODE_INVALID_VALUE,
    PROPERTY_CODE_REACHED_MAX,
    PROPERTY_CODE_PERMISSION_DENIED,
    PROPERTY_CODE_READ_ONLY_PROPERTY,
    PROPERTY_CODE_NOT_SUPPORT,
    PROPERTY_CODE_ERROR_MAP_FILE,
    PROPERTY_CODE_NOT_FOUND_PROP,
    PROPERTY_CODE_NOT_INIT
} PROPERTY_CODE;

#define IS_READY_ONLY(name) strncmp((name), "ro.", strlen("ro.")) == 0
#define LABEL_STRING_LEN 128

#ifdef STARTUP_LOCAL
#define PIPE_NAME "/tmp/propertyservice.sock"
#define PROPERTY_STORAGE_PATH "/media/sf_ubuntu/test/__properties__/property_storage"
#define PROPERTY_PERSIST_PATH "/media/sf_ubuntu/test/property/persist_properties.property"
#define PROPERTY_INFO_PATH "/media/sf_ubuntu/test/__properties__/property_info"
#else
#define PIPE_NAME "/dev/unix/socket/PropertyService"
#define PROPERTY_STORAGE_PATH "/dev/__properties__/property_storage"
#define PROPERTY_PERSIST_PATH "/data/property/persist_properties.property"
#define PROPERTY_INFO_PATH "/dev/__properties__/property_info"
#endif

#define SUBSTR_INFO_NAME  0
#define SUBSTR_INFO_LABEL 1
#define SUBSTR_INFO_TYPE  2
#define SUBSTR_INFO_MAX   4

#define WORKSPACE_FLAGS_INIT    0x01
#define WORKSPACE_FLAGS_LOADED  0x02

#define PROPERTY_LOGI(fmt, ...) STARTUP_LOGI(LABEL, fmt, ##__VA_ARGS__)
#define PROPERTY_LOGE(fmt, ...) STARTUP_LOGE(LABEL, fmt, ##__VA_ARGS__)

#define PROPERTY_CHECK(retCode, exper, ...) \
    if (!(retCode)) {                     \
        PROPERTY_LOGE(__VA_ARGS__);         \
        exper;                            \
    }

#define futex(addr1, op, val, rel, addr2, val3) \
				syscall(SYS_futex, addr1, op, val, rel, addr2, val3)
#define futex_wait_always(addr1) \
				syscall(SYS_futex, addr1, FUTEX_WAIT, *(int*)(addr1), 0, 0, 0)
#define futex_wake_single(addr1) \
				syscall(SYS_futex, addr1, FUTEX_WAKE, 1, 0, 0, 0)

typedef struct UserCred {
  pid_t pid;
  uid_t uid;
  gid_t gid;
} UserCred;

typedef struct {
    char label[LABEL_STRING_LEN];
    UserCred cred;
} PropertySecurityLabel;

typedef struct PropertyAuditData {
    const UserCred *cr;
    const char *name;
} PropertyAuditData;

typedef struct {
    atomic_uint_least32_t flags;
    WorkSpace propertyLabelSpace;
    WorkSpace propertySpace;
    PropertySecurityLabel label;
} PropertyWorkSpace;

typedef struct {
    atomic_uint_least32_t flags;
    WorkSpace persistWorkSpace;
} PropertyPersistWorkSpace;

typedef struct {
    char value[128];
} SubStringInfo;

int InitPropertyWorkSpace(PropertyWorkSpace *workSpace, int onlyRead, const char *context);
void ClosePropertyWorkSpace(PropertyWorkSpace *workSpace);

int ReadPropertyWithCheck(PropertyWorkSpace *workSpace, const char *name, PropertyHandle *handle);
int ReadPropertyValue(PropertyWorkSpace *workSpace, PropertyHandle handle, char *value, u_int32_t *len);
int ReadPropertyName(PropertyWorkSpace *workSpace, PropertyHandle handle, char *name, u_int32_t len);
u_int32_t ReadPropertySerial(PropertyWorkSpace *workSpace, PropertyHandle handle);

int AddProperty(WorkSpace *workSpace, const char *name, const char *value);
int WriteProperty(WorkSpace *workSpace, const char *name, const char *value);
int WritePropertyWithCheck(PropertyWorkSpace *workSpace,
    const PropertySecurityLabel *srcLabel, const char *name, const char *value);
int WritePropertyInfo(PropertyWorkSpace *workSpace, SubStringInfo *info, int subStrNumber);

int CheckPropertyValue(WorkSpace *workSpace, const TrieDataNode *node, const char *name, const char *value);
int CheckPropertyName(const char *name, int propInfo);
int CanReadProperty(PropertyWorkSpace *workSpace, u_int32_t labelIndex, const char *name);
int CanWriteProperty(PropertyWorkSpace *workSpace,
    const PropertySecurityLabel *srcLabel, const TrieDataNode *node, const char *name, const char *value);

int CheckMacPerms(PropertyWorkSpace *workSpace,
    const PropertySecurityLabel *srcLabel, const char *name, u_int32_t labelIndex);
int CheckControlPropertyPerms(PropertyWorkSpace *workSpace,
    const PropertySecurityLabel *srcLabel, const char *name, const char *value);

int GetSubStringInfo(const char *buff, u_int32_t buffLen, char delimiter, SubStringInfo *info, int subStrNumber);
int BuildPropertyContent(char *content, u_int32_t contentSize, const char *name, const char *value);
PropertyWorkSpace *GetPropertyWorkSpace();

typedef void (*TraversalParamPtr)(PropertyHandle handle, void* context);
typedef struct {
    TraversalParamPtr traversalParamPtr;
    void *context;
} PropertyTraversalContext;
int TraversalProperty(PropertyWorkSpace *workSpace, TraversalParamPtr walkFunc, void *cookie);

int InitPersistPropertyWorkSpace(const char *context);
int RefreshPersistProperties(PropertyWorkSpace *workSpace, const char *context);
void ClosePersistPropertyWorkSpace();
int WritePersistProperty(const char *name, const char *value);

const char *DetectPropertyChange(PropertyWorkSpace *workSpace, PropertyCache *cache,
    PropertyEvaluatePtr evaluate, u_int32_t count, const char *properties[][2]);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif