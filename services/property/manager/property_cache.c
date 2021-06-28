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
#include "property.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "property_manager.h"

#define LABEL "Manager"
#define MAX_PROPERT_IN_WATCH 5
#define NORMAL_MEMORY_FOR_PROPERTY_CACHE 4 * 1024
static WorkSpace g_workSpace;
static pthread_mutex_t cacheLock = PTHREAD_MUTEX_INITIALIZER;

static int InitNormalMemory(WorkSpace *workSpace, u_int32_t spaceSize)
{
    PROPERTY_CHECK(workSpace != NULL, return -1, "Invalid param");
    if (workSpace->area != NULL) {
        return 0;
    }

    void *areaAddr = (void *)mmap(NULL, spaceSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE | MAP_ANON, -1, 0);
    PROPERTY_CHECK(areaAddr != MAP_FAILED, return -1, "Failed to map memory error %s", strerror(errno));
    workSpace->area = (WorkArea*)areaAddr;
    atomic_init(&workSpace->area->serial, 0);
    workSpace->area->dataSize = spaceSize;
    workSpace->area->currOffset = sizeof(WorkArea);
    PROPERTY_LOGI("InitNormalMemory success, currOffset %u firstNode %u dataSize %u",
        workSpace->area->currOffset, workSpace->area->firstNode, workSpace->area->dataSize);
    return 0;
}

static PropertyCacheNode *AllocPropertyCacheNode(WorkSpace *workSpace, u_int32_t size)
{
    PROPERTY_CHECK(workSpace != NULL, return 0, "Invalid param");
    PROPERTY_CHECK((workSpace->area->currOffset + size) < workSpace->area->dataSize, return 0,
        "Failed to allocate currOffset %d, dataSize %d", workSpace->area->currOffset, workSpace->area->dataSize);
    PropertyCacheNode *cache = (PropertyCacheNode *)(workSpace->area->data + workSpace->area->currOffset);
    workSpace->area->currOffset += size;
    return cache;
}

static int CreatePropertyCache(PropertyCache *cache, PropertyWorkSpace *workSpace, PropertyEvaluatePtr evaluate)
{
    PROPERTY_CHECK(cache != NULL && evaluate != NULL, return -1, "Invalid param");
    if (cache->cacheNode != NULL) {
        return 0;
    }
    int ret = InitNormalMemory(&g_workSpace, NORMAL_MEMORY_FOR_PROPERTY_CACHE);
    PROPERTY_CHECK(ret == 0, return -1, "Failed to init normal memory");
    pthread_mutex_init(&cache->lock, NULL);
    cache->serial = GetWorkSpaceSerial(&workSpace->propertySpace);
    cache->cacheCount = 0;
    cache->evaluate = evaluate;
    cache->cacheNode = (PropertyCacheNode *)AllocPropertyCacheNode(&g_workSpace,
        sizeof(PropertyCache) * MAX_PROPERT_IN_WATCH);
    PROPERTY_CHECK(cache->cacheNode != NULL, return -1, "Failed to malloc memory");
    return 0;
}

static int AddPropertyNode(PropertyCache *cache, PropertyWorkSpace *workSpace, const char *name, const char *defValue)
{
    PROPERTY_CHECK(cache != NULL && name != NULL, return -1, "Invalid param");
    PROPERTY_CHECK(cache->cacheCount < MAX_PROPERT_IN_WATCH, return -1, "Full property in cache");

    PropertyCacheNode *cacheNode = &cache->cacheNode[cache->cacheCount++];
    int ret = memcpy_s(cacheNode->value, sizeof(cacheNode->value), defValue, strlen(defValue));
    PROPERTY_CHECK(ret == 0, return -1, "Failed to copy default value");

    ret = ReadPropertyWithCheck(workSpace, name, &cacheNode->handle);
    PROPERTY_CHECK(ret == 0, return -1, "Failed to read property");
    cacheNode->serial = ReadPropertySerial(workSpace, cacheNode->handle);
    return ret;
}

static int CheckCacheNode(PropertyWorkSpace *workSpace, PropertyCacheNode *cacheNode)
{
  return cacheNode && ReadPropertySerial(workSpace, cacheNode->handle) != cacheNode->serial;
}

static void RefreshCacheNode(PropertyWorkSpace *workSpace, PropertyCacheNode *cacheNode)
{
    cacheNode->serial = ReadPropertySerial(workSpace, cacheNode->handle);
    u_int32_t len = sizeof(cacheNode->value);
    ReadPropertyValue(workSpace, cacheNode->handle, cacheNode->value, &len);
}

static const char *TestPropertyCache(PropertyCache *cache, PropertyWorkSpace *workSpace)
{
    int changeDetected;
    if (pthread_mutex_trylock(&cache->lock)) {
        return cache->evaluate(cache->cacheCount, cache->cacheNode);
    }
    if (GetWorkSpaceSerial(&workSpace->propertySpace) != cache->serial) {
        changeDetected = 1;
    }
    for (u_int32_t i = 0; (i < cache->cacheCount) && changeDetected == 0; i++) {
        changeDetected = CheckCacheNode(workSpace, &cache->cacheNode[i]);
    }
    if (changeDetected) {
        for (u_int32_t i = 0; i < cache->cacheCount; i++) {
            RefreshCacheNode(workSpace, &cache->cacheNode[i]);
        }
        cache->serial = GetWorkSpaceSerial(&workSpace->propertySpace);
    }
    pthread_mutex_unlock(&cache->lock);

    return cache->evaluate(cache->cacheCount, cache->cacheNode);
}

const char *DetectPropertyChange(PropertyWorkSpace *workSpace, PropertyCache *cache,
    PropertyEvaluatePtr evaluate, u_int32_t count, const char *properties[][2])
{
    pthread_mutex_lock(&cacheLock);
    while (cache->cacheCount == 0) {
        int ret = CreatePropertyCache(cache, workSpace, evaluate);
        PROPERTY_CHECK(ret == 0, break, "Failed to create cache");
        for (u_int32_t i = 0; i < count; i++) {
            ret = AddPropertyNode(cache, workSpace, properties[i][0], properties[i][1]);
            PROPERTY_CHECK(ret == 0, break, "Failed to add property cache");
        }
        PROPERTY_CHECK(ret == 0, break, "Failed to add property cache");
    }
    pthread_mutex_unlock(&cacheLock);
    return TestPropertyCache(cache, workSpace);
}
