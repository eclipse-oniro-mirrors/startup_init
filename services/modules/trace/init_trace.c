/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <fcntl.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include "cJSON.h"
#include "init_module_engine.h"
#include "init_param.h"
#include "init_utils.h"
#include "plugin_adapter.h"
#include "securec.h"

#define CHUNK_SIZE 65536
#define BLOCK_SIZE 4096
#define WAIT_MILLISECONDS 10
#define BUFFER_SIZE_KB 10240  // 10M

#define TRACE_CFG_KERNEL "KERNEL"
#define TRACE_CFG_USER "USER"
#define TRACE_TAG_PARAMETER "debug.hitrace.tags.enableflags"
#define TRACE_DEBUG_FS_PATH "/sys/kernel/debug/tracing/"
#define TRACE_FS_PATH "/sys/kernel/tracing/"
#define TRACE_CFG_PATH STARTUP_INIT_UT_PATH"/system/etc/init_trace.cfg"
#define TRACE_OUTPUT_PATH "/data/service/el0/startup/init/init_trace.log"
#define TRACE_OUTPUT_PATH_ZIP "/data/service/el0/startup/init/init_trace.zip"

#define BOOT_TRACE_COUNT_PARAM "persist.hitrace.boot_trace.count"
#define BOOT_TRACE_ACTIVE_PARAM "debug.hitrace.boot_trace.active"
#define BOOT_TRACE_CONST_DEBUGGABLE_PARAM "const.debuggable"

#define BOOT_TRACE_REPEAT_MIN 1
#define BOOT_TRACE_REPEAT_MAX 100

// various operating paths of ftrace
#define TRACING_ON_PATH "tracing_on"
#define TRACE_PATH "trace"
#define TRACE_MARKER_PATH "trace_marker"
#define TRACE_CURRENT_TRACER "current_tracer"
#define TRACE_BUFFER_SIZE_KB "buffer_size_kb"
#define TRACE_CLOCK "trace_clock"
#define TRACE_DEF_CLOCK "boot"

typedef enum {
    TRACE_STATE_IDLE,
    TRACE_STATE_STARTED,
    TRACE_STATE_STOPED,
    TRACE_STATE_INTERRUPT
} TraceState;

typedef struct {
    char *traceRootPath;
    char buffer[PATH_MAX];
    cJSON *jsonRootNode;
    TraceState traceState;
    uint32_t compress : 1;
} TraceWorkspace;

static TraceWorkspace g_traceWorkspace = {NULL, {0}, NULL, 0, 0};
static TraceWorkspace *GetTraceWorkspace(void)
{
    return &g_traceWorkspace;
}

static bool IsBootTraceActiveOn(void)
{
    char active[PARAM_VALUE_LEN_MAX] = {0};
    uint32_t len = PARAM_VALUE_LEN_MAX;
    int ret = SystemReadParam(BOOT_TRACE_ACTIVE_PARAM, active, &len);
    if (ret != 0 || len == 0) {
        return false;
    }
    return strcmp(active, "1") == 0;
}

/* Match hitrace IsRootVersion / const.debuggable: only "1" allows boot_trace capture from init. */
static bool IsBootTraceConstDebuggableOn(void)
{
    char value[PARAM_VALUE_LEN_MAX] = {0};
    uint32_t len = PARAM_VALUE_LEN_MAX;
    int ret = SystemReadParam(BOOT_TRACE_CONST_DEBUGGABLE_PARAM, value, &len);
    if (ret != 0 || len == 0) {
        return false;
    }
    return strcmp(value, "1") == 0;
}

/* Align with hitrace IsTraceMounted: trace_marker must exist before OpenTrace can succeed. */
static bool IsTraceModeOpen(void)
{
    char enabled[PARAM_VALUE_LEN_MAX] = {0};
    uint32_t len = PARAM_VALUE_LEN_MAX;
    int ret = SystemReadParam("persist.init.trace.enabled", enabled, &len);
    PLUGIN_LOGI("SystemReadParam persist.init.trace.enabled:%s, ret:%d", enabled, ret);
    if (ret == 0) {
        if (strcmp(enabled, "1") == 0) {
            return true;
        }
    }
    return false;
}

static bool SpawnHitraceBootTrace(void)
{
    errno = 0;
    struct group *shellGroup = getgrnam("shell");
    if (shellGroup == NULL) {
        PLUGIN_LOGW("SpawnHitraceBootTrace: getgrnam(shell) failed errno:%d", errno);
        return false;
    }
    /* getgrnam() uses static storage; copy gid now before next lookup overwrites it. */
    gid_t shellGid = shellGroup->gr_gid;
    endgrent();
    pid_t pid = fork();
    if (pid == 0) {
        gid_t gids[] = { shellGid };
        if (setgroups(sizeof(gids) / sizeof(gids[0]), gids) != 0) {
            _exit(EXIT_FAILURE);
        }
        /* Child transitions init -> hitrace via SELinux domain_auto_transition.
         * hitrace domain needs data_local_tmp access (developer_only policy). */
        (void)execl("/system/bin/hitrace", "hitrace", "boot-trace", (char *)NULL);
    }
    if (pid < 0) {
        PLUGIN_LOGE("SpawnHitraceBootTrace: fork failed, errno:%d", errno);
        return false;
    }
    return true;
}

static char *ReadFile(const char *path)
{
    struct stat fileStat = {0};
    if (stat(path, &fileStat) != 0 || fileStat.st_size <= 0) {
        PLUGIN_LOGE("Invalid file %s or buffer %zu", path, fileStat.st_size);
        return NULL;
    }
    char realPath[PATH_MAX] = "";
    realpath(path, realPath);
    FILE *fd = fopen(realPath, "r");
    PLUGIN_CHECK(fd != NULL, return NULL, "failed fopen path %s", path);
    char *buffer = NULL;
    do {
        buffer = (char*)calloc((size_t)(fileStat.st_size + 1), sizeof(char));
        PLUGIN_CHECK(buffer != NULL, break, "failed alloc memory for path %s", path);
        if (fread(buffer, fileStat.st_size, 1, fd) != 1) {
            PLUGIN_LOGE("failed read file %s errno:%d", path, errno);
            free(buffer);
            buffer = NULL;
        } else {
            buffer[fileStat.st_size] = '\0';
        }
    } while (0);
    (void)fclose(fd);
    return buffer;
}

static int InitTraceWorkspace(TraceWorkspace *workspace)
{
    workspace->traceRootPath = NULL;
    workspace->traceState = TRACE_STATE_IDLE;
    workspace->compress = 0;
    char *fileBuf = ReadFile(TRACE_CFG_PATH);
    PLUGIN_CHECK(fileBuf != NULL, return -1, "failed read file content %s", TRACE_CFG_PATH);
    workspace->jsonRootNode = cJSON_Parse(fileBuf);
    PLUGIN_CHECK(workspace->jsonRootNode != NULL, free(fileBuf);
        return -1, "Failed to parse json file %s", TRACE_CFG_PATH);
    workspace->compress = cJSON_IsTrue(cJSON_GetObjectItem(workspace->jsonRootNode, "compress")) ? 1 : 0;
    PLUGIN_LOGI("InitTraceWorkspace compress :%d", workspace->compress);
    free(fileBuf);
    return 0;
}

static void DestroyTraceWorkspace(TraceWorkspace *workspace)
{
    if (workspace->traceRootPath) {
        free(workspace->traceRootPath);
        workspace->traceRootPath = NULL;
    }
    if (workspace->jsonRootNode) {
        cJSON_Delete(workspace->jsonRootNode);
        workspace->jsonRootNode = NULL;
    }
    workspace->traceState = TRACE_STATE_IDLE;
}

static bool IsTraceMountedInner(TraceWorkspace *workspace, const char *fsPath)
{
    int len = sprintf_s((char *)workspace->buffer, sizeof(workspace->buffer),
        "%s%s", fsPath, TRACE_MARKER_PATH);
    PLUGIN_CHECK(len > 0, return false, "failed format path %s", fsPath);
    if (access(workspace->buffer, F_OK) != -1) {
        workspace->traceRootPath = strdup(fsPath);
        PLUGIN_CHECK(workspace->traceRootPath != NULL, return false, "failed dup fsPath");
        return true;
    }
    return false;
}

static bool IsTraceMounted(TraceWorkspace *workspace)
{
    return IsTraceMountedInner(workspace, TRACE_DEBUG_FS_PATH) ||
        IsTraceMountedInner(workspace, TRACE_FS_PATH);
}

static bool IsWritableFile(const char *filename)
{
    TraceWorkspace *workspace = GetTraceWorkspace();
    PLUGIN_CHECK(workspace != NULL, return false, "failed get trace workspace");
    int len = sprintf_s((char *)workspace->buffer, sizeof(workspace->buffer),
        "%s%s", workspace->traceRootPath, filename);
    PLUGIN_CHECK(len > 0, return false, "failed format path %s", filename);
    return access(workspace->buffer, W_OK) != -1;
}

static bool WriteStrToFile(const char *filename, const char *str)
{
    PLUGIN_LOGV("WriteStrToFile filename %s %s", filename, str);
    TraceWorkspace *workspace = GetTraceWorkspace();
    PLUGIN_CHECK(workspace != NULL, return false, "failed get trace workspace");
    int len = sprintf_s((char *)workspace->buffer, sizeof(workspace->buffer),
        "%s%s", workspace->traceRootPath, filename);
    PLUGIN_CHECK(len > 0, return false, "failed format path %s", filename);
    char realPath[PATH_MAX] = "";
    realpath(workspace->buffer, realPath);
    FILE *outfile = fopen(realPath, "w");
    PLUGIN_CHECK(outfile != NULL, return false, "failed open file %s.", workspace->buffer);
    (void)fprintf(outfile, "%s", str);
    (void)fflush(outfile);
    (void)fclose(outfile);
    return true;
}

static bool SetTraceEnabled(const char *path, bool enabled)
{
    return WriteStrToFile(path, enabled ? "1" : "0");
}

static bool SetBufferSize(int bufferSize)
{
    if (!WriteStrToFile(TRACE_CURRENT_TRACER, "nop")) {
        PLUGIN_LOGE("%s", "Error: write \"nop\" to %s\n", TRACE_CURRENT_TRACER);
    }
    char buffer[20] = {0}; // 20 max int number
    int len = sprintf_s((char *)buffer, sizeof(buffer), "%d", bufferSize);
    PLUGIN_CHECK(len > 0, return false, "failed format int %d", bufferSize);
    PLUGIN_LOGE("SetBufferSize path %s %s", TRACE_BUFFER_SIZE_KB, buffer);
    return WriteStrToFile(TRACE_BUFFER_SIZE_KB, buffer);
}

static bool SetClock(const char *timeClock)
{
    return WriteStrToFile(TRACE_CLOCK, timeClock);
}

static bool SetOverWriteEnable(bool enabled)
{
    return SetTraceEnabled("options/overwrite", enabled);
}

static bool SetTgidEnable(bool enabled)
{
    return SetTraceEnabled("options/record-tgid", enabled);
}

static bool SetTraceTagsEnabled(uint64_t tags)
{
    TraceWorkspace *workspace = GetTraceWorkspace();
    PLUGIN_CHECK(workspace != NULL, return false, "failed get trace workspace");
    int len = sprintf_s((char *)workspace->buffer, sizeof(workspace->buffer), "%" PRIu64 "", tags);
    PLUGIN_CHECK(len > 0, return false, "failed format tags %" PRId64 "", tags);
    return SystemWriteParam(TRACE_TAG_PARAMETER, workspace->buffer) == 0;
}

static bool RefreshServices()
{
    return true;
}

static cJSON *GetArrayItem(const cJSON *fileRoot, int *arrSize, const char *arrName)
{
    cJSON *arrItem = cJSON_GetObjectItemCaseSensitive(fileRoot, arrName);
    PLUGIN_CHECK(cJSON_IsArray(arrItem), return NULL);
    *arrSize = cJSON_GetArraySize(arrItem);
    return *arrSize > 0 ? arrItem : NULL;
}

static bool SetUserSpaceSettings(void)
{
    TraceWorkspace *workspace = GetTraceWorkspace();
    PLUGIN_CHECK(workspace != NULL, return false, "failed get trace workspace");
    int size = 0;
    cJSON *userItem = GetArrayItem(workspace->jsonRootNode, &size, TRACE_CFG_USER);
    PLUGIN_CHECK(userItem != NULL, return false, "failed get user info");

    PLUGIN_LOGI("SetUserSpaceSettings: %d", size);
    uint64_t enabledTags = 0;
    for (int i = 0; i < size; i++) {
        cJSON *item = cJSON_GetArrayItem(userItem, i);
        PLUGIN_LOGI("Tag name = %s ", cJSON_GetStringValue(cJSON_GetObjectItem(item, "name")));
        int tag = cJSON_GetNumberValue(cJSON_GetObjectItem(item, "tag"));
        enabledTags |= 1ULL << tag;
    }
    PLUGIN_LOGI("User enabledTags: %" PRId64 "", enabledTags);
    return SetTraceTagsEnabled(enabledTags) && RefreshServices();
}

static bool ClearUserSpaceSettings(void)
{
    return SetTraceTagsEnabled(0) && RefreshServices();
}

static bool SetKernelTraceEnabled(const TraceWorkspace *workspace, bool enabled)
{
    bool result = true;
    PLUGIN_LOGI("SetKernelTraceEnabled %s", enabled ? "enable" : "disable");
    int size = 0;
    cJSON *kernelItem = GetArrayItem(workspace->jsonRootNode, &size, TRACE_CFG_KERNEL);
    PLUGIN_CHECK(kernelItem != NULL, return false, "failed get user info");
    for (int i = 0; i < size; i++) {
        cJSON *tagJson = cJSON_GetArrayItem(kernelItem, i);
        const char *name = cJSON_GetStringValue(cJSON_GetObjectItem(tagJson, "name"));
        int pathCount = 0;
        cJSON *paths = GetArrayItem(tagJson, &pathCount, "sys-files");
        if (paths == NULL) {
            continue;
        }
        PLUGIN_LOGV("Kernel tag name: %s sys-files %d", name, pathCount);
        for (int j = 0; j < pathCount; j++) {
            char *path = cJSON_GetStringValue(cJSON_GetArrayItem(paths, j));
            PLUGIN_CHECK(path != NULL, continue);
            if (!IsWritableFile(path)) {
                PLUGIN_LOGW("Path %s is not writable for %s", path, name);
                continue;
            }
            result = result && SetTraceEnabled(path, enabled);
        }
    }
    return result;
}

static bool DisableAllTraceEvents(void)
{
    TraceWorkspace *workspace = GetTraceWorkspace();
    PLUGIN_CHECK(workspace != NULL, return false, "failed get trace workspace");
    return SetKernelTraceEnabled(workspace, false);
}

static void CheckKernelType(bool *isLinux)
{
    struct utsname uts;
    if (uname(&uts) == -1) {
        PLUGIN_LOGE("Kernel type get failed,errno:%d", errno);
        return;
    }

    if (strcmp(uts.sysname, "Linux") == 0) {
        PLUGIN_LOGE("Kernel type is linux");
        *isLinux = true;
        return;
    }

    *isLinux = false;
}

static bool SetKernelSpaceSettings(void)
{
    TraceWorkspace *workspace = GetTraceWorkspace();
    PLUGIN_CHECK(workspace != NULL, return false, "failed get trace workspace");
    bool ret = SetBufferSize(BUFFER_SIZE_KB);
    PLUGIN_CHECK(ret, return false, "failed set buffer");
    ret = SetClock(TRACE_DEF_CLOCK);
    PLUGIN_CHECK(ret, return false, "failed set clock");
    bool isLinux = false;
    CheckKernelType(&isLinux);
    if (isLinux) {
        ret = SetOverWriteEnable(true);
        PLUGIN_CHECK(ret, return false, "failed set write enable");
        ret = SetTgidEnable(true);
        PLUGIN_CHECK(ret, return false, "failed set tgid enable");
    }
    ret = SetKernelTraceEnabled(workspace, false);
    PLUGIN_CHECK(ret, return false, "Pre-clear kernel tracers failed");
    return SetKernelTraceEnabled(workspace, true);
}

static bool ClearKernelSpaceSettings(void)
{
    return DisableAllTraceEvents() && SetOverWriteEnable(true) && SetBufferSize(1) && SetClock("boot");
}

static bool ClearTrace(void)
{
    TraceWorkspace *workspace = GetTraceWorkspace();
    PLUGIN_CHECK(workspace != NULL, return false, "failed get trace workspace");

    int len = sprintf_s((char *)workspace->buffer, sizeof(workspace->buffer),
        "%s%s", workspace->traceRootPath, TRACE_PATH);
    PLUGIN_CHECK(len > 0, return false, "failed format path %s", TRACE_PATH);
    char realPath[PATH_MAX] = "";
    realpath(workspace->buffer, realPath);
    // clear old trace file
    int fd = open(realPath, O_RDWR);
    PLUGIN_CHECK(fd >= 0, return false, "failed open file %s errno %d", workspace->buffer, errno);
    (void)ftruncate(fd, 0);
    close(fd);
    return true;
}

static void DumpCompressedTrace(int traceFd, int outFd)
{
    int flush = Z_NO_FLUSH;
    uint8_t *inBuffer = calloc(1, CHUNK_SIZE);
    PLUGIN_CHECK(inBuffer != NULL, return, "Error: couldn't allocate buffers\n");
    uint8_t *outBuffer = malloc(CHUNK_SIZE);
    PLUGIN_CHECK(outBuffer != NULL, free(inBuffer);
        return, "Error: couldn't allocate buffers\n");

    z_stream zs = {};
    int ret = 0;
    deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY); // 16 8 bit
    do {
        // read data
        ssize_t bytesRead = TEMP_FAILURE_RETRY(read(traceFd, inBuffer, CHUNK_SIZE));
        PLUGIN_CHECK(bytesRead >= 0, break, "Error: reading trace, errno: %d\n", errno);
        zs.avail_in = (uInt)bytesRead;
        flush = zs.avail_in == 0 ? Z_FINISH : Z_NO_FLUSH;
        zs.next_in = inBuffer;
        do {
            zs.next_out = outBuffer;
            zs.avail_out = CHUNK_SIZE;
            ret = deflate(&zs, flush);
            PLUGIN_CHECK(ret != Z_STREAM_ERROR, break, "Error: deflate trace, errno: %d\n", errno);
            size_t have = CHUNK_SIZE - zs.avail_out;
            size_t bytesWritten = (size_t)TEMP_FAILURE_RETRY(write(outFd, outBuffer, have));
            PLUGIN_CHECK(bytesWritten >= have, flush = Z_FINISH; break,
                "Error: writing deflated trace, errno: %d\n", errno);
        } while (zs.avail_out == 0);
    } while (flush != Z_FINISH);

    ret = deflateEnd(&zs);
    PLUGIN_ONLY_LOG(ret == Z_OK, "error cleaning up zlib: %d\n", ret);
    free(inBuffer);
    free(outBuffer);
}

static void DumpTrace(const TraceWorkspace *workspace, int outFd, const char *path)
{
    int len = sprintf_s((char *)workspace->buffer, sizeof(workspace->buffer), "%s%s", workspace->traceRootPath, path);
    PLUGIN_CHECK(len > 0, return, "failed format path %s", path);
    char realPath[PATH_MAX] = "";
    realpath(workspace->buffer, realPath);
    int traceFd = open(realPath, O_RDWR);
    PLUGIN_CHECK(traceFd >= 0, return, "failed open file %s errno %d", workspace->buffer, errno);

    ssize_t bytesWritten;
    ssize_t bytesRead;
    if (workspace->compress) {
        DumpCompressedTrace(traceFd, outFd);
    } else {
        char buffer[BLOCK_SIZE];
        do {
            bytesRead = TEMP_FAILURE_RETRY(read(traceFd, buffer, BLOCK_SIZE));
            if ((bytesRead == 0) || (bytesRead == -1)) {
                break;
            }
            bytesWritten = TEMP_FAILURE_RETRY(write(outFd, buffer, bytesRead));
        } while (bytesWritten > 0);
    }
    close(traceFd);
}

static bool MarkOthersClockSync(void)
{
    TraceWorkspace *workspace = GetTraceWorkspace();
    PLUGIN_CHECK(workspace != NULL, return false, "failed get trace workspace");
    int len = sprintf_s((char *)workspace->buffer, sizeof(workspace->buffer), "%s%s",
        workspace->traceRootPath, TRACE_MARKER_PATH);
    PLUGIN_CHECK(len > 0, return false, "failed format path %s", TRACE_MARKER_PATH);

    struct timespec mts = {0, 0};
    struct timespec rts = {0, 0};
    if (clock_gettime(CLOCK_REALTIME, &rts) == -1) {
        PLUGIN_LOGE("Error: get realtime, errno: %d", errno);
        return false;
    } else if (clock_gettime(CLOCK_MONOTONIC, &mts) == -1) {
        PLUGIN_LOGE("Error: get monotonic, errno: %d\n", errno);
        return false;
    }
    const unsigned int nanoSeconds = 1000000000;  // seconds converted to nanoseconds
    const unsigned int nanoToMill = 1000000;      // millisecond converted to nanoseconds
    const float nanoToSecond = 1000000000.0f;     // consistent with the ftrace timestamp format

    PLUGIN_LOGE("MarkOthersClockSync %s", workspace->buffer);
    char realPath[PATH_MAX] = { 0 };
    realpath(workspace->buffer, realPath);
    FILE *file = fopen(realPath, "wt+");
    PLUGIN_CHECK(file != NULL, return false, "Error: opening %s, errno: %d", TRACE_MARKER_PATH, errno);
    do {
        int64_t realtime = ((int64_t)rts.tv_sec * nanoSeconds + rts.tv_nsec) / nanoToMill;
        float parentTs = (float)((((float)mts.tv_sec) * nanoSeconds + mts.tv_nsec) / nanoToSecond);
        int ret = fprintf(file, "trace_event_clock_sync: realtime_ts=%" PRId64 "\n", realtime);
        PLUGIN_CHECK(ret > 0, break, "Warning: writing clock sync marker, errno: %d", errno);
        ret = fprintf(file, "trace_event_clock_sync: parent_ts=%f\n", parentTs);
        PLUGIN_CHECK(ret > 0, break, "Warning: writing clock sync marker, errno: %d", errno);
    } while (0);
    (void)fclose(file);
    return true;
}

/* If persist.hitrace.boot_trace.count is 1~100, consume once and spawn hitrace boot-trace. */
static void TryRunBootTraceByCount(void)
{
    if (IsTraceModeOpen()) {
        PLUGIN_LOGI("boot_trace skip: persist.init.trace.enabled=1 (init trace on, mutually exclusive)");
        return;
    }
    char buf[PARAM_VALUE_LEN_MAX] = {0};
    uint32_t len = PARAM_VALUE_LEN_MAX;
    int ret = SystemReadParam(BOOT_TRACE_COUNT_PARAM, buf, &len);
    if (ret != 0 || len == 0) {
        PLUGIN_LOGI("boot_trace skip: read %s failed ret:%d", BOOT_TRACE_COUNT_PARAM, ret);
        return;
    }
    errno = 0;
    char *endPtr = NULL;
    long countLong = strtol(buf, &endPtr, 10);
    if (endPtr == buf || *endPtr != '\0' || errno == ERANGE) {
        PLUGIN_LOGI("boot_trace skip: invalid count string");
        return;
    }
    if (countLong < BOOT_TRACE_REPEAT_MIN || countLong > BOOT_TRACE_REPEAT_MAX) {
        PLUGIN_LOGI("boot_trace skip: count=%ld out of [%d,%d]", countLong,
            BOOT_TRACE_REPEAT_MIN, BOOT_TRACE_REPEAT_MAX);
        return;
    }
    if (IsBootTraceActiveOn()) {
        PLUGIN_LOGI("boot_trace skip: %s already active", BOOT_TRACE_ACTIVE_PARAM);
        return;
    }
    if (!IsBootTraceConstDebuggableOn()) {
        PLUGIN_LOGI("boot_trace skip: %s is not 1 (non-debuggable image)", BOOT_TRACE_CONST_DEBUGGABLE_PARAM);
        return;
    }
    int count = (int)countLong;
    char newVal[PARAM_VALUE_LEN_MAX];
    if (sprintf_s(newVal, sizeof(newVal), "%d", count - 1) <= 0) {
        return;
    }
    if (SystemWriteParam(BOOT_TRACE_COUNT_PARAM, newVal) != 0) {
        PLUGIN_LOGE("boot_trace: write %s failed", BOOT_TRACE_COUNT_PARAM);
        return;
    }
    /* debug.hitrace.boot_trace.active is set by hitrace boot-trace after exec (not here), so shell/manual
     * launch does not depend on init pre-setting; duplicate instances see active==1 and exit early. */
    if (!SpawnHitraceBootTrace()) {
        return;
    }
    PLUGIN_LOGI("boot_trace: triggered, count %d -> %d", count, count - 1);
}

static int InitStartTrace(void)
{
    TraceWorkspace *workspace = GetTraceWorkspace();
    PLUGIN_CHECK(workspace != NULL, return 0, "failed get trace workspace");
    PLUGIN_CHECK(workspace->traceState == TRACE_STATE_IDLE, return 0,
        "Invalid state for trace %d", workspace->traceState);

    InitTraceWorkspace(workspace);
    PLUGIN_CHECK(IsTraceMounted(workspace), return -1);

    PLUGIN_CHECK(workspace->traceRootPath != NULL && workspace->jsonRootNode != NULL,
        return -1, "No trace root path or config");

    // load json init_trace.cfg
    if (!SetKernelSpaceSettings() || !SetTraceEnabled(TRACING_ON_PATH, true)) {
        PLUGIN_LOGE("failed enable kernel space setting");
        ClearKernelSpaceSettings();
        return -1;
    }
    ClearTrace();
    PLUGIN_LOGI("capturing trace...");
    if (!SetUserSpaceSettings()) {
        PLUGIN_LOGE("failed enable user space setting");
        ClearKernelSpaceSettings();
        ClearUserSpaceSettings();
        return -1;
    }
    workspace->traceState = TRACE_STATE_STARTED;
    return 0;
}

static int InitStopTrace(void)
{
    PLUGIN_LOGI("Stop trace now ...");
    TraceWorkspace *workspace = GetTraceWorkspace();
    PLUGIN_CHECK(workspace != NULL, return 0, "failed get trace workspace");
    PLUGIN_CHECK(workspace->traceState == TRACE_STATE_STARTED, return 0, "Invalid state for trace %d",
        workspace->traceState);
    workspace->traceState = TRACE_STATE_STOPED;

    MarkOthersClockSync();
    // clear user tags first and sleep a little to let apps already be notified.
    ClearUserSpaceSettings();
    usleep(WAIT_MILLISECONDS);
    SetTraceEnabled(TRACING_ON_PATH, false);

    const char *path = workspace->compress ? TRACE_OUTPUT_PATH_ZIP : TRACE_OUTPUT_PATH;
    int outFd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (outFd >= 0) {
        DumpTrace(workspace, outFd, TRACE_PATH);
        close(outFd);
    } else {
        PLUGIN_LOGE("failed open file '%s', err=%d", path, errno);
    }

    ClearTrace();
    // clear kernel setting including clock type after dump(MUST) and tracing_on is off.
    ClearKernelSpaceSettings();
    // init hitrace config
    DoJobNow("init-hitrace");
    DestroyTraceWorkspace(workspace);
    return 0;
}

static int InitInterruptTrace(void)
{
    PLUGIN_LOGI("Interrupt trace now ...");
    TraceWorkspace *workspace = GetTraceWorkspace();
    PLUGIN_CHECK(workspace != NULL, return 0, "failed get trace workspace");
    PLUGIN_CHECK(workspace->traceState == TRACE_STATE_STARTED, return 0,
        "Invalid state for trace %d", workspace->traceState);

    workspace->traceState = TRACE_STATE_INTERRUPT;
    MarkOthersClockSync();
    // clear user tags first and sleep a little to let apps already be notified.
    ClearUserSpaceSettings();
    SetTraceEnabled(TRACING_ON_PATH, false);
    ClearTrace();
    // clear kernel setting including clock type after dump(MUST) and tracing_on is off.
    ClearKernelSpaceSettings();
    // init hitrace config
    DoJobNow("init-hitrace");
    DestroyTraceWorkspace(workspace);
    return 0;
}

static int DoInitTraceCmd(int id, const char *name, int argc, const char **argv)
{
    (void)id;
    (void)name;
    PLUGIN_CHECK(argc >= 1, return -1, "Invalid parameter");
    PLUGIN_LOGI("DoInitTraceCmd argc %d cmd %s", argc, argv[0]);
    if (strcmp(argv[0], "boot_trace") == 0) {
        TryRunBootTraceByCount();
        return 0;
    }
    PLUGIN_CHECK(IsTraceModeOpen(), return -1, "init trace disabled");
    if (strcmp(argv[0], "start") == 0) {
        return InitStartTrace();
    } else if (strcmp(argv[0], "stop") == 0) {
        return InitStopTrace();
    } else if (strcmp(argv[0], "1") == 0) {
        return InitInterruptTrace();
    }
    return 0;
}

static int g_executorId = -1;

static int InitStartBootTraceByParam(const HOOK_INFO *info, void *cookie)
{
    (void)info;
    (void)cookie;
    TryRunBootTraceByCount();
    return 0;
}

static int InitTraceInit(void)
{
    if (g_executorId == -1) {
        g_executorId = AddCmdExecutor("init_trace", DoInitTraceCmd);
        PLUGIN_LOGI("InitTraceInit executorId %d", g_executorId);
    }
    InitAddPostPersistParamLoadHook(0, InitStartBootTraceByParam);
    return 0;
}

static void InitTraceExit(void)
{
    PLUGIN_LOGI("InitTraceExit executorId %d", g_executorId);
    HookMgrDel(GetBootStageHookMgr(), INIT_POST_PERSIST_PARAM_LOAD, InitStartBootTraceByParam);
    if (g_executorId != -1) {
        RemoveCmdExecutor("init_trace", g_executorId);
    }
}

MODULE_CONSTRUCTOR(void)
{
    PLUGIN_LOGI("Start trace now ...");
    InitTraceInit();
}

MODULE_DESTRUCTOR(void)
{
    InitStopTrace();
    InitTraceExit();
}
