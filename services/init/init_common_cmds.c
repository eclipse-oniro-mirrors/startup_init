/*
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd.
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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdio.h>
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

#include "init.h"
#include "init_jobs.h"
#include "init_log.h"
#include "init_service_manager.h"
#include "init_utils.h"
#include "securec.h"

static char *AddOneArg(const char *param, size_t paramLen)
{
    int valueCount = 1;
    char *begin = strchr(param, '$');
    while (begin != NULL) {
        valueCount++;
        begin = strchr(begin + 1, '$');
    }
    size_t allocSize = paramLen + (PARAM_VALUE_LEN_MAX * valueCount) + 1;
    char *arg = calloc(sizeof(char), allocSize);
    INIT_CHECK(arg != NULL, return NULL);
    int ret = GetParamValue(param, paramLen, arg, allocSize);
    INIT_ERROR_CHECK(ret == 0, free(arg);
        return NULL, "Failed to get value for %s", param);
    return arg;
}

char *BuildStringFromCmdArg(const struct CmdArgs *ctx, int startIndex)
{
    char *options = (char *)calloc(1, OPTIONS_SIZE + 1);
    INIT_ERROR_CHECK(options != NULL, return NULL, "Failed to get memory ");
    options[0] = '\0';
    int curr = 0;
    for (int i = startIndex; i < ctx->argc; i++) { // save opt
        if (ctx->argv[i] == NULL) {
            continue;
        }
        int len = snprintf_s(options + curr, OPTIONS_SIZE - curr, OPTIONS_SIZE - 1 - curr, "%s ", ctx->argv[i]);
        if (len <= 0) {
            INIT_LOGE("Failed to format other opt");
            options[0] = '\0';
            return options;
        }
        curr += len;
    }
    if ((curr > 0) && (curr < OPTIONS_SIZE)) {
        options[curr - 1] = '\0';
    }
    return options;
}

const struct CmdArgs *GetCmdArg(const char *cmdContent, const char *delim, int argsCount)
{
    INIT_CHECK_RETURN_VALUE(cmdContent != NULL, NULL);
    if (argsCount > SPACES_CNT_IN_CMD_MAX) {
        INIT_LOGW("Too much arguments for command, max number is %d", SPACES_CNT_IN_CMD_MAX);
        argsCount = SPACES_CNT_IN_CMD_MAX;
    }
    struct CmdArgs *ctx = (struct CmdArgs *)calloc(1, sizeof(struct CmdArgs) + sizeof(char *) * (argsCount + 1));
    INIT_ERROR_CHECK(ctx != NULL, return NULL, "Failed to malloc memory for arg");
    ctx->argc = 0;
    char *p = (char *)cmdContent;
    char *end = (char *)cmdContent + strlen(cmdContent);
    char *token = NULL;
    do {
        // Skip lead whitespaces
        while (isspace(*p)) {
            p++;
        }
        token = strstr(p, delim);
        if (token == NULL) {
            ctx->argv[ctx->argc] = AddOneArg(p, end - p);
            INIT_CHECK(ctx->argv[ctx->argc] != NULL, FreeCmdArg(ctx);
                return NULL);
        } else {
            ctx->argv[ctx->argc] = AddOneArg(p, token - p);
            INIT_CHECK(ctx->argv[ctx->argc] != NULL, FreeCmdArg(ctx);
                return NULL);
        }
        ctx->argc++;
        ctx->argv[ctx->argc] = NULL;
        if (ctx->argc == argsCount) {
            break;
        }
        p = token;
    } while (token != NULL);
    return ctx;
}

void FreeCmdArg(struct CmdArgs *cmd)
{
    INIT_CHECK_ONLY_RETURN(cmd != NULL);
    for (int i = 0; i < cmd->argc; ++i) {
        if (cmd->argv[i] != NULL) {
            free(cmd->argv[i]);
        }
    }
    free(cmd);
    return;
}

static void ExecCmd(const struct CmdTable *cmd, const char *cmdContent)
{
    if (cmd == NULL) {
        INIT_LOGE("Invalid cmd for %s", cmdContent);
        return;
    }

    const struct CmdArgs *ctx = GetCmdArg(cmdContent, " ", cmd->maxArg);
    if (ctx == NULL) {
        INIT_LOGE("Invalid arguments cmd: %s content: %s", cmd->name, cmdContent);
    } else if ((ctx->argc <= cmd->maxArg) && (ctx->argc >= cmd->minArg)) {
        cmd->DoFuncion(ctx, cmdContent);
    } else {
        INIT_LOGE("Invalid arguments cmd: %s content: %s argc: %d %d", cmd->name, cmdContent, ctx->argc, cmd->maxArg);
    }
    FreeCmdArg((struct CmdArgs *)ctx);
}

static void SetProcName(const struct CmdArgs *ctx, const char *procFile)
{
    int fd = open(procFile, O_WRONLY | O_CREAT | O_CLOEXEC | O_TRUNC, S_IRUSR | S_IWUSR);
    INIT_ERROR_CHECK(fd >= 0, return, "Failed to set %s errno: %d", procFile, errno);

    size_t size = strlen(ctx->argv[0]);
    ssize_t n = write(fd, ctx->argv[0], size);
    INIT_ERROR_CHECK(n == (ssize_t)size, close(fd);
        return, "Failed to write domainname errno: %d", errno);
    close(fd);
}

static void DoSetDomainname(const struct CmdArgs *ctx, const char *cmdContent)
{
    UNUSED(cmdContent);
    SetProcName(ctx, "/proc/sys/kernel/domainname");
}

static void DoSetHostname(const struct CmdArgs *ctx, const char *cmdContent)
{
    UNUSED(cmdContent);
    SetProcName(ctx, "/proc/sys/kernel/hostname");
}

static void DoSleep(const struct CmdArgs *ctx, const char *cmdContent)
{
    UNUSED(cmdContent);
    errno = 0;
    unsigned long sleepTime = strtoul(ctx->argv[0], NULL, DECIMAL_BASE);
    if (errno != 0) {
        INIT_LOGE("cannot covert sleep time in command \" sleep \"");
        return;
    }

    // Limit sleep time in 5 seconds
    const unsigned long sleepTimeLimit = 5;
    if (sleepTime > sleepTimeLimit) {
        sleepTime = sleepTimeLimit;
    }
    INIT_LOGI("Sleeping %d second(s)", sleepTime);
    sleep((unsigned int)sleepTime);
}

static void DoStart(const struct CmdArgs *ctx, const char *cmdContent)
{
    UNUSED(ctx);
    INIT_LOGD("DoStart %s", cmdContent);
    StartServiceByName(cmdContent, true);
}

static void DoStop(const struct CmdArgs *ctx, const char *cmdContent)
{
    UNUSED(ctx);
    INIT_LOGD("DoStop %s", cmdContent);
    StopServiceByName(cmdContent);
    return;
}

static void DoReset(const struct CmdArgs *ctx, const char *cmdContent)
{
    UNUSED(ctx);
    INIT_LOGD("DoReset %s", cmdContent);
    DoStop(ctx, cmdContent);
    DoStart(ctx, cmdContent);
}

static void DoCopy(const struct CmdArgs *ctx, const char *cmdContent)
{
    int srcFd = -1;
    int dstFd = -1;
    char buf[MAX_COPY_BUF_SIZE] = { 0 };
    char *realPath1 = NULL;
    char *realPath2 = NULL;
    if (ctx == NULL || ctx->argc != DEFAULT_COPY_ARGS_CNT) {
        INIT_LOGE("DoCopy invalid arguments :%s", cmdContent);
        return;
    }
    do {
        realPath1 = GetRealPath(ctx->argv[0]);
        if (realPath1 == NULL) {
            INIT_LOGE("Failed to get real path %s", ctx->argv[0]);
            break;
        }
        srcFd = open(realPath1, O_RDONLY);
        if (srcFd < 0) {
            INIT_LOGE("Failed to open source path %s  %d", ctx->argv[0], errno);
            break;
        }
        struct stat fileStat = { 0 };
        if (stat(ctx->argv[0], &fileStat) != 0) {
            INIT_LOGE("Failed to state source path %s  %d", ctx->argv[0], errno);
            break;
        }
        mode_t mode = fileStat.st_mode;
        realPath2 = GetRealPath(ctx->argv[1]);
        if (realPath2 != NULL) {
            dstFd = open(realPath2, O_WRONLY | O_TRUNC | O_CREAT, mode);
        } else {
            dstFd = open(ctx->argv[1], O_WRONLY | O_TRUNC | O_CREAT, mode);
        }
        if (srcFd < 0) {
            INIT_LOGE("Failed to open dest path %s  %d", ctx->argv[1], errno);
            break;
        }
        int rdLen = 0;
        while ((rdLen = read(srcFd, buf, sizeof(buf) - 1)) > 0) {
            int rtLen = write(dstFd, buf, rdLen);
            if (rtLen != rdLen) {
                INIT_LOGE("Failed to write to dest path %s  %d", ctx->argv[1], errno);
                break;
            }
        }
        fsync(dstFd);
    } while (0);
    INIT_CHECK(srcFd < 0, close(srcFd));
    INIT_CHECK(dstFd < 0, close(dstFd));
    INIT_CHECK(realPath1 == NULL, free(realPath1));
    INIT_CHECK(realPath2 == NULL, free(realPath2));
}

static int SetOwner(const char *file, const char *ownerStr, const char *groupStr)
{
    INIT_ERROR_CHECK(file != NULL, return -1, "SetOwner invalid file.");
    INIT_ERROR_CHECK(ownerStr != NULL, return -1, "SetOwner invalid file.");
    INIT_ERROR_CHECK(groupStr != NULL, return -1, "SetOwner invalid file.");

    uid_t owner = DecodeUid(ownerStr);
    INIT_ERROR_CHECK(owner != (uid_t)-1, return -1, "SetOwner invalid uid :%s.", ownerStr);
    gid_t group = DecodeUid(groupStr);
    INIT_ERROR_CHECK(group != (gid_t)-1, return -1, "SetOwner invalid gid :%s.", groupStr);
    return (chown(file, owner, group) != 0) ? -1 : 0;
}

static void DoChown(const struct CmdArgs *ctx, const char *cmdContent)
{
    UNUSED(cmdContent);
    // format: chown owner group /xxx/xxx/xxx
    const int pathPos = 2;
    int ret = SetOwner(ctx->argv[pathPos], ctx->argv[0], ctx->argv[1]);
    if (ret != 0) {
        INIT_LOGE("Failed to change owner %s, err %d.", cmdContent, errno);
    }
    return;
}

static void DoMkDir(const struct CmdArgs *ctx, const char *cmdContent)
{
    // mkdir support format:
    // 1.mkdir path
    // 2.mkdir path mode
    // 3.mkdir path mode owner group
    UNUSED(cmdContent);
    const int ownerPos = 2;
    const int groupPos = 3;
    if (ctx->argc != 1 && ctx->argc != (groupPos + 1) && ctx->argc != ownerPos) {
        INIT_LOGE("DoMkDir invalid arguments: %s", cmdContent);
        return;
    }
    mode_t mode = DEFAULT_DIR_MODE;
    if (mkdir(ctx->argv[0], mode) != 0 && errno != EEXIST) {
        INIT_LOGE("DoMkDir, failed for '%s', err %d.", ctx->argv[0], errno);
        return;
    }
    if (ctx->argc <= 1) {
        return;
    }

    mode = strtoul(ctx->argv[1], NULL, OCTAL_TYPE);
    if (chmod(ctx->argv[0], mode) != 0) {
        INIT_LOGE("DoMkDir failed for '%s', err %d.", ctx->argv[0], errno);
    }
    if (ctx->argc <= ownerPos) {
        return;
    }
    int ret = SetOwner(ctx->argv[0], ctx->argv[ownerPos], ctx->argv[groupPos]);
    if (ret != 0) {
        INIT_LOGE("Failed to change owner %s, err %d.", cmdContent, errno);
    }
    return;
}

static void DoChmod(const struct CmdArgs *ctx, const char *cmdContent)
{
    UNUSED(cmdContent);
    // format: chmod xxxx /xxx/xxx/xxx
    mode_t mode = strtoul(ctx->argv[0], NULL, OCTAL_TYPE);
    if (mode == 0) {
        INIT_LOGE("DoChmod, strtoul failed for %s, er %d.", cmdContent, errno);
        return;
    }

    if (chmod(ctx->argv[1], mode) != 0) {
        INIT_LOGE("Failed to change mode \" %s \" mode to %04o, err = %d", ctx->argv[1], mode, errno);
    }
}

static int GetMountFlag(unsigned long *mountflag, const char *targetStr, const char *source)
{
    if (targetStr == NULL || mountflag == NULL) {
        return 0;
    }
    struct {
        char *flagName;
        int value;
    } mountFlagMap[] = {
        { "noatime", MS_NOATIME },
        { "noexec", MS_NOEXEC },
        { "nosuid", MS_NOSUID },
        { "nodev", MS_NODEV },
        { "nodiratime", MS_NODIRATIME },
        { "ro", MS_RDONLY },
        { "rdonly", MS_RDONLY },
        { "rw", 0 },
        { "sync", MS_SYNCHRONOUS },
        { "remount", MS_REMOUNT },
        { "bind", MS_BIND },
        { "rec", MS_REC },
        { "unbindable", MS_UNBINDABLE },
        { "private", MS_PRIVATE },
        { "slave", MS_SLAVE },
        { "shared", MS_SHARED },
        { "defaults", 0 },
    };
    for (unsigned int i = 0; i < ARRAY_LENGTH(mountFlagMap); i++) {
        if (strncmp(targetStr, mountFlagMap[i].flagName, strlen(mountFlagMap[i].flagName)) == 0) {
            *mountflag |= mountFlagMap[i].value;
            return 1;
        }
    }
    if (strncmp(targetStr, "wait", strlen("wait")) == 0) {
        WaitForFile(source, WAIT_MAX_COUNT);
        return 1;
    }
    return 0;
}

static void DoMount(const struct CmdArgs *ctx, const char *cmdContent)
{
    UNUSED(ctx);
    INIT_ERROR_CHECK(ctx->argc <= SPACES_CNT_IN_CMD_MAX, return, "Invalid arg number");
    // format: fileSystemType source target mountFlag1 mountFlag2... data
    int index = 0;
    char *fileSysType = (ctx->argc > index) ? ctx->argv[index] : NULL;
    INIT_ERROR_CHECK(fileSysType != NULL, return, "Failed to get fileSysType %s", cmdContent);
    index++;

    char *source =  (ctx->argc > index) ? ctx->argv[index] : NULL;
    INIT_ERROR_CHECK(source != NULL, return, "Failed to get source %s", cmdContent);
    index++;

    // maybe only has "filesystype source target", 2 spaces
    char *target = (ctx->argc > index) ? ctx->argv[index] : NULL;
    INIT_ERROR_CHECK(target != NULL, return, "Failed to get target %s", cmdContent);
    ++index;

    int ret = 0;
    unsigned long mountflags = 0;
    while (index < ctx->argc) {
        ret = GetMountFlag(&mountflags, ctx->argv[index], source);
        if (ret == 0) {
            break;
        }
        index++;
    }
    if (index >= ctx->argc) {
        ret = mount(source, target, fileSysType, mountflags, NULL);
    } else {
        char *data = BuildStringFromCmdArg(ctx, index);
        INIT_ERROR_CHECK(data != NULL, return, "Failed to get data %s", cmdContent);
        ret = mount(source, target, fileSysType, mountflags, data);
        free(data);
    }
    if (ret != 0) {
        INIT_LOGE("Failed to mount %s, err %d.", cmdContent, errno);
    }
}

static bool CheckValidCfg(const char *path)
{
    static const char *supportCfg[] = {
        "/etc/patch.cfg",
        "/patch/fstab.cfg",
    };
    INIT_ERROR_CHECK(path != NULL, return false, "Invalid path for cfg");
    struct stat fileStat = { 0 };
    if (stat(path, &fileStat) != 0 || fileStat.st_size <= 0 || fileStat.st_size > LOADCFG_MAX_FILE_LEN) {
        return false;
    }
    size_t cfgCnt = ARRAY_LENGTH(supportCfg);
    for (size_t i = 0; i < cfgCnt; ++i) {
        if (strcmp(path, supportCfg[i]) == 0) {
            return true;
        }
    }
    return false;
}

static void DoLoadCfg(const struct CmdArgs *ctx, const char *path)
{
    UNUSED(ctx);
    char buf[LOADCFG_BUF_SIZE] = { 0 };
    size_t maxLoop = 0;
    int len;
    if (!CheckValidCfg(path)) {
        INIT_LOGE("CheckCfg file %s Failed", path);
        return;
    }
    char *realPath = GetRealPath(path);
    INIT_ERROR_CHECK(realPath != NULL, return, "Failed to get realpath %s", path);
    FILE *fp = fopen(realPath, "r");
    if (fp == NULL) {
        INIT_LOGE("Failed to open cfg %s error:%d", path, errno);
        free(realPath);
        return;
    }

    while (fgets(buf, LOADCFG_BUF_SIZE - 1, fp) != NULL && maxLoop < LOADCFG_MAX_LOOP) {
        maxLoop++;
        len = strlen(buf);
        if (len < 1) {
            continue;
        }
        if (buf[len - 1] == '\n') {
            buf[len - 1] = '\0'; // we replace '\n' with '\0'
        }
        const struct CmdTable *cmd = GetCmdByName(buf);
        if (cmd == NULL) {
            INIT_LOGE("Cannot support command: %s", buf);
            continue;
        }
        ExecCmd(cmd, &buf[strlen(cmd->name) + 1]);
    }
    free(realPath);
    (void)fclose(fp);
}

static void DoWrite(const struct CmdArgs *ctx, const char *cmdContent)
{
    UNUSED(cmdContent);
    // format: write path content
    char *realPath = GetRealPath(ctx->argv[0]);
    int fd = -1;
    if (realPath != NULL) {
        fd = open(realPath, O_WRONLY | O_CREAT | O_NOFOLLOW | O_CLOEXEC, S_IRUSR | S_IWUSR);
    } else {
        fd = open(ctx->argv[0], O_WRONLY | O_CREAT | O_NOFOLLOW | O_CLOEXEC, S_IRUSR | S_IWUSR);
    }
    if (fd >= 0) {
        size_t ret = write(fd, ctx->argv[1], strlen(ctx->argv[1]));
        if (ret < 0) {
            INIT_LOGE("DoWrite: write to file %s failed: %d", ctx->argv[0], errno);
        }
    }
    if (realPath != NULL) {
        free(realPath);
    }
    realPath = NULL;
    close(fd);
}

static void DoRmdir(const struct CmdArgs *ctx, const char *cmdContent)
{
    UNUSED(cmdContent);
    // format: rmdir path
    int ret = rmdir(ctx->argv[0]);
    if (ret == -1) {
        INIT_LOGE("DoRmdir: remove %s failed: %d.", ctx->argv[0], errno);
    }
    return;
}

static void DoRebootCmd(const struct CmdArgs *ctx, const char *cmdContent)
{
    UNUSED(ctx);
    ExecReboot(cmdContent);
    return;
}

static void DoSetrlimit(const struct CmdArgs *ctx, const char *cmdContent)
{
    UNUSED(cmdContent);
    static const char *resource[] = {
        "RLIMIT_CPU", "RLIMIT_FSIZE", "RLIMIT_DATA", "RLIMIT_STACK", "RLIMIT_CORE", "RLIMIT_RSS",
        "RLIMIT_NPROC", "RLIMIT_NOFILE", "RLIMIT_MEMLOCK", "RLIMIT_AS", "RLIMIT_LOCKS", "RLIMIT_SIGPENDING",
        "RLIMIT_MSGQUEUE", "RLIMIT_NICE", "RLIMIT_RTPRIO", "RLIMIT_RTTIME", "RLIM_NLIMITS"
    };
    // format: setrlimit resource curValue maxValue
    const int rlimMaxPos = 2;
    struct rlimit limit;
    limit.rlim_cur = (rlim_t)atoi(ctx->argv[1]);
    limit.rlim_max = (rlim_t)atoi(ctx->argv[rlimMaxPos]);
    int rcs = -1;
    for (unsigned int i = 0; i < ARRAY_LENGTH(resource); ++i) {
        if (strcmp(ctx->argv[0], resource[i]) == 0) {
            rcs = (int)i;
        }
    }
    if (rcs == -1) {
        INIT_LOGE("DoSetrlimit failed, resouces :%s not support.", ctx->argv[0]);
        return;
    }
    int ret = setrlimit(rcs, &limit);
    if (ret) {
        INIT_LOGE("DoSetrlimit failed : %d", errno);
    }
}

static void DoRm(const struct CmdArgs *ctx, const char *cmdContent)
{
    UNUSED(cmdContent);
    // format: rm /xxx/xxx/xxx
    int ret = unlink(ctx->argv[0]);
    if (ret == -1) {
        INIT_LOGE("DoRm: unlink %s failed: %d.", ctx->argv[0], errno);
    }
}

static void DoExport(const struct CmdArgs *ctx, const char *cmdContent)
{
    UNUSED(cmdContent);
    // format: export xxx /xxx/xxx/xxx
    int ret = setenv(ctx->argv[0], ctx->argv[1], 1);
    if (ret != 0) {
        INIT_LOGE("DoExport: set %s with %s failed: %d", ctx->argv[0], ctx->argv[1], errno);
    }
}

static const struct CmdTable g_cmdTable[] = {
    { "start ", 0, 1, DoStart },
    { "mkdir ", 1, 4, DoMkDir },
    { "chmod ", 2, 2, DoChmod },
    { "chown ", 3, 3, DoChown },
    { "mount ", 1, 10, DoMount },
    { "export ", 2, 2, DoExport },
    { "loadcfg ", 1, 1, DoLoadCfg },
    { "rm ", 1, 1, DoRm },
    { "rmdir ", 1, 1, DoRmdir },
    { "write ", 2, 2, DoWrite },
    { "stop ", 1, 1, DoStop },
    { "reset ", 1, 1, DoReset },
    { "copy ", 2, 2, DoCopy },
    { "reboot ", 1, 1, DoRebootCmd },
    { "setrlimit ", 3, 3, DoSetrlimit },
    { "sleep ", 1, 1, DoSleep },
    { "hostname ", 1, 1, DoSetHostname },
    { "domainname ", 1, 1, DoSetDomainname }
};

static const struct CmdTable *GetCommCmdTable(int *number)
{
    *number = (int)ARRAY_LENGTH(g_cmdTable);
    return g_cmdTable;
}

const struct CmdTable *GetCmdByName(const char *name)
{
    int cmdCnt = 0;
    const struct CmdTable *commCmds = GetCommCmdTable(&cmdCnt);
    for (int i = 0; i < cmdCnt; ++i) {
        if (strncmp(name, commCmds[i].name, strlen(commCmds[i].name)) == 0) {
            return &commCmds[i];
        }
    }
    int number = 0;
    const struct CmdTable *cmds = GetCmdTable(&number);
    for (int i = 0; i < number; ++i) {
        if (strncmp(name, cmds[i].name, strlen(cmds[i].name)) == 0) {
            return &cmds[i];
        }
    }
    return NULL;
}

const char *GetMatchCmd(const char *cmdStr, int *index)
{
    if (cmdStr == NULL || index == NULL) {
        return NULL;
    }
    int cmdCnt = 0;
    const struct CmdTable *commCmds = GetCommCmdTable(&cmdCnt);
    for (int i = 0; i < cmdCnt; ++i) {
        if (strncmp(cmdStr, commCmds[i].name, strlen(commCmds[i].name)) == 0) {
            *index = i;
            return commCmds[i].name;
        }
    }
    int number = 0;
    const struct CmdTable *cmds = GetCmdTable(&number);
    for (int i = 0; i < number; ++i) {
        if (strncmp(cmdStr, cmds[i].name, strlen(cmds[i].name)) == 0) {
            *index = cmdCnt + i;
            return cmds[i].name;
        }
    }
    return NULL;
}

const char *GetCmdKey(int index)
{
    int cmdCnt = 0;
    const struct CmdTable *commCmds = GetCommCmdTable(&cmdCnt);
    if (index < cmdCnt) {
        return commCmds[index].name;
    }
    int number = 0;
    const struct CmdTable *cmds = GetCmdTable(&number);
    if (index < (cmdCnt + number)) {
        return cmds[index - cmdCnt].name;
    }
    return NULL;
}

int GetCmdLinesFromJson(const cJSON *root, CmdLines **cmdLines)
{
    INIT_CHECK(root != NULL, return -1);
    INIT_CHECK(cmdLines != NULL, return -1);
    *cmdLines = NULL;
    if (!cJSON_IsArray(root)) {
        return -1;
    }
    int cmdCnt = cJSON_GetArraySize(root);
    if (cmdCnt <= 0) {
        return -1;
    }
    *cmdLines = (CmdLines *)calloc(1, sizeof(CmdLines) + sizeof(CmdLine) * cmdCnt);
    if (*cmdLines == NULL) {
        return -1;
    }
    (*cmdLines)->cmdNum = 0;
    for (int i = 0; i < cmdCnt; ++i) {
        cJSON *line = cJSON_GetArrayItem(root, i);
        if (!cJSON_IsString(line)) {
            continue;
        }
        char *tmp = cJSON_GetStringValue(line);
        if (tmp == NULL) {
            continue;
        }
        int index = 0;
        const char *cmd = GetMatchCmd(tmp, &index);
        if (cmd == NULL) {
            INIT_LOGE("Cannot support command: %s", tmp);
            continue;
        }
        int ret = strcpy_s((*cmdLines)->cmds[(*cmdLines)->cmdNum].cmdContent, MAX_CMD_CONTENT_LEN, tmp + strlen(cmd));
        if (ret != EOK) {
            INIT_LOGE("Invalid cmd arg: %s", tmp);
            continue;
        }
        (*cmdLines)->cmds[(*cmdLines)->cmdNum].cmdIndex = index;
        (*cmdLines)->cmdNum++;
    }
    return 0;
}

void DoCmdByName(const char *name, const char *cmdContent)
{
    if (name == NULL || cmdContent == NULL) {
        return;
    }
    const struct CmdTable *cmd = GetCmdByName(name);
    ExecCmd(cmd, cmdContent);
}

void DoCmdByIndex(int index, const char *cmdContent)
{
    if (cmdContent == NULL) {
        return;
    }
    int cmdCnt = 0;
    const struct CmdTable *commCmds = GetCommCmdTable(&cmdCnt);
    if (index < cmdCnt) {
        ExecCmd(&commCmds[index], cmdContent);
        return;
    }
    int number = 0;
    const struct CmdTable *cmds = GetCmdTable(&number);
    if (index < (cmdCnt + number)) {
        ExecCmd(&cmds[index - cmdCnt], cmdContent);
        return;
    }
}
