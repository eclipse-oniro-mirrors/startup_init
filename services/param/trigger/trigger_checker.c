/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "trigger_checker.h"

#include <ctype.h>
#include "init_param.h"
#include "trigger_manager.h"
#include "securec.h"

#define MAX_CALC_PARAM 100
// 申请整块能存作为计算的节点
int CalculatorInit(LogicCalculator *calculator, int dataNumber, int dataUnit, int needCondition)
{
    PARAM_CHECK(calculator != NULL, return -1, "Invalid param");
    PARAM_CHECK(dataUnit <= (int)sizeof(LogicData), return -1, "Invalid param");
    PARAM_CHECK(dataNumber <= MAX_CALC_PARAM, return -1, "Invalid param");
    int dataSize = dataUnit * dataNumber;
    if (needCondition) {
        dataSize += MAX_DATA_BUFFER_MAX;
    }
    calculator->data = (char *)malloc(dataSize);
    PARAM_CHECK(calculator->data != NULL, return -1, "Failed to malloc for calculator");
    calculator->dataNumber = dataNumber;
    calculator->endIndex = 0;
    calculator->dataUnit = dataUnit;

    dataSize = dataUnit * dataNumber;
    calculator->conditionName = calculator->data + dataSize;
    dataSize += SUPPORT_DATA_BUFFER_MAX;
    calculator->conditionContent = calculator->data + dataSize;
    dataSize += SUPPORT_DATA_BUFFER_MAX;
    calculator->inputName = calculator->data + dataSize;
    dataSize += SUPPORT_DATA_BUFFER_MAX;
    calculator->inputContent = calculator->data + dataSize;
    dataSize += SUPPORT_DATA_BUFFER_MAX;
    calculator->readContent = calculator->data + dataSize;
    return memset_s(calculator->triggerContent,
        sizeof(calculator->triggerContent), 0, sizeof(calculator->triggerContent));
}

void CalculatorFree(LogicCalculator *calculator)
{
    PARAM_CHECK(calculator != NULL, return, "Invalid param");
    if (calculator->data != NULL) {
        free(calculator->data);
    }
    calculator->data = NULL;
}

static void CalculatorClear(LogicCalculator *calculator)
{
    PARAM_CHECK(calculator != NULL, return, "Invalid param");
    calculator->endIndex = 0;
}

static int CalculatorPushChar(LogicCalculator *calculator, char data)
{
    PARAM_CHECK(calculator != NULL, return -1, "Invalid param");
    PARAM_CHECK(calculator->endIndex < calculator->dataNumber, return -1, "More data for calculator support");
    PARAM_CHECK(sizeof(char) == calculator->dataUnit, return -1, "More data for calculator support");
    calculator->data[calculator->endIndex++] = data;
    return 0;
}

static int CalculatorPopChar(LogicCalculator *calculator, char *data)
{
    PARAM_CHECK(calculator != NULL, return -1, "Invalid param");
    PARAM_CHECK(calculator->endIndex < calculator->dataNumber, return -1, "More data for calculator support");
    if (calculator->endIndex == 0) {
        return -1;
    }
    *data = calculator->data[--calculator->endIndex];
    return 0;
}

static int CalculatorPush(LogicCalculator *calculator, const void *data)
{
    PARAM_CHECK(calculator != NULL, return -1, "Invalid param");
    PARAM_CHECK(calculator->endIndex < calculator->dataNumber, return -1, "More data for calculator support");
    char *tmpData = (char *)calculator->data + calculator->dataUnit * calculator->endIndex;
    int ret = memcpy_s(tmpData, calculator->dataUnit, data, calculator->dataUnit);
    PARAM_CHECK(ret == EOK, return -1, "Failed to copy logic data");
    calculator->endIndex++;
    return 0;
}

static int CalculatorPop(LogicCalculator *calculator, void *data)
{
    PARAM_CHECK(calculator != NULL && data != NULL, return -1, "Invalid param");
    PARAM_CHECK(calculator->endIndex < calculator->dataNumber, return -1, "More data for calculator support");
    if (calculator->endIndex == 0) {
        return -1;
    }
    char *tmpData = (char *)calculator->data + calculator->dataUnit * (calculator->endIndex - 1);
    int ret = memcpy_s(data, calculator->dataUnit, tmpData, calculator->dataUnit);
    PARAM_CHECK(ret == EOK, return -1, "Failed to copy logic data");
    calculator->endIndex--;
    return 0;
}

static int CalculatorLength(const LogicCalculator *calculator)
{
    PARAM_CHECK(calculator != NULL, return 0, "Invalid param");
    return calculator->endIndex;
}

static int PrefixAdd(char *prefix, uint32_t *prefixIndex, uint32_t prefixLen, char op)
{
    if ((*prefixIndex + 1 + 1 + 1) >= prefixLen) {
        return -1;
    }
    prefix[(*prefixIndex)++] = ' ';
    prefix[(*prefixIndex)++] = op;
    prefix[(*prefixIndex)++] = ' ';
    return 0;
}

static int HandleOperationOr(LogicCalculator *calculator, char *prefix, uint32_t *prefixIndex, uint32_t prefixLen)
{
    char e;
    prefix[(*prefixIndex)++] = ' ';
    if (CalculatorLength(calculator) == 0) {
        CalculatorPushChar(calculator, '|');
    } else {
        do {
            CalculatorPopChar(calculator, &e);
            if (e == '(') {
                CalculatorPushChar(calculator, e);
            } else {
                int ret = PrefixAdd(prefix, prefixIndex, prefixLen, e);
                PARAM_CHECK(ret == 0, return -1, "Invalid prefix");
            }
        } while (CalculatorLength(calculator) > 0 && e != '(');
        CalculatorPushChar(calculator, '|');
    }
    return 0;
}

static int CompareValue(const char *condition, const char *value)
{
    if (strcmp(condition, "*") == 0) {
        return 1;
    }
    if (strcmp(condition, value) == 0) {
        return 1;
    }
    char *tmp = strstr(condition, "*");
    if (tmp != NULL && (strncmp(value, condition, tmp - condition) == 0)) {
        return 1;
    }
    return 0;
}

static int ComputeSubCondition(const LogicCalculator *calculator, LogicData *data, const char *condition)
{
    if (!LOGIC_DATA_TEST_FLAG(data, LOGIC_DATA_FLAGS_ORIGINAL)) {
        return LOGIC_DATA_TEST_FLAG(data, LOGIC_DATA_FLAGS_TRUE);
    }
    uint32_t triggerContentSize = strlen(calculator->triggerContent);
    // 解析条件, aaaa && bbb=1 && ccc=1的场景
    char *subStr = strstr(condition + data->startIndex, "=");
    if (subStr != NULL && ((uint32_t)(subStr - condition) > data->endIndex)) {
        if (strncmp(condition + data->startIndex, calculator->triggerContent, triggerContentSize) == 0) {
            return 1;
        }
        return 0;
    }
    int ret = GetValueFromContent(condition + data->startIndex,
        data->endIndex - data->startIndex, 0, calculator->conditionName, SUPPORT_DATA_BUFFER_MAX);
    PARAM_CHECK(ret == 0, return -1, "Failed parse content name");
    ret = GetValueFromContent(condition + data->startIndex, data->endIndex - data->startIndex,
        strlen(calculator->conditionName) + 1, calculator->conditionContent, SUPPORT_DATA_BUFFER_MAX);
    PARAM_CHECK(ret == 0, return -1, "Failed parse content value");
    // check name
    if ((calculator->inputName != NULL) && (strcmp(calculator->conditionName, calculator->inputName) == 0)) {
        return CompareValue(calculator->conditionContent, calculator->inputContent);
    } else if (strlen(calculator->conditionName) > 0) {
        uint32_t len = SUPPORT_DATA_BUFFER_MAX;
        ret = SystemReadParam(calculator->conditionName, calculator->readContent, &len);
        if (ret != 0) {
            return 0;
        }
        return CompareValue(calculator->conditionContent, calculator->readContent);
    }
    return 0;
}

int GetValueFromContent(const char *content, uint32_t contentSize, uint32_t start, char *value, uint32_t valueSize)
{
    uint32_t contentIndex = start;
    uint32_t currIndex = 0;
    while (contentIndex < contentSize && currIndex < valueSize) {
        if (content[contentIndex] == '=') {
            value[currIndex++] = '\0';
            return 0;
        }
        value[currIndex++] = content[contentIndex++];
    }
    if (currIndex < valueSize) {
        value[currIndex] = '\0';
        return 0;
    }
    return -1;
}

int ComputeCondition(LogicCalculator *calculator, const char *condition)
{
    PARAM_CHECK(calculator != NULL && condition != NULL, return -1, "Invalid calculator");
    uint32_t currIndex = 0;
    uint32_t start = 0;
    size_t conditionLen = strlen(condition);
    int noneOper = 1;
    CalculatorClear(calculator);
    LogicData data1 = {};
    LogicData data2 = {};
    while (currIndex < conditionLen) {
        if (condition[currIndex] == '|' || condition[currIndex] == '&') {
            noneOper = 0;
            int ret = CalculatorPop(calculator, (void*)&data2);
            int ret1 = CalculatorPop(calculator, (void*)&data1);
            PARAM_CHECK((ret == 0 && ret1 == 0), return -1, "Failed to pop data");

            ret = ComputeSubCondition(calculator, &data1, condition);
            data1.flags = 0;
            if (condition[currIndex] == '|' && ret == 1) {
                LOGIC_DATA_SET_FLAG(&data1, LOGIC_DATA_FLAGS_TRUE);
            } else if ((condition[currIndex] == '|' || ret == 1) &&
                (ComputeSubCondition(calculator, &data2, condition) == 1)) {
                LOGIC_DATA_SET_FLAG(&data1, LOGIC_DATA_FLAGS_TRUE);
            }
            ret = CalculatorPush(calculator, (void *)&data1);
            PARAM_CHECK(ret == 0, return -1, "Failed to push data");
            start = currIndex + 1; // 跳过符号
        } else if (isspace(condition[currIndex])) {
            if (start == currIndex) {
                start = ++currIndex;
                continue;
            }
            data1.flags = LOGIC_DATA_FLAGS_ORIGINAL;
            data1.startIndex = start;
            data1.endIndex = currIndex;
            int ret = CalculatorPush(calculator, (void *)&data1);
            PARAM_CHECK(ret == 0, return -1, "Failed to push data");
            start = currIndex + 1;
        }
        currIndex++;
    }
    if (noneOper) {
        data1.flags = LOGIC_DATA_FLAGS_ORIGINAL;
        data1.startIndex = start;
        data1.endIndex = strlen(condition);
    } else {
        int ret = CalculatorPop(calculator, &data1);
        PARAM_CHECK(ret == 0, return -1, "Invalid calculator");
    }
    return ComputeSubCondition(calculator, &data1, condition);
}

int ConvertInfixToPrefix(const char *condition, char *prefix, uint32_t prefixLen)
{
    PARAM_CHECK(condition != NULL && prefix != NULL, return -1, "Invalid condition");
    char e = 0;
    int ret;
    uint32_t curr = 0;
    uint32_t prefixIndex = 0;
    size_t conditionLen = strlen(condition);
    LogicCalculator calculator;
    PARAM_CHECK(CalculatorInit(&calculator, MAX_CALC_PARAM, 1, 0) == 0, return -1, "Failed to init calculator");

    while (curr < conditionLen) {
        if (condition[curr] == ')') {
            CalculatorPopChar(&calculator, &e);
            while (e != '(') {
                ret = PrefixAdd(prefix, &prefixIndex, prefixLen, e);
                PARAM_CHECK(ret == 0,
                    CalculatorFree(&calculator); return -1, "Invalid prefix");
                CalculatorPopChar(&calculator, &e);
            }
        } else if (condition[curr] == '|') {
            PARAM_CHECK(condition[curr + 1] == '|',
                CalculatorFree(&calculator); return -1, "Invalid condition");
            ret = HandleOperationOr(&calculator, prefix, &prefixIndex, prefixLen);
            PARAM_CHECK(ret == 0,
                CalculatorFree(&calculator); return -1, "Invalid prefix");
            curr++;
        } else if (condition[curr] == '&') {
            PARAM_CHECK(condition[curr + 1] == '&',
                CalculatorFree(&calculator); return -1, "Invalid condition");
            prefix[prefixIndex++] = ' ';
            CalculatorPushChar(&calculator, condition[curr]);
            curr++;
        } else if (condition[curr] == '(') {
            CalculatorPushChar(&calculator, condition[curr]);
        } else {
            prefix[prefixIndex++] = condition[curr];
        }
        curr++;
        PARAM_CHECK(prefixIndex < prefixLen,
            CalculatorFree(&calculator); return -1, "Invalid prefixIndex");
    }

    while (CalculatorLength(&calculator) > 0) {
        CalculatorPopChar(&calculator, &e);
        ret = PrefixAdd(prefix, &prefixIndex, prefixLen, e);
        PARAM_CHECK(ret == 0,
            CalculatorFree(&calculator);
            return -1, "Invalid prefix %u %u", prefixIndex, prefixLen);
    }
    prefix[prefixIndex] = '\0';
    CalculatorFree(&calculator);
    return 0;
}

int CheckMatchSubCondition(const char *condition, const char *input, int length)
{
    PARAM_CHECK(condition != NULL, return 0, "Invalid condition");
    PARAM_CHECK(input != NULL, return 0, "Invalid input");
    const char *tmp = strstr(condition, input);
    while (tmp != NULL) {
        PARAM_LOGV("CheckMatchSubCondition Condition: '%s' content: '%s' length %d", condition, input, length);
        if (((int)strlen(tmp) <= length) || (tmp[length] != '=')) {
            return 0;
        }
        // for condition: parameter = 1
        if (tmp == condition) {
            return 1;
        }
        // for condition: parameter1 = 1 && parameter2 = 1
        if (*(tmp - 1) == ' ') {
            return 1;
        }
        tmp = strstr(tmp + 1, input);
    }
    return 0;
}