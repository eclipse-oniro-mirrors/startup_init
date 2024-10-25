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

#include <stdio.h>
#include <string.h>

#include "init_utils.h"

#define MAX_VALUE_SIZE 128
#define RED "\033[0;32;31m"
#define GREEN "\033[0;32;32m"
#define CLEAR "\033[0m"

typedef struct {
    char *buf;
    char *name;
    char *value;
} Test;

char *GetProcCmdlineName(const char *name, const char *buffer, char *endData)
{
    char *tmp = strstr(buffer, name);
    do {
        if (tmp == NULL) {
            return NULL;
        }
        tmp = tmp + strlen(name);
        while (tmp < endData && *tmp == ' ') {
            tmp++;
        }
        if (tmp >= endData) {
            return NULL;
        }
        if (*tmp == '=') {
            break;
        }
        tmp = strstr(tmp + 1, name);
    } while (tmp < endData);

    if (tmp >= endData) {
        return NULL;
    }
    tmp++;
    return tmp;
}

int GetProcCmdlineValue(const char *name, const char *buffer, char *value, int length)
{
    INIT_ERROR_CHECK(name != NULL && buffer != NULL && value != NULL, return -1, "Failed get parameters");
    char *endData = (char *)buffer + strlen(buffer);
    char *tmp = GetProcCmdlineName(name, buffer, endData);
    if (tmp == NULL) {
        return -1;
    }

    char *temp = GetProcCmdlineName(name, tmp, endData);
    if (temp != NULL) {
        BEGET_LOGE("conflict name: %s", name);
        return -1;
    }
    size_t i = 0;
    size_t endIndex = 0;
    while (tmp < endData && *tmp == ' ') {
        tmp++;
    }
    for (; i < (size_t)length; tmp++) {
        if (tmp >= endData || *tmp == ' ' || *tmp == '\n' || *tmp == '\r' || *tmp == '\t') {
            endIndex = i;
            break;
        }
        if (*tmp == '=') {
            if (endIndex != 0) { // for root=uuid=xxxx
                break;
            }
            i = 0;
            endIndex = 0;
            continue;
        }
        value[i++] = *tmp;
    }
    if (i >= (size_t)length) {
        return -1;
    }
    value[endIndex] = '\0';
    return 0;
}

/*
* 本测试用例目前是针对模糊匹配，即name后缀相同就视为相同
* 若判断逻辑发生变化，需要修改测试用例
*/
Test g_test[] = {
    {"abc=v1", "abc", "v1"},
    {"abc=v1 abc", "abc", "v1"},     // 没有=的abc不是键
    {"abc=v1 ab=v2", "abc", "v1"},
    {"abc=v1 abcd=v2", "abc", "v1"},
    {"abc=v1 dabc=v2", "abc", ""},   // dabc的后缀和abc相同
    {"ab=v1", "abc", ""},
    {"dabc=v2", "abc", "v2"},
    {"abc=v1 ab abc=v1", "abc", ""},
    {"abc=v1 ab abc=v2", "abc", ""},
    {"abc=v1 abc=abc", "abc", ""},
    {"abc=abc", "abc", "abc"},
    {"abc=abc abc", "abc", "abc"},
    {"abc=abc abc=abc abc", "abc", ""},
    {"abc=abc abc=abc abc=abc", "abc", ""},
    {"v1=abc", "abc", ""},
    {"abc=v1", "=", ""},
    {"abc=v1abc", "abc", "v1abc"},
    {"abc=v1 cba=v1", "abc", "v1"},
    {"abc=v1 ababc=v1", "abc", ""},
    {"aaa=aaa aaaaaa", "aaa", "aaa"},
    {"aaa=aaa aaaaaa", "a", "aaa"},
    {"aaa=aaa aaaaaa", "aa", "aaa"},
    {"aaa=aaa aaaaaa", "aaaaa", ""},
    {"abC=v1 asdawed    ", "abC", "v1"},
    {"abc=v1 abc acbacb    ", "abc", "v1"},
    {"abc=v1 ab=v2 abacbabacb ", "abc", "v1"},
    {"abc=v1 abcd=v2 acb=ab abb abc   ", "abc", "v1"},
    {"abc=v1 dabc=v2  ddab=ddabc abcabc=abc  adgsb   ", "abc", ""},
    {"ab=v1 sf ababab=abc abc   ", "abc", ""},
    {"dabc=v2 dabcdabdac=dabcv2  sdf   ", "abc", "v2"},
    {"ABC=v1 ab abc=v1 bsdaf   ", "ABC", ""},
    {"abc=v1 ab abc=v2  ababc=abc ", "abc", ""},
    {"abc=v1 abc=abc cba=abc    ", "abc", ""},
    {"abc=abc cba=cba  abca=abc  abc ", "abc", "abc"},
    {"File=abc abc file=asdasd  ", "File", "abc"},
    {"abc=abc abc=abc abc aaa  ", "abc", ""},
    {"abc=abc abc=abc abc=abc ", "abc", ""},
    {"v1=abc ab=v1abc a ", "abc", ""},
    {"abc=v1 sa   ", "=", ""},
    {"abc=v1abc v1abcv1=v1 ", "abc", "v1abc"},
    {"abc=v1 cba=v1 a b c = v 1  ", "abc", "v1"},
    {"abc=v1 ababc=v1 aabbcc=v1 a ", "abc", ""},
    {"aaa=aaa aaaaaa aa aa ", "aaa", "aaa"},
    {"aaa=aaa aaaaaa AAA=aaa  ", "a", "aaa"},
    {"aaa=aaa aaaaaa Aa=aa", "aa", "aaa"},
    {"aaa=aaa aaaaaa aaaa=aaaa a=aa aa=a ", "aaaaa", ""}
};

int GetParameterFromCmdLine(const char *paramName, char *value, size_t valueLen)
{
    char *buffer = ReadFileData(BOOT_CMD_LINE);
    BEGET_ERROR_CHECK(buffer != NULL, return -1, "Failed to read /proc/cmdline");
    int ret = GetProcCmdlineValue(paramName, buffer, value, valueLen);
    free(buffer);
    return ret;
}

void MatchingTest()
{
    system("cls");
    bool success = true;
    int cntAll = sizeof(g_test) / sizeof(g_test[0]);
    int cntFailed = 0;
    for (int i = 0; i < cntAll; ++i) {
        char value[MAX_VALUE_SIZE] = {0};
        char *buffer = g_test[i].buf;
        char *name = g_test[i].name;
        (void)GetProcCmdlineValue(name, buffer, value, MAX_VALUE_SIZE);
        printf("buffer:%s\n", buffer);
        printf(GREEN"except value:%s\n"CLEAR, g_test[i].value);
        if (strcmp(value, g_test[i].value) == 0) {
            printf(GREEN"actual value:%s\n"CLEAR, value);
        } else {
            printf(RED"actual value:%s\n"CLEAR, value);
            success = false;
            cntFailed++;
        }
    }
    if (success) {
        printf(GREEN"All %d tests successed.\n"CLEAR, cntAll);
    } else {
        printf(RED"All %d tests, failed %d.\n"CLEAR, cntAll, cntFailed);
    }
}

int main()
{
    MatchingTest();
}