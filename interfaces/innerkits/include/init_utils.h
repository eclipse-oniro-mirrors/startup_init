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

#ifndef INIT_UTILS_H
#define INIT_UTILS_H
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

#include "beget_ext.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef struct {
    char *name;
    int value;
} InitArgInfo;

#define BASE_MS_UNIT 1000
#define MAX_INT_LEN 20
#define HEX_BASE 16
#define BINARY_BASE 2
#define OCTAL_BASE 8
#define DECIMAL_BASE 10
#define WAIT_MAX_SECOND 5
#define MAX_BUFFER_LEN 256
#define CMDLINE_VALUE_LEN_MAX 512
#define STDERR_HANDLE 2
#define ARRAY_LENGTH(array) (sizeof((array)) / sizeof((array)[0]))
#define BOOT_CMD_LINE STARTUP_INIT_UT_PATH"/proc/cmdline"

uid_t DecodeUid(const char *name);
gid_t DecodeGid(const char *name);
char *ReadFileToBuf(const char *configFile);
int GetProcCmdlineValue(const char *name, const char *buffer, char *value, int length);
char *ReadFileData(const char *fileName);

typedef struct NameValuePair {
    const char *name;
    const char *nameEnd;
    const char *value;
    const char *valueEnd;
} NAME_VALUE_PAIR;
int IterateNameValuePairs(const char *src, void (*iterator)(const NAME_VALUE_PAIR *nv, void *context), void *context);

int SplitString(char *srcPtr, const char *del, char **dstPtr, int maxNum);
void WaitForFile(const char *source, unsigned int maxSecond);
size_t WriteAll(int fd, const char *buffer, size_t size);
char *GetRealPath(const char *source);
int StringToInt(const char *str, int defaultValue);
int StringToUint(const char *name, unsigned int *value);
int MakeDirRecursive(const char *dir, mode_t mode);
void CheckAndCreateDir(const char *fileName);
int CheckAndCreatFile(const char *file, mode_t mode);
int MakeDir(const char *dir, mode_t mode);
int ReadFileInDir(const char *dirPath, const char *includeExt,
    int (*processFile)(const char *fileName, void *context), void *context);
char **SplitStringExt(char *buffer, const char *del, int *returnCount, int maxItemCount);
void FreeStringVector(char **vector, int count);
int InUpdaterMode(void);
int StringReplaceChr(char *strl, char oldChr, char newChr);

int OpenConsole(void);
int OpenKmsg(void);
void TrimTail(char *str, char c);
char *TrimHead(char *str, char c);

INIT_LOCAL_API uint32_t IntervalTime(struct timespec *startTime, struct timespec *endTime);

INIT_LOCAL_API int StringToULL(const char *str, unsigned long long int *out);
INIT_LOCAL_API int StringToLL(const char *str, long long int *out);
void CloseStdio(void);

int GetServiceGroupIdByPid(pid_t pid, gid_t *gids, uint32_t gidSize);
int GetParameterFromCmdLine(const char *paramName, char *value, size_t valueLen);

/**
 * @brief Get string index from a string array
 *
 * @param strArray string array
 *     Attension: last item in the array must be NULL, for example:
 *     const char *strArray[] = { "val1", "val2", NULL }
 * @param target string to be matched
 * @param ignoreCase 0 means exact match, others mean ignore case
 * @return return 0 if succeed; other values if failed.
 */
int StrArrayGetIndex(const char *strArray[], const char *target, int ignoreCase);

long long GetUptimeInMicroSeconds(const struct timespec *uptime);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif // INIT_UTILS_H
