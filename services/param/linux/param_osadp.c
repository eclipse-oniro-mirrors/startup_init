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

#include <pthread.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>

#include "param_message.h"
#include "param_utils.h"

int ParamRWMutexCreate(ParamRWMutex *lock)
{
    return 0;
}
int ParamRWMutexWRLock(ParamRWMutex *lock)
{
    return 0;
}
int ParamRWMutexRDLock(ParamRWMutex *lock)
{
    return 0;
}
int ParamRWMutexUnlock(ParamRWMutex *lock)
{
    return 0;
}

int ParamRWMutexDelete(ParamRWMutex *lock)
{
    return 0;
}

int ParamMutexCeate(ParamMutex *mutex)
{
    return 0;
}
int ParamMutexPend(ParamMutex *mutex)
{
    return 0;
}
int ParamMutexPost(ParamMutex *mutex)
{
    return 0;
}
int ParamMutexDelete(ParamMutex *mutex)
{
    return 0;
}

void *GetSharedMem(const char *fileName, MemHandle *handle, uint32_t spaceSize, int readOnly)
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
        return NULL, "Failed to map memory error %d areaAddr %p spaceSize %d", errno, areaAddr, spaceSize);
    close(fd);
    return areaAddr;
}

void FreeSharedMem(const MemHandle *handle, void *mem, uint32_t dataSize)
{
    PARAM_CHECK(mem != NULL && handle != NULL, return, "Invalid mem or handle");
    munmap((char *)mem, dataSize);
}