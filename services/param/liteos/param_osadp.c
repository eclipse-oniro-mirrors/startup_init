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
#include "param_osadp.h"

#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#ifdef __LITEOS_A__
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#else
#include "los_task.h"
#endif
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "param_security.h"

#define NSEC_PER_MSEC 1000000LL
#define MSEC_PER_SEC 1000LL

static void TimerHandle(union sigval v)
{
    ParamTimer *timer = (ParamTimer *)v.sival_ptr;
    PARAM_CHECK(timer != NULL, return, "Invalid timer");
    if (timer->timeProcessor != NULL) {
        timer->timeProcessor(timer, timer->context);
    }
}

static void SetTimeSpec(struct timespec *ts, int64_t msec)
{
    ts->tv_sec = msec / MSEC_PER_SEC; // 1000LL ms --> m
    ts->tv_nsec = (msec - ts->tv_sec * MSEC_PER_SEC) * NSEC_PER_MSEC;
}

static int StartTimer(const ParamTimer *paramTimer, int64_t whenMsec, int64_t repeat)
{
    UNUSED(repeat);
    struct itimerspec ts;
    SetTimeSpec(&ts.it_value, whenMsec);
    SetTimeSpec(&ts.it_interval, whenMsec);
    int32_t ret = timer_settime(paramTimer->timerId, 0, &ts, NULL);
    if (ret < 0) {
        PARAM_LOGE("Failed to start timer");
        return -1;
    }
    return 0;
}

int ParamTimerCreate(ParamTaskPtr *timer, ProcessTimer process, void *context)
{
    PARAM_CHECK(timer != NULL && process != NULL, return -1, "Invalid timer");
    ParamTimer *paramTimer = malloc(sizeof(ParamTimer));
    PARAM_CHECK(paramTimer != NULL, return -1, "Failed to create timer");
    paramTimer->timeProcessor = process;
    paramTimer->context = context;

    struct sigevent evp;
    (void)memset_s(&evp, sizeof(evp), 0, sizeof(evp));
    evp.sigev_value.sival_ptr = paramTimer;
    evp.sigev_notify = SIGEV_THREAD;
    evp.sigev_notify_function = TimerHandle;
    int32_t ret = timer_create(CLOCK_REALTIME, &evp, &paramTimer->timerId);
    if (ret < 0) {
        PARAM_LOGE("Failed to create timer");
        free(paramTimer);
        return -1;
    }
    *timer = paramTimer;
    return 0;
}

int ParamTimerStart(const ParamTaskPtr timer, uint64_t timeout, uint64_t repeat)
{
    PARAM_CHECK(timer != NULL, return -1, "Invalid timer");
    ParamTimer *paramTimer = (ParamTimer *)timer;
    PARAM_LOGV("ParamTimerStart timeout %llu ", timeout);
    int32_t ret = StartTimer(paramTimer, timeout, repeat);
    PARAM_CHECK(ret >= 0, return -1, "Failed to start timer");
    return 0;
}

void ParamTimerClose(ParamTaskPtr timer)
{
    PARAM_CHECK(timer != NULL, return, "Invalid timer");
    ParamTimer *paramTimer = (ParamTimer *)timer;
    timer_delete(paramTimer->timerId);
    free(paramTimer);
}

#ifdef __LITEOS_A__
void *GetSharedMem(const char *fileName, MemHandle *handle, uint32_t spaceSize, int readOnly)
{
    PARAM_CHECK(fileName != NULL && handle != NULL, return NULL, "Invalid filename or handle");
    int mode = readOnly ? O_RDONLY : O_CREAT | O_RDWR;
    void *areaAddr = NULL;
    if (!readOnly) {
        int fd = open(fileName, mode, S_IRWXU | S_IRWXG | S_IROTH);
        PARAM_CHECK(fd >= 0, return NULL, "Open file %s mode %x fail error %d", fileName, mode, errno);
        close(fd);
    }
    key_t key = ftok(fileName, 0x03);  // 0x03 flags for shmget
    PARAM_CHECK(key != -1, return NULL, "Invalid errno %d key for %s", errno, fileName);
    handle->shmid = shmget(key, spaceSize, IPC_CREAT | IPC_EXCL | 0666);  // 0666
    if (handle->shmid == -1) {
        handle->shmid = shmget(key, spaceSize, 0);  // 0666
    }
    PARAM_CHECK(handle->shmid != -1, return NULL, "Invalid shmId errno %d for %s", errno, fileName);
    areaAddr = (void *)shmat(handle->shmid, NULL, 0);
    return areaAddr;
}

void FreeSharedMem(const MemHandle *handle, void *mem, uint32_t dataSize)
{
    PARAM_CHECK(mem != NULL && handle != NULL, return, "Invalid mem or handle");
    shmdt(mem);
    shmctl(handle->shmid, IPC_RMID, NULL);
}

void paramMutexEnvInit(void)
{
    return;
}

int ParamRWMutexCreate(ParamRWMutex *lock)
{
    PARAM_CHECK(lock != NULL, return -1, "Invalid lock");
    pthread_rwlockattr_t rwlockatt;
    pthread_rwlockattr_init(&rwlockatt);
    pthread_rwlockattr_setpshared(&rwlockatt, PTHREAD_PROCESS_SHARED);
    pthread_rwlock_init(&lock->rwlock, &rwlockatt);
    return 0;
}

int ParamRWMutexWRLock(ParamRWMutex *lock)
{
    PARAM_CHECK(lock != NULL, return -1, "Invalid lock");
    pthread_rwlock_wrlock(&lock->rwlock);
    return 0;
}
int ParamRWMutexRDLock(ParamRWMutex *lock)
{
    PARAM_CHECK(lock != NULL, return -1, "Invalid lock");
    pthread_rwlock_rdlock(&lock->rwlock);
    return 0;
}
int ParamRWMutexUnlock(ParamRWMutex *lock)
{
    PARAM_CHECK(lock != NULL, return -1, "Invalid lock");
    pthread_rwlock_unlock(&lock->rwlock);
    return 0;
}

int ParamRWMutexDelete(ParamRWMutex *lock)
{
    PARAM_CHECK(lock != NULL, return -1, "Invalid lock");
    uint32_t ret = pthread_rwlock_destroy(&lock->rwlock);
    PARAM_CHECK(ret == 0, return -1, "Failed to mutex lock ret %d", ret);
    return 0;
}

int ParamMutexCeate(ParamMutex *mutex)
{
    PARAM_CHECK(mutex != NULL, return -1, "Invalid mutex");
    pthread_mutexattr_t mutexattr;
    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&mutex->mutex, &mutexattr);
    return 0;
}
int ParamMutexPend(ParamMutex *mutex)
{
    PARAM_CHECK(mutex != NULL, return -1, "Invalid mutex");
    if (pthread_mutex_lock(&mutex->mutex) != 0) {
        PARAM_LOGE("Failed to batch begin to save param errno %d", errno);
        return errno;
    }
    return 0;
}
int ParamMutexPost(ParamMutex *mutex)
{
    PARAM_CHECK(mutex != NULL, return -1, "Invalid mutex");
    pthread_mutex_unlock(&mutex->mutex);
    return 0;
}

int ParamMutexDelete(ParamMutex *mutex)
{
    PARAM_CHECK(mutex != NULL, return -1, "Invalid lock");
    uint32_t ret = pthread_mutex_destroy(&mutex->mutex);
    PARAM_CHECK(ret == 0, return -1, "Failed to mutex lock ret %d", ret);
    return 0;
}
#endif

#ifdef __LITEOS_M__
void *GetSharedMem(const char *fileName, MemHandle *handle, uint32_t spaceSize, int readOnly)
{
    PARAM_CHECK(spaceSize <= PARAM_WORKSPACE_MAX, return, "Invalid spaceSize %u", spaceSize);
    UNUSED(fileName);
    UNUSED(handle);
    UNUSED(readOnly);
    return (void *)malloc(spaceSize);
}

void FreeSharedMem(const MemHandle *handle, void *mem, uint32_t dataSize)
{
    PARAM_CHECK(mem != NULL && handle != NULL, return, "Invalid mem or handle");
    UNUSED(handle);
    UNUSED(dataSize);
    free(mem);
}

void paramMutexEnvInit(void)
{
    uint32_t ret = OsMuxInit();
    PARAM_CHECK(ret == LOS_OK, return, "Failed to init mutex ret %d", ret);
    return;
}

int ParamRWMutexCreate(ParamRWMutex *lock)
{
    PARAM_CHECK(lock != NULL, return -1, "Invalid lock");
    uint32_t ret = LOS_MuxCreate(&lock->mutex);
    PARAM_CHECK(ret == LOS_OK, return -1, "Failed to init mutex ret %d", ret);
    return 0;
}

int ParamRWMutexWRLock(ParamRWMutex *lock)
{
    PARAM_CHECK(lock != NULL, return -1, "Invalid lock");
    uint32_t ret = LOS_MuxPend(lock->mutex, LOS_WAIT_FOREVER);
    PARAM_CHECK(ret == LOS_OK, return -1, "Failed to mutex lock ret %d %d", ret, lock->mutex);
    return 0;
}

int ParamRWMutexRDLock(ParamRWMutex *lock)
{
    PARAM_CHECK(lock != NULL, return -1, "Invalid lock");
    uint32_t ret = LOS_MuxPend(lock->mutex, LOS_WAIT_FOREVER);
    PARAM_CHECK(ret == LOS_OK, return -1, "Failed to mutex lock ret %d %d", ret, lock->mutex);
    return 0;
}

int ParamRWMutexUnlock(ParamRWMutex *lock)
{
    PARAM_CHECK(lock != NULL, return -1, "Invalid lock");
    uint32_t ret = LOS_MuxPost(lock->mutex);
    PARAM_CHECK(ret == LOS_OK, return -1, "Failed to mutex lock ret %d %d", ret, lock->mutex);
    return 0;
}

int ParamRWMutexDelete(ParamRWMutex *lock)
{
    PARAM_CHECK(lock != NULL, return -1, "Invalid lock");
    uint32_t ret = LOS_MuxDelete(lock->mutex);
    PARAM_CHECK(ret == LOS_OK, return -1, "Failed to mutex lock ret %d %d", ret, lock->mutex);
    return 0;
}

int ParamMutexCeate(ParamMutex *mutex)
{
    PARAM_CHECK(mutex != NULL, return -1, "Invalid lock");
    uint32_t ret = LOS_MuxCreate(&mutex->mutex);
    PARAM_CHECK(ret == LOS_OK, return -1, "Failed to init mutex ret %d", ret);
    return 0;
}

int ParamMutexPend(ParamMutex *mutex)
{
    PARAM_CHECK(mutex != NULL, return -1, "Invalid lock");
    uint32_t ret = LOS_MuxPend(mutex->mutex, LOS_WAIT_FOREVER);
    PARAM_CHECK(ret == LOS_OK, return -1, "Failed to mutex lock ret %d %d", ret, mutex->mutex);
    return 0;
}

int ParamMutexPost(ParamMutex *mutex)
{
    PARAM_CHECK(mutex != NULL, return -1, "Invalid lock");
    uint32_t ret = LOS_MuxPost(mutex->mutex);
    PARAM_CHECK(ret == LOS_OK, return -1, "Failed to mutex lockret %d %d", ret, mutex->mutex);
    return 0;
}

int ParamMutexDelete(ParamMutex *mutex)
{
    PARAM_CHECK(mutex != NULL, return -1, "Invalid mutex");
    uint32_t ret = LOS_MuxDelete(mutex->mutex);
    PARAM_CHECK(ret == LOS_OK, return -1, "Failed to delete mutex lock ret %d %d", ret, mutex->mutex);
    return 0;
}
#endif

#ifndef STARTUP_INIT_TEST
static int InitLocalSecurityLabel(ParamSecurityLabel *security, int isInit)
{
    UNUSED(isInit);
    PARAM_CHECK(security != NULL, return -1, "Invalid security");
#if defined __LITEOS_A__
    security->cred.pid = getpid();
    security->cred.uid = getuid();
    security->cred.gid = 0;
#else
    security->cred.pid = 0;
    security->cred.uid = 0;
    security->cred.gid = 0;
#endif
    security->flags[PARAM_SECURITY_DAC] |= LABEL_CHECK_IN_ALL_PROCESS;
    return 0;
}

static int FreeLocalSecurityLabel(ParamSecurityLabel *srcLabel)
{
    return 0;
}

static int DacGetParamSecurityLabel(const char *path)
{
    UNUSED(path);
    return 0;
}

static int CheckFilePermission(const ParamSecurityLabel *localLabel, const char *fileName, int flags)
{
    UNUSED(flags);
    PARAM_CHECK(localLabel != NULL && fileName != NULL, return -1, "Invalid param");
    return 0;
}

static int DacCheckParamPermission(const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
#if defined(__LITEOS_A__)
    uid_t uid = getuid();
    return uid <= SYS_UID_INDEX ? DAC_RESULT_PERMISSION : DAC_RESULT_FORBIDED;
#endif
#if defined(__LITEOS_M__)
    return DAC_RESULT_PERMISSION;
#endif
    return DAC_RESULT_PERMISSION;
}

int RegisterSecurityDacOps(ParamSecurityOps *ops, int isInit)
{
    PARAM_CHECK(ops != NULL, return -1, "Invalid param");
    PARAM_LOGV("RegisterSecurityDacOps %d", isInit);
    int ret = strcpy_s(ops->name, sizeof(ops->name), "dac");
    ops->securityGetLabel = NULL;
    ops->securityInitLabel = InitLocalSecurityLabel;
    ops->securityCheckFilePermission = CheckFilePermission;
    ops->securityCheckParamPermission = DacCheckParamPermission;
    ops->securityFreeLabel = FreeLocalSecurityLabel;
    if (isInit) {
        ops->securityGetLabel = DacGetParamSecurityLabel;
    }
    return ret;
}

void LoadGroupUser(void)
{
}
#endif

uint32_t Difftime(time_t curr, time_t base)
{
#ifndef __LITEOS_M__
    return (uint32_t)difftime(curr, base);
#else
    return 0;
#endif
}