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
#include "fs_manager/fs_manager.h"
#include "fs_manager/fs_manager_log.h"
#include "init_jobs_internal.h"
#include "init_log.h"
#include "init_param.h"
#include "init_service_manager.h"
#include "init_utils.h"
#include "securec.h"

int GetParamValue(const char *symValue, unsigned int symLen, char *paramValue, unsigned int paramLen)
{
    INIT_CHECK_RETURN_VALUE((symValue != NULL) && (paramValue != NULL) && (paramLen != 0), -1);
    char tmpName[PARAM_NAME_LEN_MAX] = { 0 };
    int ret = 0;
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

static void DoIfup(const struct CmdArgs *ctx)
{
    struct ifreq interface;
    INIT_ERROR_CHECK(strncpy_s(interface.ifr_name, IFNAMSIZ - 1, ctx->argv[0], strlen(ctx->argv[0])) == EOK,
        return, "DoIfup failed to copy interface name");
    INIT_LOGD("interface name: %s", interface.ifr_name);

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
    INIT_LOGD("Install mode %s ", fileName);
    char *realPath = GetRealPath(fileName);
    INIT_ERROR_CHECK(realPath != NULL, return, "Can not get real file name from param %s", ctx->argv[0]);
    INIT_CHECK((ctx->argc > 1 && ctx->argv[1] != NULL && strcmp(ctx->argv[1], "-f")) != 0, // [-f]
        flags = MODULE_INIT_IGNORE_VERMAGIC | MODULE_INIT_IGNORE_MODVERSIONS;
        index++);

    char *options = BuildStringFromCmdArg(ctx, index); // [options]
    int fd = open(realPath, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd >= 0) {
        int rc = syscall(__NR_finit_module, fd, options, flags);
        if (rc == -1) {
            INIT_LOGE("Failed to install mode for %s failed options %s err: %d", realPath, options, errno);
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
    INIT_LOGD("set param name: %s, value %s ", ctx->argv[0], ctx->argv[1]);
    SystemWriteParam(ctx->argv[0], ctx->argv[1]);
}

static void DoLoadPersistParams(const struct CmdArgs *ctx)
{
    INIT_LOGD("load persist params : %s", ctx->argv[0]);
    LoadPersistParams();
}

static void DoTriggerCmd(const struct CmdArgs *ctx)
{
    INIT_LOGD("DoTrigger :%s", ctx->argv[0]);
    DoTriggerExec(ctx->argv[0]);
}

static void DoLoadDefaultParams(const struct CmdArgs *ctx)
{
    int mode = 0;
    if (ctx->argc > 1 && strcmp(ctx->argv[1], "onlyadd") == 0) {
        mode = LOAD_PARAM_ONLY_ADD;
    }
    INIT_LOGD("DoLoadDefaultParams args : %s %d", ctx->argv[0], mode);
    LoadDefaultParams(ctx->argv[0], mode);
}

static void DoExec(const struct CmdArgs *ctx)
{
    // format: exec /xxx/xxx/xxx xxx
    pid_t pid = fork();
    INIT_ERROR_CHECK(pid >= 0, return, "DoExec: failed to fork child process to exec \"%s\"", ctx->argv[0]);

    if (pid == 0) {
        INIT_ERROR_CHECK(ctx != NULL && ctx->argv[0] != NULL, _exit(0x7f),
            "DoExec: invalid arguments to exec \"%s\"", ctx->argv[0]);
        int ret = execv(ctx->argv[0], ctx->argv);
        if (ret == -1) {
            INIT_LOGE("DoExec: execute \"%s\" failed: %d.", ctx->argv[0], errno);
        }
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
    FsManagerLogInit(LOG_TO_KERNEL, "");
    (void)MountAllWithFstabFile(ctx->argv[0], 0);
}

static void DoUmountFstabFile(const struct CmdArgs *ctx)
{
    INIT_LOGI("Umount partitions from fstab file \" %s \"", ctx->argv[0]);
    FsManagerLogInit(LOG_TO_KERNEL, "");
    int rc = UmountAllWithFstabFile(ctx->argv[0]);
    if (rc < 0) {
        INIT_LOGE("Run command umount_fstab failed");
    } else {
        INIT_LOGI("Umount partitions from fstab done");
    }
}

static const struct CmdTable g_cmdTable[] = {
    { "exec ", 1, 10, DoExec },
    { "mknode ", 1, 5, DoMakeNode },
    { "makedev ", 2, 2, DoMakeDevice },
    { "symlink ", 2, 2, DoSymlink },
    { "trigger ", 1, 1, DoTriggerCmd },
    { "insmod ", 1, 10, DoInsmod },
    { "setparam ", 2, 2, DoSetParam },
    { "load_persist_params ", 1, 1, DoLoadPersistParams },
    { "load_param ", 1, 2, DoLoadDefaultParams },
    { "ifup ", 1, 1, DoIfup },
    { "mount_fstab ", 1, 1, DoMountFstabFile },
    { "umount_fstab ", 1, 1, DoUmountFstabFile },
};

const struct CmdTable *GetCmdTable(int *number)
{
    *number = (int)ARRAY_LENGTH(g_cmdTable);
    return g_cmdTable;
}
