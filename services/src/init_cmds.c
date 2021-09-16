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
#include <sys/resource.h>
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
#ifndef OHOS_LITE
#include "init_param.h"
#endif
#include "init_reboot.h"
#include "init_service_manager.h"
#include "init_utils.h"
#include "securec.h"

#define DEFAULT_DIR_MODE 0755  // mkdir, default mode
#define SPACES_CNT_IN_CMD_MAX 10   // mount, max number of spaces in cmdline
#define SPACES_CNT_IN_CMD_MIN 2    // mount, min number of spaces in cmdline

#define LOADCFG_BUF_SIZE  128  // loadcfg, max buffer for one cmdline
#define LOADCFG_MAX_FILE_LEN 51200  // loadcfg, max file size is 50K
#define LOADCFG_MAX_LOOP 20  // loadcfg, to prevent to be trapped in infite loop
#define OCTAL_TYPE 8  // 8 means octal to decimal
#define MAX_BUFFER 256
#define AUTHORITY_MAX_SIZE 128
#define WAIT_MAX_COUNT 10
#define MAX_EACH_CMD_LENGTH 30

static const char *g_supportCfg[] = {
    "/etc/patch.cfg",
    "/patch/fstab.cfg",
};

#ifndef OHOS_LITE
int GetParamValue(char *symValue, char *paramValue, unsigned int paramLen)
{
    if ((symValue == NULL) || (paramValue == NULL) || (paramLen == 0)) {
        return -1;
    }
    char tmpName[MAX_PARAM_NAME_LEN] = {0};
    char tmpValue[MAX_PARAM_VALUE_LEN] = {0};
    unsigned int tmpLen = 0;
    char *p = NULL;
    char *tmpptr = NULL;

    p = strchr(symValue, '$');
    if (p == NULL) { // not has '$' copy the original string
        INIT_CHECK_ONLY_RETURN(strncpy_s(paramValue, paramLen, symValue,
        paramLen - 1) == EOK, return -1);
        return 0;
    }
    tmpLen = p - symValue;
    if (tmpLen > 0) { // copy '$' front string
        INIT_CHECK_ONLY_RETURN(strncpy_s(paramValue, paramLen, symValue, tmpLen) == EOK, return -1);
    }
    p++;
    if (*p == '{') {
        p++;
        char *right = strchr(p, '}');
        if (right == NULL) {
            INIT_LOGE("Invalid cfg file name, miss '}'.");
            return -1;
        }
        tmpLen = right - p;
        if (tmpLen > MAX_PARAM_NAME_LEN) {
            INIT_LOGE("Parameter name longer than %d", MAX_PARAM_NAME_LEN);
            return -1;
        }
        INIT_CHECK_ONLY_RETURN(strncpy_s(tmpName, MAX_PARAM_NAME_LEN, p, tmpLen) == EOK, return -1);
        int ret = SystemReadParam(tmpName, tmpValue, &tmpLen); // get param
        if (ret != 0) {
            INIT_LOGE("Failed to read parameter \" %s \"", tmpName);
            return -1;
        }
        // change param to new string
        INIT_CHECK_ONLY_RETURN(strncat_s(paramValue, paramLen, tmpValue, MAX_PARAM_VALUE_LEN) == EOK, return -1);
        tmpptr = right + 1;
        tmpLen = paramLen - (tmpptr - symValue);
        if (*tmpptr != '\0') { // copy last string
            INIT_CHECK_ONLY_RETURN(strncat_s(paramValue, paramLen, tmpptr, tmpLen) == EOK, return -1);
        }
        return 0;
    } else {
        INIT_LOGE("Invalid cfg file name, miss '{'.");
        return -1;
    }
}
#endif

struct CmdArgs* GetCmd(const char *cmdContent, const char *delim)
{
    struct CmdArgs *ctx = (struct CmdArgs *)malloc(sizeof(struct CmdArgs));
    INIT_CHECK_ONLY_RETURN(ctx != NULL, return NULL);

    ctx->argv = (char**)malloc(sizeof(char *) * MAX_CMD_NAME_LEN);
    INIT_CHECK_ONLY_RETURN(ctx->argv != NULL, FreeCmd(&ctx);
        return NULL);

    char tmpCmd[MAX_BUFFER];
    INIT_CHECK_ONLY_RETURN(strncpy_s(tmpCmd, strlen(cmdContent) + 1, cmdContent, strlen(cmdContent)) == EOK,
        FreeCmd(&ctx);
        return NULL);
    tmpCmd[strlen(cmdContent)] = '\0';

    char *buffer = NULL;
    char *token = strtok_r(tmpCmd, delim, &buffer);
    ctx->argc = 0;
    while (token != NULL) {
#ifndef OHOS_LITE
        ctx->argv[ctx->argc] = calloc(sizeof(char), MAX_EACH_CMD_LENGTH + MAX_PARAM_VALUE_LEN);
        INIT_CHECK_ONLY_RETURN(ctx->argv[ctx->argc] != NULL, FreeCmd(&ctx);
            return NULL);
        INIT_CHECK_ONLY_RETURN(GetParamValue(token, ctx->argv[ctx->argc],
            MAX_EACH_CMD_LENGTH + MAX_PARAM_VALUE_LEN) == 0, FreeCmd(&ctx);
            return NULL);
#else
        ctx->argv[ctx->argc] = calloc(sizeof(char), MAX_EACH_CMD_LENGTH);
        INIT_CHECK_ONLY_RETURN(ctx->argv[ctx->argc] != NULL, FreeCmd(&ctx);
            return NULL);
        INIT_CHECK_ONLY_RETURN(strncpy_s(ctx->argv[ctx->argc], strlen(cmdContent) + 1, token, strlen(token)) == EOK,
            FreeCmd(&ctx);
            return NULL);
#endif
        if (ctx->argc > MAX_CMD_NAME_LEN - 1) {
            INIT_LOGE("GetCmd failed, max cmd number is 10.");
            FreeCmd(&ctx);
            return NULL;
        }
        token = strtok_r(NULL, delim, &buffer);
        ctx->argc += 1;
    }
    ctx->argv[ctx->argc] = NULL;
    return ctx;
}

void FreeCmd(struct CmdArgs **cmd)
{
    struct CmdArgs *tmpCmd = *cmd;
    INIT_CHECK_ONLY_RETURN(tmpCmd != NULL, return);
    for (int i = 0; i < tmpCmd->argc; ++i) {
        INIT_CHECK_ONLY_RETURN(tmpCmd->argv[i] == NULL, free(tmpCmd->argv[i]));
    }
    INIT_CHECK_ONLY_RETURN(tmpCmd->argv == NULL, free(tmpCmd->argv));
    free(tmpCmd);
    return;
}

static void DoStart(const char* cmdContent)
{
    INIT_LOGD("DoStart %s ", cmdContent);
    StartServiceByName(cmdContent);
}

static void DoStop(const char* cmdContent)
{
    INIT_LOGD("DoStop %s ", cmdContent);
    StopServiceByName(cmdContent);
}

static void DoReset(const char* cmdContent)
{
    INIT_LOGD("DoReset %s ", cmdContent);
    DoStop(cmdContent);
    DoStart(cmdContent);
}

static bool IsArgsExpected(const struct CmdArgs *ctx, int expectedArgCount)
{
    if (ctx == NULL || ctx->argv == NULL || ctx->argc != expectedArgCount) {
        return false;
    } else {
        return true;
    }
}

static void DoCopyInernal(const char *source, const char *target)
{
    bool isSuccess = true;
    if (source == NULL || target == NULL) {
        INIT_LOGE("Copy file with invalid arguments");
        return;
    }

    struct stat st = {0};
    int srcFd = open(source, O_RDONLY);
    if (srcFd < 0) {
        INIT_LOGE("Open \" %s \" failed, err = %d", source, errno);
        close(srcFd);
        srcFd = -1;
        return;
    }

    if (fstat(srcFd, &st) < 0) {
        INIT_LOGE("Failed to get file \" %s \" stat", source);
        close(srcFd);
        srcFd = -1;
        return;
    }
    int dstFd = open(target, O_WRONLY | O_TRUNC | O_CREAT, st.st_mode);
    if (dstFd >= 0) {
        char buf[MAX_COPY_BUF_SIZE] = {0};
        ssize_t readn = -1;
        ssize_t writen = -1;
        while ((readn = read(srcFd, buf, MAX_COPY_BUF_SIZE - 1)) > 0) {
            writen = write(dstFd, buf, (size_t)readn);
            if (writen != readn)  {
                isSuccess = false;
                break;
            }
        }
	if (!isSuccess) {
            INIT_LOGE("Copy from \" %s \" to \" %s \" failed", source, target);
        }
        fsync(dstFd);
    }
    if (srcFd >= 0) {
        close(srcFd);
        srcFd = -1;
    }
    if (dstFd >= 0) {
        close(dstFd);
        dstFd = -1;
    }
}

static void DoCopy(const char* cmdContent)
{
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");

    if (!IsArgsExpected(ctx, DEFAULT_COPY_ARGS_CNT)) {
        FreeCmd(&ctx);
        return;
    }
    char sourceFile[PATH_MAX] = {0};
    char targetFile[PATH_MAX] = {0};
    if (realpath(ctx->argv[0], sourceFile) == NULL) {
        if (errno != ENOENT) {
            return;
        }
    }
    if (realpath(ctx->argv[1], targetFile) == NULL) {
        if (errno != ENOENT) {
            return;
        }
    }
    DoCopyInernal(sourceFile, targetFile);
    FreeCmd(&ctx);
    ctx = NULL;
    return;
}

static void DoChown(const char* cmdContent)
{
    // format: chown owner group /xxx/xxx/xxx
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");

    // Max arguments of chown is 3.
    if (!IsArgsExpected(ctx, 3)) {
        FreeCmd(&ctx);
        return;
    }

    uid_t owner = (uid_t)-1;
    gid_t group = (gid_t)-1;
    if (isalpha(ctx->argv[0][0])) {
        owner = DecodeUid(ctx->argv[0]);
        INIT_ERROR_CHECK_AND_RETURN(owner != (uid_t)-1, FreeCmd(&ctx), "DoChown decode owner failed.");
    } else {
        owner = strtoul(ctx->argv[0], NULL, 0);
    }

    if (isalpha(ctx->argv[1][0])) {
        group = DecodeUid(ctx->argv[1]);
        INIT_ERROR_CHECK_AND_RETURN(group != (gid_t)-1, FreeCmd(&ctx), "DoChown decode group failed.");
    } else {
        group = strtoul(ctx->argv[1], NULL, 0);
    }

    int pathPos = 2;
    if (chown(ctx->argv[pathPos], owner, group) != 0) {
        INIT_LOGE("DoChown, failed for %s, err %d.", cmdContent, errno);
    }

    FreeCmd(&ctx);
    return;
}

static void DoMkDir(const char* cmdContent)
{
    // format: mkdir /xxx/xxx/xxx or mkdir /xxx/xxx/xxx mode owner group
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");

    // At least 1 argument of mkdir
    if (!IsArgsExpected(ctx, 1)) {
        FreeCmd(&ctx);
        return;
    }

    mode_t mode = DEFAULT_DIR_MODE;
    if (mkdir(ctx->argv[0], mode) != 0 && errno != EEXIST) {
        INIT_LOGE("DoMkDir, failed for %s, err %d.", cmdContent, errno);
        FreeCmd(&ctx);
        return;
    }

    if (ctx->argc > 1) {
        mode = strtoul(ctx->argv[1], NULL, OCTAL_TYPE);
        if (chmod(ctx->argv[0], mode) != 0) {
            INIT_LOGE("DoMkDir failed for %s, err %d.", cmdContent, errno);
        }
        int ownerPos = 2;
        int groupPos = 3;
        char chownCmdContent[AUTHORITY_MAX_SIZE] = { 0 };
        if (snprintf_s(chownCmdContent, AUTHORITY_MAX_SIZE, AUTHORITY_MAX_SIZE - 1, "%s %s %s",
            ctx->argv[ownerPos], ctx->argv[groupPos], ctx->argv[0]) > 0) {
            DoChown(chownCmdContent);
        }
    }
    FreeCmd(&ctx);
    return;
}

static void DoChmod(const char* cmdContent)
{
    // format: chmod xxxx /xxx/xxx/xxx
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");

    // chmod expect 2 arugments
    if (!IsArgsExpected(ctx, 2)) {
        FreeCmd(&ctx);
        return;
    }

    mode_t mode = strtoul(ctx->argv[0], NULL, OCTAL_TYPE);
    if (mode != 0) {
        if (chmod(ctx->argv[1], mode) != 0) {
            INIT_LOGE("DoChmod, failed for %s, err %d.", cmdContent, errno);
        }
    } else {
        INIT_LOGE("Invalid mode for command chmod");
    }
    FreeCmd(&ctx);
    return;
}

static char* CopySubStr(const char* srcStr, size_t startPos, size_t endPos)
{
    if (endPos <= startPos) {
        INIT_LOGE("DoMount, invalid params<%zu, %zu> for %s.", endPos, startPos, srcStr);
        return NULL;
    }

    size_t mallocLen = endPos - startPos + 1;
    char* retStr = (char*)malloc(mallocLen);
    if (retStr == NULL) {
        INIT_LOGE("DoMount, malloc failed! malloc size %zu, for %s.", mallocLen, srcStr);
        return NULL;
    }

    const char* copyStart = srcStr + startPos;
    if (memcpy_s(retStr, mallocLen, copyStart, endPos - startPos) != EOK) {
        INIT_LOGE("DoMount, memcpy_s failed for %s.", srcStr);
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
        WaitForFile(source, WAIT_MAX_COUNT);
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
                INIT_LOGE("DoMount, too many spaces, bad format for %s.", cmdContent);
                return 0;
            }
            spacePosArr[(*spaceCnt) - 1] = i;
        }
    }

    if ((*spaceCnt) < SPACES_CNT_IN_CMD_MIN ||           // spaces count should not less than 2(at least 3 items)
        spacePosArr[0] == 0 ||                           // should not start with space
        spacePosArr[(*spaceCnt) - 1] == strLen - 1) {    // should not end with space
        INIT_LOGE("DoMount, bad format for %s.", cmdContent);
        return 0;
    }

    // spaces should not be adjacent
    for (size_t i = 1; i < (*spaceCnt); ++i) {
        if (spacePosArr[i] == spacePosArr[i - 1] + 1) {
            INIT_LOGE("DoMount, bad format for %s.", cmdContent);
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
        INIT_LOGE("DoMount, failed for %s, err %d.", cmdContent, errno);
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
    char *realPath = realpath(fileName, NULL);
    if (realPath == NULL) {
        return;
    }
    int fd = open(realPath, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        INIT_LOGE("failed to open %s: %d", realPath, errno);
        free(realPath);
        realPath = NULL;
        return;
    }
    int rc = syscall(__NR_finit_module, fd, options, flags);
    if (rc == -1) {
        INIT_LOGE("finit_module for %s failed: %d", realPath, errno);
    }
    if (fd >= 0) {
        close(fd);
    }
    free(realPath);
    realPath = NULL;
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
        INIT_LOGE("DoInsmod options too long, maybe lost some of options");
    }
    line = (char *)malloc(count + 1);
    if (line == NULL) {
        INIT_LOGE("DoInsmod allocate memory failed.");
        return;
    }

    if (memcpy_s(line, count, cmdContent, count) != EOK) {
        INIT_LOGE("DoInsmod memcpy failed");
        free(line);
        return;
    }
    line[count] = '\0';
    do {
        if ((p = strtok_r(line, " ", &restPtr)) == NULL) {
            INIT_LOGE("DoInsmod cannot get filename.");
            free(line);
            return;
        }
        fileName = p;
        INIT_LOGI("DoInsmod fileName is [%s].", fileName);
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
    const int maxArgCount = 2;
    if (ctx == NULL || ctx->argv == NULL || ctx->argc != maxArgCount) {
        INIT_LOGE("DoSetParam failed.");
        FreeCmd(&ctx);
        return;
    }
    INIT_LOGE("param name: %s, value %s ", ctx->argv[0], ctx->argv[1]);
    SystemWriteParam(ctx->argv[0], ctx->argv[1]);
    FreeCmd(&ctx);
    return;
}

#endif // OHOS_LITE

static bool CheckValidCfg(const char *path)
{
    size_t cfgCnt = ARRAY_LENGTH(g_supportCfg);
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

    INIT_LOGI("DoLoadCfg cfg file %s", path);
    if (!CheckValidCfg(path)) {
        INIT_LOGE("CheckCfg file %s Failed", path);
        return;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        INIT_LOGE("open cfg error = %d", errno);
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
        INIT_LOGE("DoWrite: invalid arguments");
        FreeCmd(&ctx);
        return;
    }

    int fd = open(ctx->argv[0], O_WRONLY | O_CREAT | O_NOFOLLOW | O_CLOEXEC, S_IRUSR |
    S_IWUSR);
    if (fd != -1) {
        size_t ret = write(fd, ctx->argv[1], strlen(ctx->argv[1]));
        if (ret < 0) {
            INIT_LOGE("DoWrite: write to file %s failed: %d", ctx->argv[0], errno);
        }
        close(fd);
        fd = -1;
    } else {
        INIT_LOGE("Open \" %s \" failed, err = %d", ctx->argv[0], errno);
    }
    FreeCmd(&ctx);
    return;
}

static void DoRmdir(const char *cmdContent)
{
    // format: rmdir path
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");
    if (ctx == NULL || ctx->argv == NULL || ctx->argc != 1) {
        INIT_LOGE("DoRmdir: invalid arguments");
        FreeCmd(&ctx);
        return;
    }

    int ret = rmdir(ctx->argv[0]);
    if (ret == -1) {
        INIT_LOGE("DoRmdir: remove %s failed: %d.", ctx->argv[0], errno);
    }
    FreeCmd(&ctx);
    return;
}

static void DoSetrlimit(const char *cmdContent)
{
    char *resource[] = {
        "RLIMIT_CPU", "RLIMIT_FSIZE", "RLIMIT_DATA", "RLIMIT_STACK", "RLIMIT_CORE", "RLIMIT_RSS",
        "RLIMIT_NPROC", "RLIMIT_NOFILE", "RLIMIT_MEMLOCK", "RLIMIT_AS", "RLIMIT_LOCKS", "RLIMIT_SIGPENDING",
        "RLIMIT_MSGQUEUE", "RLIMIT_NICE", "RLIMIT_RTPRIO", "RLIMIT_RTTIME", "RLIM_NLIMITS"
    };
    // format: setrlimit resource curValue maxValue
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");
    int setrlimitCmdNumber = 3;
    int rlimMaxPos = 2;
    if (ctx == NULL || ctx->argv == NULL || ctx->argc != setrlimitCmdNumber) {
        INIT_LOGE("DoSetrlimit: invalid arguments");
        FreeCmd(&ctx);
        return;
    }

    struct rlimit limit;
    limit.rlim_cur = atoi(ctx->argv[1]);
    limit.rlim_max = atoi(ctx->argv[rlimMaxPos]);
    int rcs = -1;
    for (unsigned int i = 0; i < ARRAY_LENGTH(resource); ++i) {
        if (strcmp(ctx->argv[0], resource[i]) == 0) {
            rcs = (int)i;
        }
    }
    if (rcs == -1) {
        INIT_LOGE("DoSetrlimit failed, resouces :%s not support.", ctx->argv[0]);
    } else {
        int ret = setrlimit(rcs, &limit);
        if (ret) {
            INIT_LOGE("DoSetrlimit failed : %d", errno);
        }
    }
    FreeCmd(&ctx);
    return;
}

static void DoRm(const char *cmdContent)
{
    // format: rm /xxx/xxx/xxx
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");
    if (ctx == NULL || ctx->argv == NULL || ctx->argc != 1) {
        INIT_LOGE("DoRm: invalid arguments");
        FreeCmd(&ctx);
        return;
    }
    int ret = unlink(ctx->argv[0]);
    if (ret == -1) {
        INIT_LOGE("DoRm: unlink %s failed: %d.", ctx->argv[0], errno);
    }
    FreeCmd(&ctx);
    return;
}

static void DoExport(const char *cmdContent)
{
    // format: export xxx /xxx/xxx/xxx
    struct CmdArgs *ctx = GetCmd(cmdContent, " ");
    int exportCmdNumber = 2;
    if (ctx == NULL || ctx->argv == NULL || ctx->argc != exportCmdNumber) {
        INIT_LOGE("DoExport: invalid arguments");
        FreeCmd(&ctx);
        return;
    }
    int ret = setenv(ctx->argv[0], ctx->argv[1], 1);
    if (ret != 0) {
        INIT_LOGE("DoExport: set %s with %s failed: %d", ctx->argv[0], ctx->argv[1], errno);
    }
    FreeCmd(&ctx);
    return;
}

static void DoExec(const char *cmdContent)
{
    // format: exec /xxx/xxx/xxx xxx
    pid_t pid = fork();
    if (pid < 0) {
        INIT_LOGE("DoExec: failed to fork child process to exec \"%s\"", cmdContent);
        return;
    }
    if (pid == 0) {
        struct CmdArgs *ctx = GetCmd(cmdContent, " ");
        if (ctx == NULL || ctx->argv == NULL) {
            INIT_LOGE("DoExec: invalid arguments");
            _exit(0x7f);
        }
#ifdef OHOS_LITE
        int ret = execve(ctx->argv[0], ctx->argv, NULL);
#else
        int ret = execv(ctx->argv[0], ctx->argv);
#endif
        if (ret == -1) {
            INIT_LOGE("DoExec: execute \"%s\" failed: %d.", cmdContent, errno);
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
        INIT_LOGE("DoSymlink: invalid arguments.");
        FreeCmd(&ctx);
        return;
    }

    int ret = symlink(ctx->argv[0], ctx->argv[1]);
    if (ret != 0) {
        INIT_LOGE("DoSymlink: link %s to %s failed: %d", ctx->argv[0], ctx->argv[1], errno);
    }
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
        INIT_LOGE("DoMakeNode: invalid arguments");
        FreeCmd(&ctx);
        return;
    }

    if (!access(ctx->argv[1], F_OK)) {
        INIT_LOGE("DoMakeNode failed, path has not sexisted");
    } else {
        mode_t deviceMode = GetDeviceMode(ctx->argv[deviceTypePos]);
        unsigned int major = strtoul(ctx->argv[majorDevicePos], NULL, decimal);
        unsigned int minor = strtoul(ctx->argv[minorDevicePos], NULL, decimal);
        mode_t authority = strtoul(ctx->argv[authorityPos], NULL, octal);

        int ret = mknod(ctx->argv[0], deviceMode | authority, makedev(major, minor));
        if (ret != 0) {
            INIT_LOGE("DoMakeNode: path: %s failed: %d", ctx->argv[0], errno);
        }
    }
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
        INIT_LOGE("DoMakedevice: invalid arugments");
        FreeCmd(&ctx);
        return;
    }
    unsigned int major = strtoul(ctx->argv[0], NULL, decimal);
    unsigned int minor = strtoul(ctx->argv[1], NULL, decimal);
    dev_t deviceId = makedev(major, minor);
    if (deviceId < 0) {
        INIT_LOGE("DoMakedevice \" %s \" failed :%d ", cmdContent, errno);
    }
    FreeCmd(&ctx);
    return;
}
#endif // __LITEOS__

struct CmdTable {
    char name[MAX_CMD_NAME_LEN];
    void (*DoFuncion)(const char *cmdContent);
};

static const struct CmdTable CMD_TABLE[] = {
    { "start ", DoStart },
    { "mkdir ", DoMkDir },
    { "stop ", DoStop },
    { "reset ", DoReset },
    { "copy ", DoCopy },
    { "chmod ", DoChmod },
    { "chown ", DoChown },
    { "mount ", DoMount },
    { "write ", DoWrite },
    { "rmdir ", DoRmdir },
    { "rm ", DoRm },
#ifndef OHOS_LITE
    { "symlink ", DoSymlink },
    { "makedev ", DoMakeDevice },
    { "mknode ", DoMakeNode },
    { "insmod ", DoInsmod },
    { "trigger ", DoTriggerExec },
    { "setparam ", DoSetParam },
    { "load_param ", LoadDefaultParams },
#endif
    { "export ", DoExport },
    { "setrlimit ", DoSetrlimit },
    { "exec ", DoExec },
    { "loadcfg ", DoLoadCfg },
    { "reboot ", DoReboot }
};

void ParseCmdLine(const char* cmdStr, CmdLine* resCmd)
{
    size_t cmdLineLen = 0;
    if (cmdStr == NULL || resCmd == NULL || (cmdLineLen = strlen(cmdStr)) == 0) {
        return;
    }

    size_t supportCmdCnt = ARRAY_LENGTH(CMD_TABLE);
    int foundAndSucceed = 0;
    for (size_t i = 0; i < supportCmdCnt; ++i) {
        size_t curCmdNameLen = strlen(CMD_TABLE[i].name);
        if (cmdLineLen > curCmdNameLen && cmdLineLen <= (curCmdNameLen + MAX_CMD_CONTENT_LEN) &&
            strncmp(CMD_TABLE[i].name, cmdStr, curCmdNameLen) == 0) {
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
        INIT_LOGE("Cannot parse command: %s", cmdStr);
        (void)memset_s(resCmd, sizeof(*resCmd), 0, sizeof(*resCmd));
    }
}

void DoCmd(const CmdLine* curCmd)
{
    // null curCmd or empty command, just quit.
    if (curCmd == NULL || curCmd->name[0] == '\0') {
        return;
    }
    DoCmdByName(curCmd->name, curCmd->cmdContent);
}

void DoCmdByName(const char *name, const char *cmdContent)
{
    if (name == NULL || cmdContent == NULL) {
        return;
    }
    size_t cmdCnt = sizeof(CMD_TABLE) / sizeof(CMD_TABLE[0]);
    unsigned int i = 0;

    for (; i < cmdCnt; ++i) {
        if (strncmp(name, CMD_TABLE[i].name, strlen(CMD_TABLE[i].name)) == 0) {
            CMD_TABLE[i].DoFuncion(cmdContent);
            break;
        }
    }
    if (i == cmdCnt) {
        INIT_LOGE("DoCmd, unknown cmd name %s.", name);
    }
}

const char *GetMatchCmd(const char *cmdStr)
{
    if (cmdStr == NULL) {
        return NULL;
    }
    size_t supportCmdCnt = sizeof(CMD_TABLE) / sizeof(CMD_TABLE[0]);
    for (size_t i = 0; i < supportCmdCnt; ++i) {
        size_t curCmdNameLen = strlen(CMD_TABLE[i].name);
        if (strncmp(CMD_TABLE[i].name, cmdStr, curCmdNameLen) == 0) {
            return cmdStr;
        }
    }
    return NULL;
}
