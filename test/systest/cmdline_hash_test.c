/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#include "init_log.h"
#include "securec.h"
#include "service_control.h"

typedef struct NameValuePair {
    const char *name;
    const char *nameEnd;
    const char *value;
    const char *valueEnd;
} NAME_VALUE_PAIR;

#define MAX_BUF_SIZE  1024
#define MAX_SMALL_BUFFER 4096

#define MAX_PRIME_BOUND 10000
int g_prime[MAX_PRIME_BOUND];
bool g_notPrime[MAX_PRIME_BOUND] = {1, 1};

#define MAX_TABLE_SIZE 10000
int g_base;
int g_mod;
int g_primeCnt;
const char *g_cmdHashTable[MAX_TABLE_SIZE] = {0};

int GetHashByStr(const char *str)
{
    if (str == NULL) {
        return 0;
    }
    uint64_t res = 0;
    const char *tmp = str;
    while (*tmp != '\0') {
        res = res * g_base + (*tmp);
        tmp++;
    }
    return res % g_mod;
}

void EulerSieve()
{
    g_primeCnt = 0;
    for (int i = 2; i < MAX_PRIME_BOUND; ++i) { // 2是最小质数
        if (g_notPrime[i] == 0) {
            g_prime[g_primeCnt] = i;
            g_primeCnt++;
        }
        for (int j = 0; j < g_primeCnt; ++j) {
            if (i * g_prime[j] >= MAX_PRIME_BOUND) {
                break;
            }
            g_notPrime[i * g_prime[j]] = true; // 质数的倍数不是质数
            if (i % g_prime[j] == 0) {
                break;
            }
        }
    }
}

char *ReadFileData(const char *fileName)
{
    if (fileName == NULL) {
        return NULL;
    }
    char *buffer = NULL;
    int fd = -1;
    fd = open(fileName, O_RDONLY);
    if (fd < 0) {
        printf("Failed to read file %s errno:%d \n", fileName, errno);
        return NULL;
    }

    buffer = (char *)calloc(1, MAX_SMALL_BUFFER);
    if (buffer == NULL) {
        printf("Failed to allocate memory for %s", fileName);
        return NULL;
    }

    ssize_t readLen = read(fd, buffer, MAX_SMALL_BUFFER - 1);
    if (readLen <= 0 || readLen >= MAX_SMALL_BUFFER) {
        close(fd);
        free(buffer);
        printf("Failed to read data for %s", fileName);
        return NULL;
    }

    buffer[readLen] = '\0';
    if (fd != -1) {
        close(fd);
    }
    return buffer;
}

int CheckConflict(NAME_VALUE_PAIR nv)
{
    int nameSize = nv.nameEnd - nv.name;
    int valueSize = nv.valueEnd - nv.value;
    char *name = (char *)calloc(nameSize + 1, 1);
    if (name == NULL) {
        printf("Failed to malloc name. \n");
        return -1;
    }
    char *value = (char *)calloc(valueSize + 1, 1);
    if (value == NULL) {
        printf("Failed to malloc value. \n");
        free(name);
        return -1;
    }

    int ret = strncpy_s(name, nv.nameEnd - nv.name, nv.name, nameSize);
    if (ret < 0) {
        printf("copy name failed. \n");
        return -1;
    }
    ret = strncpy_s(value, nv.valueEnd - nv.value, nv.value, valueSize);
    if (ret < 0) {
        printf("copy value failed. \n");
        return -1;
    }

    int hash = GetHashByStr(name);
    if (g_cmdHashTable[hash] == NULL) {
        g_cmdHashTable[hash] = value;
    } else {
        free(name);
        return -1;
    }

    free(name);
    return 0;
}

int GetCmdlineNum(const char *src)
{
    int cnt = 0;
    const char *seperator;
    const char *tmp = src;
    NAME_VALUE_PAIR nv;
    
    do {
        nv.name = tmp;
        seperator = strchr(tmp, ' ');
        if (seperator == NULL) {
            nv.valueEnd = tmp + strlen(tmp);
            tmp = NULL;
        } else {
            nv.valueEnd = seperator;
            tmp = seperator + 1;
        }

        seperator = strchr(nv.name, '=');
        if (seperator == NULL) {
            continue;
        }
        if (seperator > nv.valueEnd) {
            continue;
        }

        nv.value = seperator + 1;
        nv.nameEnd = seperator;
        
        cnt += 1;
    } while (tmp != NULL);

    return cnt;
}

bool isConflict(const char *src)
{
    const char *seperator;
    const char *tmp = src;
    NAME_VALUE_PAIR nv;
    
    do {
        // Find space seperator
        nv.name = tmp;
        seperator = strchr(tmp, ' ');
        if (seperator == NULL) {
            // Last nv
            nv.valueEnd = tmp + strlen(tmp);
            tmp = NULL;
        } else {
            nv.valueEnd = seperator;
            tmp = seperator + 1;
        }

        // Find equal seperator
        seperator = strchr(nv.name, '=');
        if (seperator == NULL) {
            // Invalid name value pair
            continue;
        }
        if (seperator > nv.valueEnd) {
            // name without value, just ignore
            continue;
        }
        nv.nameEnd = seperator;
        nv.value = seperator + 1;

        int ret = CheckConflict(nv);
        if (ret < 0) {
            return true;
        }
    } while (tmp != NULL);

    return false;
}

int main(int argc, char **argv)
{
    if (argc <= 0) {
        return 0;
    }
    
    printf("\r\n\r\n"
            "+=========================================================+\r\n"
            "|        本工具用于对给定的键值对文件输出无冲突的基和模       |\r\n"
            "|        需要键值对以key=value形式存储，以空格为分隔符       |\r\n"
            "+=========================================================+\r\n"
            "\r\n接下来请输入键值对字符串所存储的文件：\r\n");
    
    char file[256];
    int ret = scanf_s("%s", file, sizeof(file));
    if (ret <= 0) {
        printf("input error \n");
        return 0;
    }

    const char *buf = ReadFileData(file);
    if (buf == NULL) {
        printf("read file failed. \n");
        return 0;
    }
    int num = GetCmdlineNum(buf);
    printf("文件中共有%d条键值对，设定哈希表大小从%d至%d进行遍历 \n", num, num, MAX_TABLE_SIZE);
    EulerSieve();
    for (g_mod = num; g_mod < MAX_TABLE_SIZE; ++g_mod) {
        for (int i = 1; i < g_primeCnt; ++i) {
            g_base = g_prime[i];
            ret = isConflict(buf);
            if (ret == 0) {
                printf("BASE=%d, MOD(MAX_HASH_SIZE)=%d \n", g_base, g_mod);

                return 0;
            }
        }
    }

    printf("can't find suitable parameters. \n");
    return 0;
}