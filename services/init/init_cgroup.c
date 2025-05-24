/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>

#include "securec.h"
#include "init.h"
#include "init_log.h"
#include "init_utils.h"
#include "init_service.h"

INIT_STATIC int GetCgroupPath(Service *service, char *buffer, uint32_t buffLen)
{
    int ret = snprintf_s(buffer, buffLen, buffLen - 1, "/dev/pids/native/%s/pid_%d/", service->name, service->pid);
    INIT_ERROR_CHECK(ret > 0, return ret, "Failed to snprintf_s in GetCgroupPath, errno: %d", errno);
    INIT_LOGV("Cgroup path %s ", buffer);
    return 0;
}

static int WriteToFile(const char *path, pid_t pid)
{
    INIT_CHECK_RETURN_VALUE(pid != 0, -1);
    char pidName[PATH_MAX] = {0};
    int fd = open(path, O_RDWR | O_APPEND);
    INIT_ERROR_CHECK(fd >= 0, return -1, "Failed to open file errno: %d path: %s", errno, path);
    int ret = 0;
    INIT_LOGV(" WriteToFile pid %d", pid);
    do {
        ret = snprintf_s(pidName, sizeof(pidName), sizeof(pidName) - 1, "%d", pid);
        INIT_ERROR_CHECK(ret > 0, break, "Failed to snprintf_s in WriteToFile, errno: %d", errno);
        ret = write(fd, pidName, strlen(pidName));
        INIT_ERROR_CHECK(ret > 0, break, "Failed to write file errno: %d path: %s %s", errno, path, pidName);
        ret = 0;
    } while (0);

    close(fd);
    return ret;
}

static void KillProcessesByCGroup(const char *path, Service *service)
{
    FILE *file = fopen(path, "r");
    INIT_ERROR_CHECK(file != NULL, return, "Open file fail %s errno: %d", path, errno);
    pid_t pid = 0;
    while (fscanf_s(file, "%d\n", &pid) == 1 && pid > 0) {
        INIT_LOGV(" KillProcessesByCGroup pid %d ", pid);
        if (pid == service->pid) {
            continue;
        }
        INIT_LOGI("Kill service pid %d now ...", pid);
        if (kill(pid, SIGKILL) != 0) {
            INIT_LOGE("Unable to kill process, pid: %d ret: %d", pid, errno);
        }
    }
    (void)fclose(file);
}

static void DoRmdir(const TimerHandle handler, void *context)
{
    UNUSED(handler);
    Service *service = (Service *)context;
    INIT_ERROR_CHECK(service != NULL, return, "Service timer process with invalid service");
    ServiceStopTimer(service);

    char path[PATH_MAX] = {};
    int ret = GetCgroupPath(service, path, sizeof(path));
    free(service->name);
    free(service);
    INIT_ERROR_CHECK(ret == 0, return, "Failed to get real path errno: %d", errno);
    ret = rmdir(path);
    if (ret != 0) {
        INIT_LOGE("Failed to rmdir %s errno: %d", path, errno);
    }
}

static void RmdirTimer(Service *service, uint64_t timeout)
{
    Service *serviceRmdir = (Service *)calloc(1, sizeof(Service));
    INIT_ERROR_CHECK(serviceRmdir != NULL, return, "Failed to malloc for serviceRmdir");
    serviceRmdir->pid = service->pid;
    int strLen = strlen(service->name);
    serviceRmdir->name = (char *)malloc(sizeof(char)*(strLen + 1));
    int ret = strcpy_s(serviceRmdir->name, strLen + 1, service->name);
    if (ret != 0) {
        free(serviceRmdir->name);
        free(serviceRmdir);
        INIT_LOGE("Failed to copy, ret: %d", errno);
        return;
    }
    LE_STATUS status = LE_CreateTimer(LE_GetDefaultLoop(), &serviceRmdir->timer, DoRmdir, (void *)serviceRmdir);
    if (status != LE_SUCCESS) {
        INIT_LOGE("Create service timer for service \' %s Rmdir \' failed, status = %d", serviceRmdir->name, status);
        free(serviceRmdir->name);
        free(serviceRmdir);
        return;
    }
    status = LE_StartTimer(LE_GetDefaultLoop(), serviceRmdir->timer, timeout, 1);
    if (status != LE_SUCCESS) {
        INIT_LOGE("Start service timer for service \' %s Rmdir \' failed, status = %d", serviceRmdir->name, status);
        free(serviceRmdir->name);
        free(serviceRmdir);
    }
}

int ProcessServiceDied(Service *service)
{
    INIT_CHECK_RETURN_VALUE(service != NULL, -1);
    INIT_CHECK_RETURN_VALUE(service->pid != -1, 0);
    INIT_CHECK_RETURN_VALUE(service->isCgroupEnabled, 0);
    char path[PATH_MAX] = {};
    INIT_LOGV("ProcessServiceDied %d to cgroup ", service->pid);
    int ret = GetCgroupPath(service, path, sizeof(path));
    INIT_ERROR_CHECK(ret == 0, return -1, "Failed to get real path errno: %d", errno);
    ret = strcat_s(path, sizeof(path), "cgroup.procs");
    INIT_ERROR_CHECK(ret == 0, return ret, "Failed to strcat_s errno: %d", errno);
    KillProcessesByCGroup(path, service);
    RmdirTimer(service, 200);  // 200ms
    return ret;
}

int ProcessServiceAdd(Service *service)
{
    INIT_CHECK_RETURN_VALUE(service != NULL, -1);
    INIT_CHECK_RETURN_VALUE(service->isCgroupEnabled, 0);
    char path[PATH_MAX] = {};
    INIT_LOGV("ProcessServiceAdd %d to cgroup ", service->pid);
    int ret = GetCgroupPath(service, path, sizeof(path));
    INIT_ERROR_CHECK(ret == 0, return -1, "Failed to get real path errno: %d", errno);
    (void)MakeDirRecursive(path, 0755);  // 0755 default mode
    ret = strcat_s(path, sizeof(path), "cgroup.procs");
    INIT_ERROR_CHECK(ret == 0, return ret, "Failed to strcat_s errno: %d", errno);
    ret = WriteToFile(path, service->pid);
    INIT_ERROR_CHECK(ret == 0, return ret, "write pid to cgroup.procs fail %s", path);
    INIT_LOGV("Add service %d to cgroup %s success", service->pid, path);
    return 0;
}