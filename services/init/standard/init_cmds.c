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
#include "init_cmds.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/module.h>

#include "bootstage.h"
#include "fs_manager/fs_manager.h"
#include "init_jobs_internal.h"
#include "init_log.h"
#include "init_param.h"
#include "init_service_manager.h"
#include "init_utils.h"
#include "sandbox.h"
#include "sandbox_namespace.h"
#include "securec.h"
#ifdef WITH_SELINUX
#include <policycoreutils.h>
#endif

static const char *g_fscryptPolicyKey = "fscrypt.policy.config";

int GetParamValue(const char *symValue, unsigned int symLen, char *paramValue, unsigned int paramLen)
{
    INIT_CHECK_RETURN_VALUE((symValue != NULL) && (paramValue != NULL) && (paramLen != 0), -1);
    char tmpName[PARAM_NAME_LEN_MAX] = { 0 };
    int ret;
    uint32_t curr = 0;
    char *start = (char *)symValue;
    char *end = (char *)symValue + symLen;
    do {
        char *begin = strchr(start, '$');
        if (begin == NULL || begin >= end) { // not has '$' copy the original string
            ret = strncpy_s(paramValue + curr, paramLen - curr, start, symLen);
            INIT_ERROR_CHECK(ret == EOK, return -1, "Failed to copy start %s", start);
            break;
        } else {
            ret = memcpy_s(paramValue + curr, paramLen - curr, start, begin - start);
            INIT_ERROR_CHECK(ret == 0, return -1, "Failed to copy first value %s", symValue);
            curr += begin - start;
        }
        while (*begin != '{') {
            INIT_CHECK_RETURN_VALUE(*begin != '\0', -1);
            begin++;
        }
        begin++;
        char *left = strchr(begin, '}');
        INIT_CHECK_RETURN_VALUE(left != NULL, -1);

        // copy param name
        ret = strncpy_s(tmpName, PARAM_NAME_LEN_MAX, begin, left - begin);
        INIT_ERROR_CHECK(ret == EOK, return -1, "Invalid param name %s", symValue);
        uint32_t valueLen = paramLen - curr;
        ret = SystemReadParam(tmpName, paramValue + curr, &valueLen);
        INIT_ERROR_CHECK(ret == 0, return -1, "Failed to get param %s", tmpName);
        curr += valueLen;
        left++;
        if ((unsigned int)(left - symValue) >= symLen) {
            break;
        }
        start = left;
    } while (1);
    return 0;
}

static int SyncExecCommand(int argc, char * const *argv)
{
    INIT_CHECK(!(argc == 0 || argv == NULL || argv[0] == NULL), return -1);
    pid_t pid = fork();
    INIT_ERROR_CHECK(!(pid < 0), return -1, "Fork new process to format failed: %d", errno);
    if (pid == 0) {
        INIT_CHECK_ONLY_ELOG(execv(argv[0], argv) == 0, "execv %s failed! err %d.", argv[0], errno);
        exit(-1);
    }
    int status;
    pid_t ret = waitpid(pid, &status, 0);
    if (ret != pid) {
        INIT_LOGE("Failed to wait pid %d, errno %d", pid, errno);
        return ret;
    }
    INIT_CHECK_ONLY_ELOG(!(!WIFEXITED(status) || WEXITSTATUS(status) != 0),
        "Command %s failed with status %d", argv[0], WEXITSTATUS(status));
    return 0;
}

static void DoIfup(const struct CmdArgs *ctx)
{
    struct ifreq interface;
    INIT_ERROR_CHECK(strncpy_s(interface.ifr_name, IFNAMSIZ - 1, ctx->argv[0], strlen(ctx->argv[0])) == EOK,
        return, "DoIfup failed to copy interface name");
    INIT_LOGV("interface name: %s", interface.ifr_name);

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    INIT_ERROR_CHECK(fd >= 0, return, "DoIfup failed to create socket, err = %d", errno);

    if (ioctl(fd, SIOCGIFFLAGS, &interface) >= 0) {
        interface.ifr_flags |= IFF_UP;
        INIT_CHECK_ONLY_ELOG(ioctl(fd, SIOCSIFFLAGS, &interface) >= 0,
            "DoIfup failed to do ioctl with command \"SIOCSIFFLAGS\", err = %d", errno);
    }
    close(fd);
    fd = -1;
}

// format insmod <ko name> [-f] [options]
static void DoInsmod(const struct CmdArgs *ctx)
{
    int index = 0;
    int flags = 0;
    char *fileName = NULL;
    if (ctx->argc > index) {
        fileName = ctx->argv[index];
        index++;
    }
    INIT_ERROR_CHECK(fileName != NULL, return, "Can not find file name from param %s", ctx->argv[0]);
    INIT_LOGV("Install mode %s ", fileName);
    char *realPath = GetRealPath(fileName);
    INIT_ERROR_CHECK(realPath != NULL, return, "Can not get real file name from param %s", ctx->argv[0]);
    if (ctx->argc > 1 && ctx->argv[1] != NULL && strcmp(ctx->argv[1], "-f") == 0) { // [-f]
        flags = MODULE_INIT_IGNORE_VERMAGIC | MODULE_INIT_IGNORE_MODVERSIONS;
        index++;
    }
    char *options = BuildStringFromCmdArg(ctx, index); // [options]
    int fd = open(realPath, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd >= 0) {
        int rc = syscall(__NR_finit_module, fd, options, flags);
        if (rc == -1) {
            INIT_LOGE("Failed to install kernel module for %s failed options %s err: %d", realPath, options, errno);
        }
    }
    if (options != NULL) {
        free(options);
    }
    if (fd >= 0) {
        close(fd);
    }
    free(realPath);
    return;
}

static void DoSetParam(const struct CmdArgs *ctx)
{
    INIT_LOGV("set param name: %s, value %s ", ctx->argv[0], ctx->argv[1]);
    SystemWriteParam(ctx->argv[0], ctx->argv[1]);
}

static void DoLoadPersistParams(const struct CmdArgs *ctx)
{
    INIT_LOGV("LoadPersistParams");
    LoadPersistParams();
    HookMgrExecute(GetBootStageHookMgr(), INIT_POST_PERSIST_PARAM_LOAD, NULL, NULL);
}

static void DoTriggerCmd(const struct CmdArgs *ctx)
{
    INIT_LOGV("DoTrigger :%s", ctx->argv[0]);
    DoTriggerExec(ctx->argv[0]);
}

static void DoLoadDefaultParams(const struct CmdArgs *ctx)
{
    int mode = 0;
    if (ctx->argc > 1 && strcmp(ctx->argv[1], "onlyadd") == 0) {
        mode = LOAD_PARAM_ONLY_ADD;
    }
    INIT_LOGV("DoLoadDefaultParams args : %s %d", ctx->argv[0], mode);
    LoadDefaultParams(ctx->argv[0], mode);
}

static void DoSyncExec(const struct CmdArgs *ctx)
{
    // format: syncexec /xxx/xxx/xxx xxx
    INIT_ERROR_CHECK(ctx != NULL && ctx->argv[0] != NULL, return,
        "DoSyncExec: invalid arguments to exec \"%s\"", ctx->argv[0]);
    int ret = SyncExecCommand(ctx->argc, ctx->argv);
    INIT_LOGI("DoSyncExec end with ret %d", ret);
    return;
}

static void DoExec(const struct CmdArgs *ctx)
{
    // format: exec /xxx/xxx/xxx xxx
    INIT_ERROR_CHECK(ctx != NULL && ctx->argv[0] != NULL, return,
        "DoExec: invalid arguments to exec \"%s\"", ctx->argv[0]);
    pid_t pid = fork();
    INIT_ERROR_CHECK(pid >= 0, return, "DoExec: failed to fork child process to exec \"%s\"", ctx->argv[0]);

    if (pid == 0) {
        OpenHidebug(ctx->argv[0]);
        int ret = execv(ctx->argv[0], ctx->argv);
        INIT_CHECK_ONLY_ELOG(ret != -1, "DoExec: execute \"%s\" failed: %d.", ctx->argv[0], errno);
        _exit(0x7f);
    }
    return;
}

static void DoSymlink(const struct CmdArgs *ctx)
{
    // format: symlink /xxx/xxx/xxx /xxx/xxx/xxx
    int ret = symlink(ctx->argv[0], ctx->argv[1]);
    if (ret != 0 && errno != EEXIST) {
        INIT_LOGE("DoSymlink: link %s to %s failed: %d", ctx->argv[0], ctx->argv[1], errno);
    }
}

static mode_t GetDeviceMode(const char *deviceStr)
{
    switch (*deviceStr) {
        case 'b':
        case 'B':
            return S_IFBLK;
        case 'c':
        case 'C':
            return S_IFCHR;
        case 'f':
        case 'F':
            return S_IFIFO;
        default:
            return -1;
    }
}

static void DoMakeNode(const struct CmdArgs *ctx)
{
    // format: mknod path b 0644 1 9
    const int deviceTypePos = 1;
    const int authorityPos = 2;
    const int majorDevicePos = 3;
    const int minorDevicePos = 4;
    const int decimal = 10;
    const int octal = 8;
    INIT_ERROR_CHECK(access(ctx->argv[1], F_OK), return, "DoMakeNode failed, path has sexisted");
    mode_t deviceMode = GetDeviceMode(ctx->argv[deviceTypePos]);
    errno = 0;
    unsigned int major = strtoul(ctx->argv[majorDevicePos], NULL, decimal);
    INIT_CHECK_ONLY_ELOG(errno != ERANGE, "Failed to strtoul %s", ctx->argv[majorDevicePos]);
    unsigned int minor = strtoul(ctx->argv[minorDevicePos], NULL, decimal);
    INIT_CHECK_ONLY_ELOG(errno != ERANGE, "Failed to strtoul %s", ctx->argv[minorDevicePos]);
    mode_t authority = strtoul(ctx->argv[authorityPos], NULL, octal);
    INIT_CHECK_ONLY_ELOG(errno != ERANGE, "Failed to strtoul %s", ctx->argv[authorityPos]);
    int ret = mknod(ctx->argv[0], deviceMode | authority, makedev(major, minor));
    if (ret != 0) {
        INIT_LOGE("DoMakeNode: path: %s failed: %d", ctx->argv[0], errno);
    }
}

static void DoMakeDevice(const struct CmdArgs *ctx)
{
    // format: makedev major minor
    const int decimal = 10;
    errno = 0;
    unsigned int major = strtoul(ctx->argv[0], NULL, decimal);
    INIT_CHECK_ONLY_ELOG(errno != ERANGE, "Failed to strtoul %s", ctx->argv[0]);
    unsigned int minor = strtoul(ctx->argv[1], NULL, decimal);
    INIT_CHECK_ONLY_ELOG(errno != ERANGE, "Failed to strtoul %s", ctx->argv[1]);
    dev_t deviceId = makedev(major, minor);
    INIT_CHECK_ONLY_ELOG(deviceId >= 0, "DoMakedevice \" major:%s, minor:%s \" failed :%d ", ctx->argv[0],
        ctx->argv[1], errno);
    return;
}

static void DoMountFstabFile(const struct CmdArgs *ctx)
{
    INIT_LOGI("Mount partitions from fstab file \" %s \"", ctx->argv[0]);
    (void)MountAllWithFstabFile(ctx->argv[0], 0);
}

static void DoUmountFstabFile(const struct CmdArgs *ctx)
{
    INIT_LOGI("Umount partitions from fstab file \" %s \"", ctx->argv[0]);
    int rc = UmountAllWithFstabFile(ctx->argv[0]);
    if (rc < 0) {
        INIT_LOGE("Run command umount_fstab failed");
    } else {
        INIT_LOGI("Umount partitions from fstab done");
    }
}

static void DoRestorecon(const struct CmdArgs *ctx)
{
#ifdef WITH_SELINUX
    if (ctx->argc != 1) {
        INIT_LOGE("DoRestorecon invalid arguments.");
        return;
    }

    if (RestoreconRecurse(ctx->argv[0])) {
        INIT_LOGE("DoRestorecon failed for '%s', err %d.", ctx->argv[0], errno);
    }
#endif
    return;
}

static void DoLoadAccessTokenId(const struct CmdArgs *ctx)
{
    LoadAccessTokenId();
}

static int FilterService(const Service *service, const char **exclude, int size)
{
    for (int i = 0; i < size; i++) {
        if (exclude[i] != NULL && strcmp(service->name, exclude[i]) == 0) {
            return 0;
        }
    }
    return 1;
}

static void DoStopAllServices(const struct CmdArgs *ctx)
{
    int flags = SERVICE_ATTR_INVALID;
    if (ctx->argc >= 1 && strcmp(ctx->argv[0], "true") == 0) {
        flags |= SERVICE_ATTR_NEEDWAIT;
        StopAllServices(flags, (const char **)(&ctx->argv[1]), ctx->argc - 1, FilterService);
    } else {
        StopAllServices(flags, (const char **)ctx->argv, ctx->argc, FilterService);
    }
    return;
}

static void DoUmount(const struct CmdArgs *ctx)
{
    INIT_LOGI("DoUmount %s",  ctx->argv[0]);
    int ret = GetMountStatusForMountPoint(ctx->argv[0]);
    if (ret == 0) {
        ret = umount(ctx->argv[0]);
        if ((ret != 0) && (ctx->argc > 1) && (strcmp(ctx->argv[1], "MNT_FORCE") == 0)) {
            ret = umount2(ctx->argv[0], MNT_FORCE);
        }
    }
    INIT_ERROR_CHECK(ret == 0, return, "Failed to umount %s errno = %d.", ctx->argv[0], errno);
}

static void DoSync(const struct CmdArgs *ctx)
{
    sync();
}

static void DoTimerStart(const struct CmdArgs *ctx)
{
    INIT_LOGI("Timer start service with arg = %s", ctx->argv[0]);
    char *arg = ctx->argv[0];
    int count = 0;
    int expectedCount = 2;
    char **splitArgs = SplitStringExt(ctx->argv[0], "|", &count, expectedCount);
    if (splitArgs == NULL) {
        INIT_LOGE("Call timer_start with invalid arguments");
        return;
    }

    if (count != expectedCount) {
        INIT_LOGE("Call timer_start with unexpected arguments %s", arg);
        FreeStringVector(splitArgs, count);
        return;
    }

    Service *service = GetServiceByName(splitArgs[0]);
    if (service == NULL) {
        INIT_LOGE("Cannot find service in timer_start command");
        FreeStringVector(splitArgs, count);
        return;
    }

    errno = 0;
    uint64_t timeout = strtoull(splitArgs[1], NULL, DECIMAL_BASE);
    if (errno != 0) {
        INIT_LOGE("call timer_start with invalid timer");
        FreeStringVector(splitArgs, count);
        return;
    }
    // not need this anymore , release memory.
    FreeStringVector(splitArgs, count);
    ServiceStartTimer(service, timeout);
}

static void DoTimerStop(const struct CmdArgs *ctx)
{
    INIT_LOGI("Stop service timer with arg = %s", ctx->argv[0]);
    const char *serviceName = ctx->argv[0];
    Service *service = GetServiceByName(serviceName);
    if (service == NULL) {
        INIT_LOGE("Cannot find service in timer_stop command");
        return;
    }
    ServiceStopTimer(service);
}

static void DoInitGlobalKey(const struct CmdArgs *ctx)
{
    INIT_LOGI("DoInitGlobalKey: start");
    if (ctx == NULL || ctx->argc != 1) {
        INIT_LOGE("DoInitGlobalKey: para invalid");
        return;
    }
    const char *dataDir = "/data";
    if (strncmp(ctx->argv[0], dataDir, strlen(dataDir)) != 0) {
        INIT_LOGE("DoInitGlobalKey: not data partitation");
        return;
    }
    char * const argv[] = {
        "/system/bin/sdc",
        "filecrypt",
        "init_global_key",
        NULL
    };
    int argc = ARRAY_LENGTH(argv);
    int ret = SyncExecCommand(argc, argv);
    INIT_LOGI("DoInitGlobalKey: end, ret = %d", ret);
}

static void DoInitMainUser(const struct CmdArgs *ctx)
{
    INIT_LOGI("DoInitMainUser: start");
    if (ctx == NULL) {
        INIT_LOGE("DoInitMainUser: para invalid");
        return;
    }
    char * const argv[] = {
        "/system/bin/sdc",
        "filecrypt",
        "init_main_user",
        NULL
    };
    int argc = ARRAY_LENGTH(argv);
    int ret = SyncExecCommand(argc, argv);
    INIT_LOGI("DoInitMainUser: end, ret = %d", ret);
}

int FileCryptEnable(char *fileCryptOption)
{
    INIT_LOGI("FileCryptEnable: start");
    if (fileCryptOption == NULL) {
        INIT_LOGE("FileCryptEnable:option null");
        return -EINVAL;
    }
    int ret = SystemWriteParam(g_fscryptPolicyKey, fileCryptOption);
    if (ret != 0) {
        INIT_LOGE("FileCryptEnable:set fscrypt config failed");
        return ret;
    }
    INIT_LOGI("FileCryptEnable:set fscrypt config success, policy:%s", fileCryptOption);

    return ret;
}

static void DoMkswap(const struct CmdArgs *ctx)
{
    INIT_LOGI("DoMkswap: start");
    if (ctx == NULL) {
        INIT_LOGE("DoMkswap: para invalid");
        return;
    }
    char *const argv[] = {
        "/system/bin/mkswap",
        ctx->argv[0],
        NULL
    };
    int argc = ARRAY_LENGTH(argv);
    int ret = SyncExecCommand(argc, argv);
    INIT_LOGI("DoMkswap: end, ret = %d", ret);
}

static void DoSwapon(const struct CmdArgs *ctx)
{
    INIT_LOGI("DoSwapon: start");
    if (ctx == NULL) {
        INIT_LOGE("DoSwapon: para invalid");
        return;
    }
    char *const argv[] = {
        "/system/bin/swapon",
        ctx->argv[0],
        NULL
    };
    int argc = ARRAY_LENGTH(argv);
    int ret = SyncExecCommand(argc, argv);
    INIT_LOGI("DoSwapon: end, ret = %d", ret);
}

static void DoMkSandbox(const struct CmdArgs *ctx)
{
    INIT_LOGI("DoMkSandbox: start");
    if ((ctx == NULL) || (ctx->argc != 1)) {
        INIT_LOGE("Call DoMkSandbox with invalid arguments");
        return;
    }

    const char *sandbox = ctx->argv[0];
    if (sandbox == NULL) {
        INIT_LOGE("Invalid sandbox name.");
        return;
    }
    InitDefaultNamespace();
    if (!InitSandboxWithName(sandbox)) {
        INIT_LOGE("Failed to init sandbox with name %s.", sandbox);
    }

    if (PrepareSandbox(sandbox) != 0) {
        INIT_LOGE("Failed to prepare sandbox %s.", sandbox);
        DestroySandbox(sandbox);
    }
    if (EnterDefaultNamespace() < 0) {
        INIT_LOGE("Failed to set default namespace.");
    }
    CloseDefaultNamespace();
}

static const struct CmdTable g_cmdTable[] = {
    { "syncexec ", 1, 10, DoSyncExec },
    { "exec ", 1, 10, DoExec },
    { "mknode ", 1, 5, DoMakeNode },
    { "makedev ", 2, 2, DoMakeDevice },
    { "symlink ", 2, 2, DoSymlink },
    { "trigger ", 1, 1, DoTriggerCmd },
    { "insmod ", 1, 10, DoInsmod },
    { "setparam ", 2, 2, DoSetParam },
    { "load_persist_params ", 1, 1, DoLoadPersistParams },
    { "load_param ", 1, 2, DoLoadDefaultParams },
    { "load_access_token_id ", 1, 1, DoLoadAccessTokenId },
    { "ifup ", 1, 1, DoIfup },
    { "mount_fstab ", 1, 1, DoMountFstabFile },
    { "umount_fstab ", 1, 1, DoUmountFstabFile },
    { "restorecon ", 1, 1, DoRestorecon },
    { "stopAllServices ", 0, 10, DoStopAllServices },
    { "umount ", 1, 1, DoUmount },
    { "sync ", 0, 1, DoSync },
    { "timer_start", 1, 1, DoTimerStart },
    { "timer_stop", 1, 1, DoTimerStop },
    { "init_global_key ", 1, 1, DoInitGlobalKey },
    { "init_main_user ", 0, 1, DoInitMainUser },
    { "mkswap", 1, 1, DoMkswap},
    { "swapon", 1, 1, DoSwapon},
    { "mksandbox", 1, 1, DoMkSandbox},
};

const struct CmdTable *GetCmdTable(int *number)
{
    *number = (int)ARRAY_LENGTH(g_cmdTable);
    return g_cmdTable;
}

void OpenHidebug(const char *name)
{
#ifdef SUPPORT_PROFILER_HIDEBUG
#ifdef __aarch64__
    const char *debugSoPath = "/system/lib64/libhidebug.so";
#else
    const char *debugSoPath = "/system/lib/libhidebug.so";
#endif
    do {
        if (access(debugSoPath, F_OK) != 0) {
            break;
        }
        void* handle = dlopen(debugSoPath, RTLD_LAZY);
        if (handle == NULL) {
            INIT_LOGE("Failed to dlopen libhidebug.so, %s\n", dlerror());
            break;
        }
        bool (* initParam)();
        initParam = (bool (*)())dlsym(handle, "InitEnvironmentParam");
        if (initParam == NULL) {
            INIT_LOGE("Failed to dlsym InitEnvironmentParam, %s\n", dlerror());
            dlclose(handle);
            break;
        }
        (*initParam)(name);
        dlclose(handle);
    } while (0);
#endif
}
