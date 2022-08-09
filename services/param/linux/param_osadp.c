/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "param_osadp.h"

#include <errno.h>
#include <sys/mman.h>

#include "param_message.h"
#include "param_utils.h"

INIT_LOCAL_API void paramMutexEnvInit(void)
{
}

INIT_LOCAL_API int ParamRWMutexCreate(ParamRWMutex *lock)
{
    PARAM_CHECK(lock != NULL, return -1, "Invalid lock");
    pthread_rwlockattr_t rwlockatt;
    pthread_rwlockattr_init(&rwlockatt);
    pthread_rwlockattr_setpshared(&rwlockatt, PTHREAD_PROCESS_SHARED);
    pthread_rwlock_init(&lock->rwlock, &rwlockatt);
    return 0;
}

INIT_LOCAL_API int ParamRWMutexWRLock(ParamRWMutex *lock)
{
    PARAM_CHECK(lock != NULL, return -1, "Invalid lock");
    pthread_rwlock_wrlock(&lock->rwlock);
    return 0;
}
INIT_LOCAL_API int ParamRWMutexRDLock(ParamRWMutex *lock)
{
    PARAM_CHECK(lock != NULL, return -1, "Invalid lock");
    pthread_rwlock_rdlock(&lock->rwlock);
    return 0;
}
INIT_LOCAL_API int ParamRWMutexUnlock(ParamRWMutex *lock)
{
    PARAM_CHECK(lock != NULL, return -1, "Invalid lock");
    pthread_rwlock_unlock(&lock->rwlock);
    return 0;
}

INIT_LOCAL_API int ParamRWMutexDelete(ParamRWMutex *lock)
{
    PARAM_CHECK(lock != NULL, return -1, "Invalid lock");
    int ret = pthread_rwlock_destroy(&lock->rwlock);
    PARAM_CHECK(ret == 0, return -1, "Failed to mutex lock ret %d", ret);
    return 0;
}

INIT_LOCAL_API int ParamMutexCreate(ParamMutex *mutex)
{
    return 0;
}
INIT_LOCAL_API int ParamMutexPend(ParamMutex *mutex)
{
    return 0;
}
INIT_LOCAL_API int ParamMutexPost(ParamMutex *mutex)
{
    return 0;
}
INIT_LOCAL_API int ParamMutexDelete(ParamMutex *mutex)
{
    return 0;
}

INIT_LOCAL_API void *GetSharedMem(const char *fileName, MemHandle *handle, uint32_t spaceSize, int readOnly)
{
    PARAM_CHECK(fileName != NULL, return NULL, "Invalid filename or handle");
    int mode = readOnly ? O_RDONLY : O_CREAT | O_RDWR | O_TRUNC;
    int fd = open(fileName, mode, S_IRWXU | S_IRWXG | S_IROTH);
    PARAM_CHECK(fd >= 0, return NULL, "Open file %s mode %x fail error %d", fileName, mode, errno);

    int prot = PROT_READ;
    if (!readOnly) {
        prot = PROT_READ | PROT_WRITE;
        ftruncate(fd, spaceSize);
    }
    void *areaAddr = (void *)mmap(NULL, spaceSize, prot, MAP_SHARED, fd, 0);
    PARAM_CHECK(areaAddr != MAP_FAILED && areaAddr != NULL, close(fd);
        return NULL, "Failed to map memory error %d spaceSize %d", errno, spaceSize);
    close(fd);
    return areaAddr;
}

INIT_LOCAL_API void FreeSharedMem(const MemHandle *handle, void *mem, uint32_t dataSize)
{
    PARAM_CHECK(mem != NULL && handle != NULL, return, "Invalid mem or handle");
    munmap((char *)mem, dataSize);
}

INIT_LOCAL_API uint32_t Difftime(time_t curr, time_t base)
{
    return (uint32_t)difftime(curr, base);
}