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

#ifndef STARTUP_TRIGER_CHECKER_H
#define STARTUP_TRIGER_CHECKER_H
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define MAX_TRIGGER_CMD_NAME_LEN 32
#define MAX_TRIGGER_NAME_LEN 128
#define MAX_TRIGGER_TYPE_LEN 16

#define SUPPORT_DATA_BUFFER_MAX 128
#define MAX_DATA_BUFFER_MAX (SUPPORT_DATA_BUFFER_MAX * 5)
#define CONDITION_EXTEND_LEN 32

#define LOGIC_DATA_FLAGS_ORIGINAL 0x1
#define LOGIC_DATA_FLAGS_TRUE 0x08

#define LOGIC_DATA_TEST_FLAG(data, flag) (((data)->flags & flag) == (flag))
#define LOGIC_DATA_SET_FLAG(data, flag) (data)->flags |= (flag)
#define LOGIC_DATA_CLEAR_FLAG(data, flag) (data)->flags &= ~(flag)

typedef struct {
    uint32_t flags;
    uint32_t startIndex;
    uint32_t endIndex;
} LogicData;

struct tagTriggerNode_;
typedef int (*PARAM_CHECK_DONE)(struct tagTriggerNode_ *trigger, const char *content, uint32_t size);
typedef struct {
    char triggerContent[MAX_TRIGGER_NAME_LEN];
    PARAM_CHECK_DONE triggerCheckDone;
    int dataNumber;
    int endIndex;
    int dataUnit;
    char *conditionName;
    char *conditionContent;
    char *inputName;
    char *inputContent;
    char *readContent;
    char *data;
} LogicCalculator;

int CalculatorInit(LogicCalculator *calculator, int dataNumber, int dataUnit, int needCondition);
void CalculatorFree(LogicCalculator *calculator);
int ConvertInfixToPrefix(const char *condition, char *prefix, uint32_t prefixLen);
int ComputeCondition(LogicCalculator *calculator, const char *condition);
int GetValueFromContent(const char *content, uint32_t contentSize, uint32_t start, char *value, uint32_t valueSize);
int CheckMatchSubCondition(const char *condition, const char *input, int length);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif // STARTUP_TRIGER_CHECKER_H