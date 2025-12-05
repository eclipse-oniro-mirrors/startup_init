/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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
#include <stdio.h>
#include <dirent.h>
#include <ctype.h>
#include <mntent.h>
#include <linux/module.h>

#include "bootstage.h"
#include "fs_manager/fs_manager.h"
#include "init_cmdexecutor.h"
#include "init_jobs_internal.h"
#include "init_log.h"
#include "init_param.h"
#include "init_service_manager.h"
#include "init_utils.h"
#include "sandbox.h"
#include "sandbox_namespace.h"
#include "securec.h"
#include "fscrypt_utils.h"

#ifdef SUPPORT_PROFILER_HIDEBUG
#include <hidebug_base.h>
#endif

#define FSCRYPT_POLICY_BUF_SIZE (60)
#define DECIMAL 10
#define OCTAL 8
#define BOOT_DETECTOR_IOCTL_BASE 'B'
#define SET_SHUT_STAGE _IOW(BOOT_DETECTOR_IOCTL_BASE, 106, int)
#define SHUT_STAGE_FRAMEWORK_START 1

#define PROC_PATH "/proc"
#define KILL_WAIT_TIME 100000
#define INTERVAL_WAIT 10
#define MAX_MOUNTS 256
#define MOUNTINFO_MAX_SIZE 4096
#define OPEN_FILE_MOD 0700
#define TIME_LEN 64

int GetParamValue(const char *symValue, unsigned int symLen, char *paramValue, unsigned int paramLen)
{
    INIT_CHECK_RETURN_VALUE((symValue != NULL) && (paramValue != NULL) && (paramLen != 0), -1);
    char tmpName[PARAM_NAME_LEN_MAX] = { 0 };
    int ret;
    uint32_t curr = 0;
    char *start = (char *)symValue;
    if (symLen > strlen(symValue)) {
        return -1;
    }
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
        if (paramLen < curr) {
            INIT_LOGE("Failed to get param value over max length");
            return -1;
        }
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
    INIT_LOGI("Sync exec: %s", argv[0]);
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
        return -1;
    }
    INIT_LOGI("Sync exec: %s result %d %d", argv[0], WEXITSTATUS(status), WIFEXITED(status));
    return WEXITSTATUS(status);
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
}

static void DoLoadPrivatePersistParams(const struct CmdArgs *ctx)
{
    INIT_LOGV("LoadPersistParams");
    LoadPrivatePersistParams();
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
    INIT_ERROR_CHECK(ctx != NULL && ctx->argv[0] != NULL, return, "DoSyncExec: invalid arguments");
    SyncExecCommand(ctx->argc, ctx->argv);
    return;
}

static void DoExec(const struct CmdArgs *ctx)
{
    // format: exec /xxx/xxx/xxx xxx
    INIT_ERROR_CHECK(ctx != NULL && ctx->argv[0] != NULL, return, "DoExec: invalid arguments");
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
    INIT_ERROR_CHECK(access(ctx->argv[1], F_OK), return, "DoMakeNode failed, path has sexisted");
    mode_t deviceMode = GetDeviceMode(ctx->argv[deviceTypePos]);
    errno = 0;
    unsigned int major = strtoul(ctx->argv[majorDevicePos], NULL, DECIMAL);
    INIT_CHECK_ONLY_ELOG(errno != ERANGE, "Failed to strtoul %s", ctx->argv[majorDevicePos]);
    unsigned int minor = strtoul(ctx->argv[minorDevicePos], NULL, DECIMAL);
    INIT_CHECK_ONLY_ELOG(errno != ERANGE, "Failed to strtoul %s", ctx->argv[minorDevicePos]);
    mode_t authority = strtoul(ctx->argv[authorityPos], NULL, OCTAL);
    INIT_CHECK_ONLY_ELOG(errno != ERANGE, "Failed to strtoul %s", ctx->argv[authorityPos]);
    int ret = mknod(ctx->argv[0], deviceMode | authority, makedev(major, minor));
    if (ret != 0) {
        INIT_LOGE("DoMakeNode: path: %s failed: %d", ctx->argv[0], errno);
    }
}

static void DoMakeDevice(const struct CmdArgs *ctx)
{
    // format: makedev major minor
    errno = 0;
    unsigned int major = strtoul(ctx->argv[0], NULL, DECIMAL);
    INIT_CHECK_ONLY_ELOG(errno != ERANGE, "Failed to strtoul %s", ctx->argv[0]);
    unsigned int minor = strtoul(ctx->argv[1], NULL, DECIMAL);
    INIT_CHECK_ONLY_ELOG(errno != ERANGE, "Failed to strtoul %s", ctx->argv[1]);
    dev_t deviceId = makedev(major, minor);
    INIT_CHECK_ONLY_ELOG(deviceId >= 0, "DoMakedevice \" major:%s, minor:%s \" failed :%d ", ctx->argv[0],
        ctx->argv[1], errno);
    return;
}

static void DoMountFstabFileSp(const struct CmdArgs *ctx)
{
    INIT_LOGI("Mount partitions from fstab file \" %s \"", ctx->argv[0]);
    INIT_TIMING_STAT cmdTimer;
    (void)clock_gettime(CLOCK_MONOTONIC, &cmdTimer.startTime);
    int ret = MountAllWithFstabFile(ctx->argv[0], 0);
    (void)clock_gettime(CLOCK_MONOTONIC, &cmdTimer.endTime);
    long long diff = InitDiffTime(&cmdTimer);
    INIT_LOGI("Mount partitions from fstab file \" %s \" finish ret %d", ctx->argv[0], ret);
    HookMgrExecute(GetBootStageHookMgr(), INIT_MOUNT_STAGE, NULL, NULL);
    char buffer[PARAM_VALUE_LEN_MAX] = {0};
    ret = sprintf_s(buffer, PARAM_VALUE_LEN_MAX, "%lld", diff);
    if (ret <= 0) {
        INIT_LOGE("Failed to sprintf_s");
        return;
    }
    ret = SystemWriteParam("boot.time.fstab", buffer);
    INIT_ERROR_CHECK(ret == 0, return, "write param boot.time.fstab failed");
}

static void DoMountFstabFile(const struct CmdArgs *ctx)
{
    INIT_LOGI("Mount partitions from fstab file \" %s \"", ctx->argv[0]);
    int ret = MountAllWithFstabFile(ctx->argv[0], 0);
    INIT_LOGI("Mount partitions from fstab file \" %s \" finish ret %d", ctx->argv[0], ret);
    HookMgrExecute(GetBootStageHookMgr(), INIT_MOUNT_STAGE, NULL, NULL);
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
    int ret;
    INIT_CMD_INFO cmdInfo;
    cmdInfo.cmdName = "restorecon";
    cmdInfo.cmdContent = ctx->argv[0];
    cmdInfo.reserved = NULL;
    if (ctx->argc > 1) {
        if (strcmp(ctx->argv[0], "-F") == 0) {
            cmdInfo.cmdContent = ctx->argv[1];
        }
        if (strcmp(ctx->argv[ctx->argc - 1], "--skip-ELX") == 0) {
            cmdInfo.reserved = ctx->argv[ctx->argc - 1];
        }
    }

    HOOK_EXEC_OPTIONS options;
    options.flags = TRAVERSE_STOP_WHEN_ERROR;
    options.preHook = NULL;
    options.postHook = NULL;
    ret = HookMgrExecute(GetBootStageHookMgr(), INIT_RESTORECON, (void*)(&cmdInfo), (void*)(&options));
    if (ret == 0) {
        return;
    }
    if (ctx->argc > 1) {
        if (strcmp(ctx->argv[0], "-F") == 0) {
            PluginExecCmdByName("restoreContentRecurseForce", ctx->argv[1]);
        } else if (strcmp(ctx->argv[ctx->argc - 1], "--skip-ELX") == 0) {
            PluginExecCmdByName("restoreContentRecurseSkipElx", ctx->argv[0]);
        }
    } else {
        PluginExecCmdByName("restoreContentRecurse", ctx->argv[0]);
    }
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
            return 1;
        }
    }
    return 0;
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

static void WaitAfterKillPid(int interval, long long totalWait)
{
    INIT_TIMING_STAT cmdTimer;
    (void)clock_gettime(CLOCK_MONOTONIC, &cmdTimer.startTime);
    long long count = 1;
    while (count > 0) {
        usleep(interval);
        count++;
        (void)clock_gettime(CLOCK_MONOTONIC, &cmdTimer.endTime);
        long long diff = InitDiffTime(&cmdTimer);
        if (diff > totalWait) {
            count = 0;
        }
    }
}

static void DoKillAllPid()
{
    DIR *procDir = opendir(PROC_PATH);
    INIT_ERROR_CHECK(procDir != NULL, return, "failed to open proc dir");
    struct dirent *entry;
    while ((entry = readdir(procDir)) != NULL) {
        if (entry->d_type != DT_DIR) {
            continue;
        }
        const char *dirName = entry->d_name;
        if (dirName[0] == '\0' || !isdigit(dirName[0])) {
            continue;
        }
        size_t len = strlen(dirName);
        bool isNumeric = true;
        for (size_t i = 0; i < len; ++i) {
            if (!isdigit(dirName[i])) {
                isNumeric = false;
                break;
            }
        }

        if (!isNumeric) {
            continue;
        }
        int pid = atoi(dirName);
        if (pid != 1) {
            INIT_LOGI("send sig 9 to %d", pid);
            INIT_ERROR_CHECK(kill(pid, SIGKILL) == 0, continue, "kill pid %d failed! err %d", pid, errno);
        }
    }
    WaitAfterKillPid(INTERVAL_WAIT, KILL_WAIT_TIME);
    closedir(procDir);
}

static void UmountPath(const char *path)
{
    int ret = umount2(path, MNT_FORCE);
    if (ret == 0) {
        INIT_LOGW("umount success: %s", path);
        return;
    }
    INIT_LOGW("umount failed: %s, errno: %d", path, errno);
}

static void DoUmountProc()
{
    struct mntent *mnt;
    char *mountPoint[MAX_MOUNTS];
    int count = 0;

    FILE *fp = setmntent("/proc/mounts", "r");
    INIT_ERROR_CHECK(fp != NULL, return, "failed to open /proc/mounts, errno: %d", errno);

    while ((mnt = getmntent(fp)) != NULL && count < MAX_MOUNTS) {
        if (strcmp(mnt->mnt_dir, "/") == 0 ||
            strcmp(mnt->mnt_dir, "/log") == 0 ||
            strcmp(mnt->mnt_type, "procfs") == 0 ||
            strcmp(mnt->mnt_type, "sysfs") == 0 ||
            strcmp(mnt->mnt_type, "devfs") == 0 ||
            strcmp(mnt->mnt_type, "configfs") == 0 ||
            strcmp(mnt->mnt_type, "overlay") == 0 ||
            strcmp(mnt->mnt_type, "cgroup") == 0 ||
            strcmp(mnt->mnt_type, "tracefs") == 0 ||
            strcmp(mnt->mnt_type, "debugfs") == 0 ||
            strcmp(mnt->mnt_type, "erofs") == 0 ||
            strcmp(mnt->mnt_type, "tmpfs") == 0 ||
            strcmp(mnt->mnt_type, "ext4") == 0) {
            INIT_LOGW("skip system mounts: %s (%s), opts: %s.", mnt->mnt_dir, mnt->mnt_type, mnt->mnt_opts);
            continue;
        }

        mountPoint[count] = strdup(mnt->mnt_dir);
        if (!mountPoint[count]) {
            INIT_LOGE("failed allocate memory for mount indo");
            break;
        }
        count++;
    }
    endmntent(fp);

    for (int i = count - 1; i >= 0; i--) {
        if (mountPoint[i] != NULL) {
            UmountPath(mountPoint[i]);
            free(mountPoint[i]);
        }
    }
}

static void WriteCurtime(int fd)
{
    struct timespec currentTime;
    clock_gettime(CLOCK_REALTIME, &currentTime);
    struct tm *timeStruct = localtime(&currentTime.tv_sec);
    char timestamp[TIME_LEN];
    size_t result = strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeStruct);
    if (result == 0) {
        INIT_LOGE("time set failed");
        timestamp[0] = '\0';
    }

    write(fd, timestamp, strlen(timestamp));
    write(fd, "\n", 1);
}

static void InitUmountFaultLog()
{
    const char *dumpPath = "/log/startup/mntdump.txt";
    int dumpFd = open(dumpPath, O_CREAT | O_WRONLY | O_TRUNC, OPEN_FILE_MOD);
    if (dumpFd == -1) {
        INIT_LOGE("mount dump path creat fail, errno is %d", errno);
        return;
    }
    WriteCurtime(dumpFd);
    const char *mountInfoPath = "/proc/mounts";
    int mountFd = open(mountInfoPath, O_RDONLY);
    if (mountFd == -1) {
        INIT_LOGE("failed to open %s, errno: %d", mountInfoPath, errno);
        close(dumpFd);
        return;
    }

    char buffer[MOUNTINFO_MAX_SIZE];
    ssize_t readBytes;
    int writeBytes;
    while ((readBytes = read(mountFd, buffer, MOUNTINFO_MAX_SIZE)) > 0) {
        ssize_t totalWrite = 0;
        while (totalWrite < readBytes) {
            writeBytes = write(dumpFd, buffer, readBytes);
            if (writeBytes == -1 && errno == EINTR) {
                continue;
            }
            if (writeBytes == -1) {
                INIT_LOGE("failed to write to %s, errno: %d", dumpPath, errno);
                break;
            }
            totalWrite += writeBytes;
        }
    }
    close(dumpFd);
    close(mountFd);
    INIT_LOGI("mount info dump end");
}

static void DumpFdInfo()
{
    char dumpPath[] = "/log/startup/fddump.txt";
    pid_t pid = fork();
    if (pid == 0) {
        int fd = open(dumpPath, O_CREAT | O_WRONLY | O_TRUNC, OPEN_FILE_MOD);
        if (fd == -1) {
            INIT_LOGE("open file failed, err is %d", errno);
            return;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
        execlp("lsof", "lsof", NULL);
    } else if (pid < 0) {
        INIT_LOGE("fork failed, err is %d", errno);
        return;
    }
    int status;
    pid_t ret = waitpid(pid, &status, 0);
    if (ret != pid) {
        INIT_LOGE("failed to wait pid %d, errno is %d", pid, errno);
    }
    INIT_LOGI("dump fd info end");
}

static void DoUmountOtherNsData()
{
    INIT_LOGI("umount ns data");
    int ret = EnterSandbox("system");
    if (ret != 0) {
        INIT_LOGE("Enter system sandbox failed");
    }
    ret = umount("/data");
    if (ret != 0) {
        INIT_LOGE("Umount data in system ns failed");
    }
    ret = EnterSandbox("chipset");
    ret = umount("/data");
    if (ret != 0) {
        INIT_LOGE("Umount data in chipset ns failed");
    }
    INIT_LOGI("umount ns data end");
}

static void DoUmount(const struct CmdArgs *ctx)
{
    INIT_LOGI("DoUmount %s",  ctx->argv[0]);
    MountStatus status = GetMountStatusForMountPoint(ctx->argv[0]);
    if (status == MOUNT_MOUNTED) {
        int ret = umount(ctx->argv[0]);
        if ((ret != 0) && (ctx->argc > 1) && (strcmp(ctx->argv[1], "MNT_FORCE") == 0)) {
            ret = umount2(ctx->argv[0], MNT_FORCE);
        }
        INIT_CHECK_ONLY_ELOG(ret == 0, "Failed to umount %s, errno %d", ctx->argv[0], errno);
        if (ret != 0 && strcmp(ctx->argv[0], "/data") == 0) {
            DoKillAllPid();
            DoUmountProc();
            InitUmountFaultLog();
            DumpFdInfo();
            DoUmountOtherNsData();
        }
    } else if (status == MOUNT_UMOUNTED) {
        INIT_LOGI("%s is already umounted", ctx->argv[0]);
    } else {
        INIT_LOGE("Failed to get %s mount status", ctx->argv[0]);
    }
}

static void DoRemoveDmDevice(const struct CmdArgs *ctx)
{
    INIT_LOGI("DoRemoveDmDevice %s",  ctx->argv[0]);
    int ret = FsManagerDmRemoveDevice(ctx->argv[0]);
    if (ret < 0) {
        INIT_LOGE("Failed to remove dm device %s", ctx->argv[0]);
    } else {
        INIT_LOGI("Successed to remove dm device %s", ctx->argv[0]);
    }
}

static void DoMountOneFstabFile(const struct CmdArgs *ctx)
{
    bool required = true;
    if (ctx->argc > 2 && (strcmp(ctx->argv[2], "0") == 0)) { // 2 : required arg position
        required = false;
    }
    INIT_LOGI("Mount %s from fstab file \" %s \" \" %d \"",  ctx->argv[1], ctx->argv[0], (int)required);
    int ret = MountOneWithFstabFile(ctx->argv[0], ctx->argv[1], required);
    if (ret < 0) {
        INIT_LOGE("Failed to mount dm device %d", ret);
    } else {
        INIT_LOGI("Successed to mount dm device %d", ret);
    }
}

static void DoSync(const struct CmdArgs *ctx)
{
    INIT_LOGI("start sync");
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

static bool InitFscryptPolicy(void)
{
    char policy[FSCRYPT_POLICY_BUF_SIZE];
    if (LoadFscryptPolicy(policy, FSCRYPT_POLICY_BUF_SIZE) == 0) {
#ifndef STARTUP_INIT_TEST
        if (SetFscryptSysparam(policy) == 0) {
            return true;
        }
#endif
    }
    return false;
}

static void DoInitGlobalKey(const struct CmdArgs *ctx)
{
    INIT_LOGV("Do init global key start");
    const char *dataDir = "/data";
    if (strncmp(ctx->argv[0], dataDir, strlen(dataDir)) != 0) {
        INIT_LOGE("Not data partitation");
        return;
    }
    if (!InitFscryptPolicy()) {
        INIT_LOGW("Init fscrypt failed, not enable fscrypt");
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
    if (ret != 0) {
        INIT_LOGE("[startup_failed]Init global key failed %d", INIT_GOLBALY_KEY_FAILED);
    }
    INIT_CMD_INFO cmdInfo;
    cmdInfo.cmdName = "init_global_key";
    cmdInfo.cmdContent = (const char *)&ret;
    cmdInfo.reserved = NULL;
    HookMgrExecute(GetBootStageHookMgr(), INIT_POST_DATA_UNENCRYPT, (void*)(&cmdInfo), NULL);
}

static void DoInitMainUser(const struct CmdArgs *ctx)
{
    char * const argv[] = {
        "/system/bin/sdc",
        "filecrypt",
        "init_main_user",
        NULL
    };
    int argc = ARRAY_LENGTH(argv);
    int ret = SyncExecCommand(argc, argv);
    INIT_CMD_INFO cmdInfo;
    cmdInfo.cmdName = "init_main_user";
    cmdInfo.cmdContent = (const char *)&ret;
    cmdInfo.reserved = NULL;
    HookMgrExecute(GetBootStageHookMgr(), INIT_POST_DATA_UNENCRYPT, (void*)(&cmdInfo), NULL);
}

static void DoMkswap(const struct CmdArgs *ctx)
{
    char *const argv[] = {
        "/system/bin/mkswap",
        ctx->argv[0],
        NULL
    };
    int argc = ARRAY_LENGTH(argv);
    SyncExecCommand(argc, argv);
}

static void DoSwapon(const struct CmdArgs *ctx)
{
    char *const argv[] = {
        "/system/bin/swapon",
        ctx->argv[0],
        NULL
    };
    int argc = ARRAY_LENGTH(argv);
    SyncExecCommand(argc, argv);
}

static void DoMkSandbox(const struct CmdArgs *ctx)
{
    INIT_LOGV("Do make sandbox start");
    const char *sandbox = ctx->argv[0];
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

static void DoStopFeedHighdog(const struct CmdArgs *ctx)
{
    INIT_LOGI("stop feed highdog");
    int fd = open("/dev/bbox", O_WRONLY);
    if (fd < 0) {
        INIT_LOGE("open /dev/bbox failed");
        return;
    }
    int stage = SHUT_STAGE_FRAMEWORK_START;
    int ret = ioctl(fd, SET_SHUT_STAGE, &stage);
    if (ret < 0) {
        INIT_LOGE("set shut stage failed");
    }
    close(fd);
    return;
}

static const struct CmdTable g_cmdTable[] = {
    { "syncexec ", 1, 10, 0, DoSyncExec },
    { "exec ", 1, 10, 0, DoExec },
    { "mknode ", 5, 5, 0, DoMakeNode },
    { "makedev ", 2, 2, 0, DoMakeDevice },
    { "symlink ", 2, 2, 1, DoSymlink },
    { "trigger ", 0, 1, 0, DoTriggerCmd },
    { "insmod ", 1, 10, 1, DoInsmod },
    { "setparam ", 2, 2, 0, DoSetParam },
    { "load_persist_params ", 0, 1, 0, DoLoadPersistParams },
    { "load_private_persist_params ", 0, 1, 0, DoLoadPrivatePersistParams },
    { "load_param ", 1, 2, 0, DoLoadDefaultParams },
    { "load_access_token_id ", 0, 1, 0, DoLoadAccessTokenId },
    { "ifup ", 1, 1, 1, DoIfup },
    { "mount_fstab ", 1, 1, 0, DoMountFstabFile },
    { "umount_fstab ", 1, 1, 0, DoUmountFstabFile },
    { "remove_dm_device", 1, 1, 0, DoRemoveDmDevice },
    { "mount_one_fstab", 2, 3, 0, DoMountOneFstabFile },
    { "restorecon ", 1, 2, 1, DoRestorecon },
    { "stopAllServices ", 0, 10, 0, DoStopAllServices },
    { "umount ", 1, 1, 0, DoUmount },
    { "sync ", 0, 1, 0, DoSync },
    { "timer_start", 1, 1, 0, DoTimerStart },
    { "timer_stop", 1, 1, 0, DoTimerStop },
    { "init_global_key ", 1, 1, 0, DoInitGlobalKey },
    { "init_main_user ", 0, 1, 0, DoInitMainUser },
    { "mkswap", 1, 1, 0, DoMkswap },
    { "swapon", 1, 1, 0, DoSwapon },
    { "mksandbox", 1, 1, 0, DoMkSandbox },
    { "stop_feed_highdog", 0, 1, 0, DoStopFeedHighdog },
    { "mount_fstab_sp ", 1, 1, 0, DoMountFstabFileSp },
};

const struct CmdTable *GetCmdTable(int *number)
{
    *number = (int)ARRAY_LENGTH(g_cmdTable);
    return g_cmdTable;
}

void OpenHidebug(const char *name)
{
#ifdef SUPPORT_PROFILER_HIDEBUG
    InitEnvironmentParam(name);
#endif
}

int SetFileCryptPolicy(const char *dir)
{
    if (dir == NULL) {
        INIT_LOGE("SetFileCryptPolicy:dir is null");
        return -EINVAL;
    }
#ifndef STARTUP_INIT_TEST
        return FscryptPolicyEnable(dir);
#endif
    return 0;
}
