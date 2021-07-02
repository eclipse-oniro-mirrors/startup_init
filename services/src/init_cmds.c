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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#ifndef OHOS_LITE
#include <linux/module.h>
#endif
#include <sys/syscall.h>
#include "init_jobs.h"
#include "init_log.h"
#include "init_reboot.h"
#include "init_service_manager.h"
#include "init_utils.h"
#include "securec.h"
#ifndef OHOS_LITE
#include "init_param.h"
#endif

#define DEFAULT_DIR_MODE 0755  // mkdir, default mode
#define SPACES_CNT_IN_CMD_MAX 10   // mount, max number of spaces in cmdline
#define SPACES_CNT_IN_CMD_MIN 2    // mount, min number of spaces in cmdline

#define LOADCFG_BUF_SIZE  128  // loadcfg, max buffer for one cmdline
#define LOADCFG_MAX_FILE_LEN 51200  // loadcfg, max file size is 50K
#define LOADCFG_MAX_LOOP 20  // loadcfg, to prevent to be trapped in infite loop
#define OCTAL_TYPE 8  // 8 means octal to decimal
#define MAX_BUFFER 256
#define AUTHORITY_MAX_SIZE 128
#define CONVERT_MICROSEC_TO_SEC(x) ((x) / 1000 / 1000)

static const char *g_supportCfg[] = {
    "/etc/patch.cfg",
    "/patch/fstab.cfg",
};

static const char* g_supportedCmds[] = {
    "start ",
    "mkdir ",
    "chmod ",
    "chown ",
    "mount ",
    "export ",
    "loadcfg ",
    "insmod ",
    "rm ",
    "rmdir ",
    "write ",
    "exec ",
    "mknode ",
    "makedev ",
    "symlink ",
    "stop ",
    "trigger ",
    "reset ",
    "copy ",
    "setparam ",
    "load_persist_params ",
    "load_param ",
    "reboot ",
};

void ParseCmdLine(const char* cmdStr, CmdLine* resCmd)
{
    size_t cmdLineLen = 0;
    if (cmdStr == NULL || resCmd == NULL || (cmdLineLen = strlen(cmdStr)) == 0) {
        return;
    }

    size_t supportCmdCnt = sizeof(g_supportedCmds) / sizeof(g_supportedCmds[0]);
    int foundAndSucceed = 0;
    for (size_t i = 0; i < supportCmdCnt; ++i) {
        size_t curCmdNameLen = strlen(g_supportedCmds[i]);
        if (cmdLineLen > curCmdNameLen && cmdLineLen <= (curCmdNameLen + MAX_CMD_CONTENT_LEN) &&
            strncmp(g_supportedCmds[i], cmdStr, curCmdNameLen) == 0) {
            if (memcpy_s(resCmd->name, MAX_CMD_NAME_LEN, cmdStr, curCmdNameLen) != EOK) {
                break;
            }
            resCmd->name[curCmdNameLen] = '\0';

            const char* cmdContent = cmdStr + curCmdNameLen;
            size_t cmdContentLen = cmdLineLen - curCmdNameLen;
            if (memcpy_s(resCmd->cmdContent, MAX_CMD_CONTENT_LEN, cmdContent, cmdContentLen) != EOK) {
                break;
            }
            resCmd->cmdContent[cmdContentLen] = '\0';
            foundAndSucceed = 1;
            break;
        }
    }

    if (!foundAndSucceed) {
        INIT_LOGE("Cannot parse command: %s\n", cmdStr);
        (void)memset_s(resCmd, sizeof(*resCmd), 0, sizeof(*resCmd));
    }
}

static void DoStart(const char* cmdContent)
{
    INIT_LOGD("DoStart %s \n", cmdContent);
    StartServiceByName(cmdContent);
}

static void DoStop(const char* cmdContent)
{
    INIT_LOGD("DoStop %s \n", cmdContent);
    StopServiceByName(cmdContent);
}

static void DoReset(const char* cmdContent)
{
    INIT_LOGD("DoReset %s \n", cmdContent);
    DoStop(cmdContent);
    DoStart(cmdContent);
}

static void DoCopy(const char* cmdContent)
{
    int srcFd = -1;
    int dstFd = -1;
    int rdLen = 0;
    int rtLen = 0;
    char buf[MAX_COPY_BUF_SIZE] = {0};
    mode_t mode = 0;
    struct stat fileStat = {0};
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");
    if (ctx == NULL || ctx->argv == NULL || ctx->argc != DEFAULT_COPY_ARGS_CNT) {
        INIT_LOGE("DoCopy failed.\n");
        goto out;
    }
    srcFd = open(ctx->argv[0], O_RDONLY);
    INIT_ERROR_CHECK(srcFd >= 0, goto out, "copy open %s fail %d! \n", ctx->argv[0], errno);
    INIT_ERROR_CHECK(stat(ctx->argv[0], &fileStat) == 0, goto out, "stat fail \n");
    mode = fileStat.st_mode;
    dstFd = open(ctx->argv[1], O_WRONLY | O_TRUNC | O_CREAT, mode);
    INIT_ERROR_CHECK(dstFd >= 0, goto out, "copy open %s fail %d! \n", ctx->argv[1], errno);
    while ((rdLen = read(srcFd, buf, sizeof(buf) - 1)) > 0) {
        rtLen = write(dstFd, buf, rdLen);
        INIT_ERROR_CHECK(rtLen == rdLen, goto out, "write %s file fail %d! \n", ctx->argv[1], errno);
    }
    fsync(dstFd);
out:
    FreeCmd(&ctx);
    ctx = NULL;
    close(srcFd);
    srcFd = -1;
    close(dstFd);
    dstFd = -1;
    return;
}

static void DoChown(const char* cmdContent)
{
    // format: chown owner group /xxx/xxx/xxx
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");
    if (ctx == NULL || ctx->argv == NULL || ctx->argc != 3) {
        INIT_LOGE("DoChown failed.\n");
        goto out;
    }

    uid_t owner = (uid_t)-1;
    gid_t group = (gid_t)-1;
    if (isalpha(ctx->argv[0][0])) {
        owner = DecodeUid(ctx->argv[0]);
        INIT_ERROR_CHECK(owner != (uid_t)-1, goto out, "DoChown decode owner failed.\n");
    } else {
        owner = strtoul(ctx->argv[0], NULL, 0);
    }

    if (isalpha(ctx->argv[1][0])) {
        group = DecodeUid(ctx->argv[1]);
        INIT_ERROR_CHECK(group != (gid_t)-1, goto out, "DoChown decode group failed.\n");
    } else {
        group = strtoul(ctx->argv[1], NULL, 0);
    }

    int pathPos = 2;
    if (chown(ctx->argv[pathPos], owner, group) != 0) {
        INIT_LOGE("DoChown, failed for %s, err %d.\n", cmdContent, errno);
    }
out:
    FreeCmd(&ctx);
    return;
}

static void DoMkDir(const char* cmdContent)
{
    // format: mkdir /xxx/xxx/xxx or mkdir /xxx/xxx/xxx mode owner group
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");
    if (ctx == NULL || ctx->argv == NULL || ctx->argc < 1) {
        INIT_LOGE("DoMkDir failed.\n");
        goto out;
    }

    mode_t mode = DEFAULT_DIR_MODE;
    for (size_t i = 0; i < strlen(ctx->argv[0]); ++i) {
        if (ctx->argv[0][i] == '/') {
            ctx->argv[0][i] = '\0';
            if (access(ctx->argv[0], 0) != 0 ) {
                mkdir(ctx->argv[0], mode);
            }
            ctx->argv[0][i]='/';
        }
    }
    if (access(ctx->argv[0], 0) != 0) {
        if (mkdir(ctx->argv[0], mode) != 0 && errno != EEXIST) {
            INIT_LOGE("DoMkDir %s failed, err %d.\n", ctx->argv[0], errno);
            goto out;
        }
    }

    if (ctx->argc > 1) {
        mode = strtoul(ctx->argv[1], NULL, OCTAL_TYPE);
        if (chmod(ctx->argv[0], mode) != 0) {
            INIT_LOGE("DoMkDir failed for %s, err %d.\n", cmdContent, errno);
        }
        int ownerPos = 2;
        int groupPos = 3;
        char chownCmdContent[AUTHORITY_MAX_SIZE] = { 0 };
        if (snprintf_s(chownCmdContent, AUTHORITY_MAX_SIZE, AUTHORITY_MAX_SIZE - 1, "%s %s %s",
            ctx->argv[ownerPos], ctx->argv[groupPos], ctx->argv[0]) == -1) {
            INIT_LOGE("DoMkDir snprintf failed.\n");
            goto out;
        }
        DoChown(chownCmdContent);
    }
out:
    FreeCmd(&ctx);
    return;
}

static void DoChmod(const char* cmdContent)
{
    // format: chmod xxxx /xxx/xxx/xxx
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");
    if (ctx == NULL || ctx->argv == NULL || ctx->argc != 2) {
        INIT_LOGE("DoChmod failed.\n");
        goto out;
    }

    mode_t mode = strtoul(ctx->argv[0], NULL, OCTAL_TYPE);
    if (mode == 0) {
        INIT_LOGE("DoChmod, strtoul failed for %s, er %d.\n", cmdContent, errno);
        goto out;
    }

    if (chmod(ctx->argv[1], mode) != 0) {
        INIT_LOGE("DoChmod, failed for %s, err %d.\n", cmdContent, errno);
    }
out:
    FreeCmd(&ctx);
    return;
}

static char* CopySubStr(const char* srcStr, size_t startPos, size_t endPos)
{
    if (endPos <= startPos) {
        INIT_LOGE("DoMount, invalid params<%zu, %zu> for %s.\n", endPos, startPos, srcStr);
        return NULL;
    }

    size_t mallocLen = endPos - startPos + 1;
    char* retStr = (char*)malloc(mallocLen);
    if (retStr == NULL) {
        INIT_LOGE("DoMount, malloc failed! malloc size %zu, for %s.\n", mallocLen, srcStr);
        return NULL;
    }

    const char* copyStart = srcStr + startPos;
    if (memcpy_s(retStr, mallocLen, copyStart, endPos - startPos) != EOK) {
        INIT_LOGE("DoMount, memcpy_s failed for %s.\n", srcStr);
        free(retStr);
        return NULL;
    }
    retStr[mallocLen - 1] = '\0';

    // for example, source may be none
    if (strncmp(retStr, "none", strlen("none")) == 0) {
        retStr[0] = '\0';
    }
    return retStr;
}

static void WaitForFile(const char *source)
{
    struct stat sourceInfo;
    unsigned int waitTime = 500000;
    int maxCount = 10; // 10 means that sleep 10 times, 500ms at a time
    int count = 0;
    do {
        usleep(waitTime);
        count++;
    } while ((stat(source, &sourceInfo) < 0) && (errno == ENOENT) && (count < maxCount));
    if (count == maxCount) {
        INIT_LOGE("wait for file:%s failed after %d.\n", source, maxCount * CONVERT_MICROSEC_TO_SEC(waitTime));
    }
    return;
}

static int GetMountFlag(unsigned long* mountflags, const char* targetStr, const char *source)
{
    if (targetStr == NULL) {
        return 0;
    }

    if (strncmp(targetStr, "nodev", strlen("nodev")) == 0) {
        (*mountflags) |= MS_NODEV;
    } else if (strncmp(targetStr, "noexec", strlen("noexec")) == 0) {
        (*mountflags) |= MS_NOEXEC;
    } else if (strncmp(targetStr, "nosuid", strlen("nosuid")) == 0) {
        (*mountflags) |= MS_NOSUID;
    } else if (strncmp(targetStr, "rdonly", strlen("rdonly")) == 0) {
        (*mountflags) |= MS_RDONLY;
    } else if (strncmp(targetStr, "noatime", strlen("noatime")) == 0) {
        (*mountflags) |= MS_NOATIME;
    } else if (strncmp(targetStr, "wait", strlen("wait")) == 0) {
        WaitForFile(source);
    } else {
        return 0;
    }
    return 1;
}

static int CountSpaces(const char* cmdContent, size_t* spaceCnt, size_t* spacePosArr, size_t spacePosArrLen)
{
    *spaceCnt = 0;
    size_t strLen = strlen(cmdContent);
    for (size_t i = 0; i < strLen; ++i) {
        if (cmdContent[i] == ' ') {
            ++(*spaceCnt);
            if ((*spaceCnt) > spacePosArrLen) {
                INIT_LOGE("DoMount, too many spaces, bad format for %s.\n", cmdContent);
                return 0;
            }
            spacePosArr[(*spaceCnt) - 1] = i;
        }
    }

    if ((*spaceCnt) < SPACES_CNT_IN_CMD_MIN ||           // spaces count should not less than 2(at least 3 items)
        spacePosArr[0] == 0 ||                           // should not start with space
        spacePosArr[(*spaceCnt) - 1] == strLen - 1) {    // should not end with space
        INIT_LOGE("DoMount, bad format for %s.\n", cmdContent);
        return 0;
    }

    // spaces should not be adjacent
    for (size_t i = 1; i < (*spaceCnt); ++i) {
        if (spacePosArr[i] == spacePosArr[i - 1] + 1) {
            INIT_LOGE("DoMount, bad format for %s.\n", cmdContent);
            return 0;
        }
    }
    return 1;
}

static void DoMount(const char* cmdContent)
{
    size_t spaceCnt = 0;
    size_t spacePosArr[SPACES_CNT_IN_CMD_MAX] = {0};
    if (!CountSpaces(cmdContent, &spaceCnt, spacePosArr, SPACES_CNT_IN_CMD_MAX)) {
        return;
    }

    // format: fileSystemType source target mountFlag1 mountFlag2... data
    unsigned long mountflags = 0;
    size_t strLen = strlen(cmdContent);
    size_t indexOffset = 0;
    char* fileSysType = CopySubStr(cmdContent, 0, spacePosArr[indexOffset]);
    if (fileSysType == NULL) {
        return;
    }

    char* source = CopySubStr(cmdContent, spacePosArr[indexOffset] + 1, spacePosArr[indexOffset + 1]);
    if (source == NULL) {
        free(fileSysType);
        return;
    }
    ++indexOffset;

    // maybe only has "filesystype source target", 2 spaces
    size_t targetEndPos = (indexOffset == spaceCnt - 1) ? strLen : spacePosArr[indexOffset + 1];
    char* target = CopySubStr(cmdContent, spacePosArr[indexOffset] + 1, targetEndPos);
    if (target == NULL) {
        free(fileSysType);
        free(source);
        return;
    }
    ++indexOffset;

    // get mountflags, if fail, the rest part of string will be data
    while (indexOffset < spaceCnt) {
        size_t tmpStrEndPos = (indexOffset == spaceCnt - 1) ? strLen : spacePosArr[indexOffset + 1];
        char* tmpStr = CopySubStr(cmdContent, spacePosArr[indexOffset] + 1, tmpStrEndPos);
        int ret = GetMountFlag(&mountflags, tmpStr, source);
        free(tmpStr);
        tmpStr = NULL;

        // get flag failed, the rest part of string will be data
        if (ret == 0) {
            break;
        }
        ++indexOffset;
    }

    int mountRet;
    if (indexOffset >= spaceCnt) {    // no data
        mountRet = mount(source, target, fileSysType, mountflags, NULL);
    } else {
        const char* dataStr = cmdContent + spacePosArr[indexOffset] + 1;
        mountRet = mount(source, target, fileSysType, mountflags, dataStr);
    }

    if (mountRet != 0) {
        INIT_LOGE("DoMount, failed for %s, err %d.\n", cmdContent, errno);
    }

    free(fileSysType);
    free(source);
    free(target);
}

#ifndef OHOS_LITE
#define OPTIONS_SIZE 128u
static void DoInsmodInternal(const char *fileName, char *secondPtr, char *restPtr, int flags)
{
    char options[OPTIONS_SIZE] = {0};
    if (flags == 0) { //  '-f' option
        if (restPtr != NULL && secondPtr != NULL) { // Reset arugments, combine then all.
            if (snprintf_s(options, sizeof(options), OPTIONS_SIZE -1, "%s %s", secondPtr, restPtr) == -1) {
                return;
            }
        } else if (secondPtr != NULL) {
            if (strncpy_s(options, OPTIONS_SIZE - 1, secondPtr, strlen(secondPtr)) != 0) {
                return;
            }
        }
    } else { // Only restPtr is option
        if (restPtr != NULL) {
            if (strncpy_s(options, OPTIONS_SIZE - 1, restPtr, strlen(restPtr)) != 0) {
                return;
            }
        }
    }
    if (!fileName) {
        return;
    }
    char *realPath = (char *)calloc(MAX_BUFFER, sizeof(char));
    if (realPath == NULL) {
        return;
    }
    realPath = realpath(fileName, realPath);
    int fd = open(realPath, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        INIT_LOGE("failed to open %s: %d\n", realPath, errno);
        free(realPath);
        return;
    }
    int rc = syscall(__NR_finit_module, fd, options, flags);
    if (rc == -1) {
        INIT_LOGE("finit_module for %s failed: %d\n", realPath, errno);
    }
    if (fd >= 0) {
        close(fd);
    }
    free(realPath);
    return;
}

// format insmod <ko name> [-f] [options]
static void DoInsmod(const char *cmdContent)
{
    char *p = NULL;
    char *restPtr = NULL;
    char *fileName = NULL;
    char *line = NULL;
    int flags = 0;

    size_t count = strlen(cmdContent);
    if (count > OPTIONS_SIZE) {
        INIT_LOGE("options too long, maybe lost some of options\n");
    }
    line = (char *)malloc(count + 1);
    if (line == NULL) {
        INIT_LOGE("Allocate memory failed.\n");
        return;
    }

    if (memcpy_s(line, count, cmdContent, count) != EOK) {
        INIT_LOGE("memcpy failed\n");
        free(line);
        return;
    }
    line[count] = '\0';
    do {
        if ((p = strtok_r(line, " ", &restPtr)) == NULL) {
            INIT_LOGE("debug, cannot get filename\n");
            free(line);
            return;
        }
        fileName = p;
        INIT_LOGE("debug, fileName is [%s]\n", fileName);
        if ((p = strtok_r(NULL, " ", &restPtr)) == NULL) {
            break;
        }
        if (!strcmp(p, "-f")) {
            flags = MODULE_INIT_IGNORE_VERMAGIC | MODULE_INIT_IGNORE_MODVERSIONS;
        }
    } while (0);
    DoInsmodInternal(fileName, p, restPtr, flags);
    if (line != NULL) {
        free(line);
    }
    return;
}

static void DoSetParam(const char* cmdContent)
{
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");
    if (ctx == NULL || ctx->argv == NULL || ctx->argc != 2) {
        INIT_LOGE("DoSetParam failed.\n");
        goto out;
    }
    INIT_LOGE("param name: %s, value %s \n", ctx->argv[0], ctx->argv[1]);
    SystemWriteParam(ctx->argv[0], ctx->argv[1]);
out:
    FreeCmd(&ctx);
    return;
}

#endif // OHOS_LITE

static bool CheckValidCfg(const char *path)
{
    size_t cfgCnt = sizeof(g_supportCfg) / sizeof(g_supportCfg[0]);
    struct stat fileStat = {0};

    if (stat(path, &fileStat) != 0 || fileStat.st_size <= 0 || fileStat.st_size > LOADCFG_MAX_FILE_LEN) {
        return false;
    }

    for (size_t i = 0; i < cfgCnt; ++i) {
        if (strcmp(path, g_supportCfg[i]) == 0) {
            return true;
        }
    }

    return false;
}

static void DoLoadCfg(const char *path)
{
    char buf[LOADCFG_BUF_SIZE] = {0};
    FILE *fp = NULL;
    size_t maxLoop = 0;
    CmdLine *cmdLine = NULL;
    int len;

    if (path == NULL) {
        return;
    }

    INIT_LOGI("DoLoadCfg cfg file %s\n", path);
    if (!CheckValidCfg(path)) {
        INIT_LOGE("CheckCfg file %s Failed\n", path);
        return;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        INIT_LOGE("open cfg error = %d\n", errno);
        return;
    }

    cmdLine = (CmdLine *)malloc(sizeof(CmdLine));
    if (cmdLine == NULL) {
        INIT_LOGE("malloc cmdline error");
        fclose(fp);
        return;
    }

    while (fgets(buf, LOADCFG_BUF_SIZE, fp) != NULL && maxLoop < LOADCFG_MAX_LOOP) {
        maxLoop++;
        len = strlen(buf);
        if (len < 1) {
            continue;
        }
        if (buf[len - 1] == '\n') {
            buf[len - 1] = '\0'; // we replace '\n' with '\0'
        }
        (void)memset_s(cmdLine, sizeof(CmdLine), 0, sizeof(CmdLine));
        ParseCmdLine(buf, cmdLine);
        DoCmd(cmdLine);
        (void)memset_s(buf, sizeof(char) * LOADCFG_BUF_SIZE, 0, sizeof(char) * LOADCFG_BUF_SIZE);
    }

    free(cmdLine);
    fclose(fp);
}

static void DoWrite(const char *cmdContent)
{
    // format: write path content
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");
    int writeCmdNumber = 2;
    if (ctx == NULL || ctx->argv == NULL || ctx->argc != writeCmdNumber) {
        INIT_LOGE("DoWrite: invalid arguments\n");
        goto out;
    }

    int fd = open(ctx->argv[0], O_WRONLY | O_CREAT | O_NOFOLLOW | O_CLOEXEC, S_IRWXU |
 S_IRGRP | S_IROTH);
    if (fd == -1) {
        INIT_LOGE("DoWrite: open %s failed: %d\n", ctx->argv[0], errno);
        goto out;
    }

    size_t ret = write(fd, ctx->argv[1], strlen(ctx->argv[1]));
    if (ret < 0) {
        INIT_LOGE("DoWrite: write to file %s failed: %d\n", ctx->argv[0], errno);
        close(fd);
        goto out;
    }
    close(fd);
out:
    FreeCmd(&ctx);
    return;
}

static void DoRmdir(const char *cmdContent)
{
    // format: rmdir path
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");
    if (ctx == NULL || ctx->argv == NULL || ctx->argc != 1) {
        INIT_LOGE("DoRmdir: invalid arguments\n");
        goto out;
    }

    int ret = rmdir(ctx->argv[0]);
    if (ret == -1) {
        INIT_LOGE("DoRmdir: remove %s failed: %d.\n", ctx->argv[0], errno);
        goto out;
    }
out:
    FreeCmd(&ctx);
    return;
}

static void DoRm(const char *cmdContent)
{
    // format: rm /xxx/xxx/xxx
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");
    if (ctx == NULL || ctx->argv == NULL || ctx->argc != 1) {
        INIT_LOGE("DoRm: invalid arguments\n");
        goto out;
    }
    int ret = unlink(ctx->argv[0]);
    if (ret == -1) {
        INIT_LOGE("DoRm: unlink %s failed: %d.\n", ctx->argv[0], errno);
        goto out;
    }
out:
    FreeCmd(&ctx);
    return;
}

static void DoExport(const char *cmdContent)
{
    // format: export xxx /xxx/xxx/xxx
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");
    if (ctx == NULL || ctx->argv == NULL || ctx->argc != 2) {
        INIT_LOGE("DoExport: invalid arguments\n");
        goto out;
    }
    int ret = setenv(ctx->argv[0], ctx->argv[1], 1);
    if (ret != 0) {
        INIT_LOGE("DoExport: set %s with %s failed: %d\n", ctx->argv[0], ctx->argv[1], errno);
        goto out;
    }
out:
    FreeCmd(&ctx);
    return;
}

static void DoExec(const char *cmdContent)
{
    // format: exec /xxx/xxx/xxx xxx
    pid_t pid = fork();
    if (pid < 0) {
        INIT_LOGE("DoExec: failed to fork child process to exec \"%s\"\n", cmdContent);
        return;
    }
    if (pid == 0) {
        struct CmdArgs *ctx = GetCmd(cmdContent, " ");
        if (ctx == NULL || ctx->argv == NULL) {
            INIT_LOGE("DoExec: invalid arguments\n");
            _exit(0x7f);
        }
#ifdef OHOS_LITE
        int ret = execve(ctx->argv[0], ctx->argv, NULL);
#else
        int ret = execv(ctx->argv[0], ctx->argv);
#endif
        if (ret == -1) {
            INIT_LOGE("DoExec: execute \"%s\" failed: %d.\n", cmdContent, errno);
        }
        FreeCmd(&ctx);
        _exit(0x7f);
    }
    return;
}

#ifndef __LITEOS__
static void DoSymlink(const char *cmdContent)
{
    // format: symlink /xxx/xxx/xxx /xxx/xxx/xxx
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");
    int symlinkCmdNumber = 2;
    if (ctx == NULL || ctx->argv == NULL || ctx->argc != symlinkCmdNumber) {
        INIT_LOGE("DoSymlink: invalid arguments.\n");
        goto out;
    }

    int ret = symlink(ctx->argv[0], ctx->argv[1]);
    if (ret != 0) {
        INIT_LOGE("DoSymlink: link %s to %s failed: %d\n", ctx->argv[0], ctx->argv[1], errno);
        goto out;
    }
out:
    FreeCmd(&ctx);
    return;
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

static void DoMakeNode(const char *cmdContent)
{
    // format: mknod path b 0644 1 9
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");
    int mkNodeCmdNumber = 5;
    int deviceTypePos = 1;
    int authorityPos = 2;
    int majorDevicePos = 3;
    int minorDevicePos = 4;
    int decimal = 10;
    int octal = 8;
    if (ctx == NULL || ctx->argv == NULL || ctx->argc != mkNodeCmdNumber) {
        INIT_LOGE("DoMakeNode: invalid arguments\n");
        goto out;
    }

    if (!access(ctx->argv[1], F_OK)) {
        INIT_LOGE("DoMakeNode failed, path has not sexisted\n");
        goto out;
    }
    mode_t deviceMode = GetDeviceMode(ctx->argv[deviceTypePos]);
    unsigned int major = strtoul(ctx->argv[majorDevicePos], NULL, decimal);
    unsigned int minor = strtoul(ctx->argv[minorDevicePos], NULL, decimal);
    mode_t authority = strtoul(ctx->argv[authorityPos], NULL, octal);

    int ret = mknod(ctx->argv[0], deviceMode | authority, makedev(major, minor));
    if (ret != 0) {
        INIT_LOGE("DoMakeNode: path: %s failed: %d\n", ctx->argv[0], errno);
        goto out;
    }
out:
    FreeCmd(&ctx);
    return;
}

static void DoMakeDevice(const char *cmdContent)
{
    // format: makedev major minor
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");
    int makeDevCmdNumber = 2;
    int decimal = 10;
    if (ctx == NULL || ctx->argv == NULL || ctx->argc != makeDevCmdNumber) {
        INIT_LOGE("DoMakedevice: invalid arugments\n");
        goto out;
    }
    unsigned int major = strtoul(ctx->argv[0], NULL, decimal);
    unsigned int minor = strtoul(ctx->argv[1], NULL, decimal);
    dev_t deviceId = makedev(major, minor);
    if (deviceId < 0) {
        INIT_LOGE("DoMakedevice \" %s \" failed :%d \n", cmdContent, errno);
        goto out;
    }
out:
    FreeCmd(&ctx);
    return;
}
#endif // __LITEOS__

void DoCmd(const CmdLine* curCmd)
{
    // null curCmd or empty command, just quit.
    if (curCmd == NULL || curCmd->name[0] == '\0') {
        return;
    }
    // INIT_LOGE("curCmd->name:%s, curCmd->cmdContent:%s\n", curCmd->name, curCmd->cmdContent);
    DoCmdByName(curCmd->name, curCmd->cmdContent);
}

void DoCmdByName(const char *name, const char *cmdContent)
{
    if (name == NULL || cmdContent == NULL) {
        return;
    }
    if (strncmp(name, "start ", strlen("start ")) == 0) {
        DoStart(cmdContent);
    } else if (strncmp(name, "mkdir ", strlen("mkdir ")) == 0) {
        DoMkDir(cmdContent);
    } else if (strncmp(name, "stop ", strlen("stop ")) == 0) {
        DoStop(cmdContent);
    } else if (strncmp(name, "reset ", strlen("reset ")) == 0) {
        DoReset(cmdContent);
    } else if (strncmp(name, "copy ", strlen("copy ")) == 0) {
        DoCopy(cmdContent);
    } else if (strncmp(name, "chmod ", strlen("chmod ")) == 0) {
        DoChmod(cmdContent);
    } else if (strncmp(name, "chown ", strlen("chown ")) == 0) {
        DoChown(cmdContent);
    } else if (strncmp(name, "mount ", strlen("mount ")) == 0) {
        DoMount(cmdContent);
    } else if (strncmp(name, "write ", strlen("write ")) == 0) {
        DoWrite(cmdContent);
    } else if (strncmp(name, "rmdir ", strlen("rmdir ")) == 0) {
        DoRmdir(cmdContent);
    } else if (strncmp(name, "rm ", strlen("rm ")) == 0) {
        DoRm(cmdContent);
    } else if (strncmp(name, "export ", strlen("export ")) == 0) {
        DoExport(cmdContent);
    } else if (strncmp(name, "exec ", strlen("exec ")) == 0) {
        DoExec(cmdContent);
#ifndef __LITEOS__
    } else if (strncmp(name, "symlink ", strlen("symlink ")) == 0) {
        DoSymlink(cmdContent);
    } else if (strncmp(name, "makedev ", strlen("makedev ")) == 0) {
        DoMakeDevice(cmdContent);
    } else if (strncmp(name, "mknode ", strlen("mknode ")) == 0) {
        DoMakeNode(cmdContent);
#endif
    } else if (strncmp(name, "loadcfg ", strlen("loadcfg ")) == 0) {
        DoLoadCfg(cmdContent);
#ifndef OHOS_LITE
    } else if (strncmp(name, "insmod ", strlen("insmod ")) == 0) {
        DoInsmod(cmdContent);
    } else if (strncmp(name, "trigger ", strlen("trigger ")) == 0) {
        INIT_LOGD("ready to trigger job: %s\n", name);
        DoTriggerExec(cmdContent);
    } else if (strncmp(name, "load_persist_params ", strlen("load_persist_params ")) == 0) {
        LoadPersistParams();
    } else if (strncmp(name, "setparam ", strlen("setparam ")) == 0) {
        DoSetParam(cmdContent);
    } else if (strncmp(name, "load_param ", strlen("load_param ")) == 0) {
        LoadDefaultParams(cmdContent);
#endif
    } else if (strncmp(name, "reboot ", strlen("reboot ")) == 0) {
        DoReboot(cmdContent);
    }  else {
        INIT_LOGE("DoCmd, unknown cmd name %s.\n", name);
    }
}

const char *GetMatchCmd(const char *cmdStr)
{
    if (cmdStr == NULL) {
        return NULL;
    }
    size_t supportCmdCnt = sizeof(g_supportedCmds) / sizeof(g_supportedCmds[0]);
    for (size_t i = 0; i < supportCmdCnt; ++i) {
        size_t curCmdNameLen = strlen(g_supportedCmds[i]);
        if (strncmp(g_supportedCmds[i], cmdStr, curCmdNameLen) == 0) {
            return g_supportedCmds[i];
        }
    }
    return NULL;
}
