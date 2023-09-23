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

#include <cinttypes>
#include <gtest/gtest.h>
#include "init_utils.h"

using namespace testing::ext;
using namespace std;

namespace init_ut {
class StrUtilUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(StrUtilUnitTest, StrArrayGetIndex_unit_test, TestSize.Level1)
{
    int ret;
    const char *strArray[] = { "a1", "a2", "a3", NULL};

    // Invalid arguments test
    ret = OH_StrArrayGetIndex(NULL, "test", 0);
    EXPECT_EQ(ret, -1);
    ret = OH_StrArrayGetIndex(NULL, NULL, 0);
    EXPECT_EQ(ret, -1);
    ret = OH_StrArrayGetIndex(strArray, NULL, 0);
    EXPECT_EQ(ret, -1);

    // Matched
    ret = OH_StrArrayGetIndex(strArray, "a1", 0);
    EXPECT_EQ(ret, 0);
    ret = OH_StrArrayGetIndex(strArray, "a2", 0);
    EXPECT_EQ(ret, 1);
    ret = OH_StrArrayGetIndex(strArray, "a3", 0);
    EXPECT_EQ(ret, 2);

    // Not matched
    ret = OH_StrArrayGetIndex(strArray, "aa1", 0);
    EXPECT_EQ(ret, -1);
    ret = OH_StrArrayGetIndex(strArray, "A1", 0);
    EXPECT_EQ(ret, -1);
    ret = OH_StrArrayGetIndex(strArray, "A2", 0);
    EXPECT_EQ(ret, -1);
    ret = OH_StrArrayGetIndex(strArray, "A3", 0);
    EXPECT_EQ(ret, -1);

    // Ignore case
    ret = OH_StrArrayGetIndex(strArray, "A1", 1);
    EXPECT_EQ(ret, 0);
    ret = OH_StrArrayGetIndex(strArray, "A2", 2);
    EXPECT_EQ(ret, 1);
    ret = OH_StrArrayGetIndex(strArray, "A3", 3);
    EXPECT_EQ(ret, 2);
}

HWTEST_F(StrUtilUnitTest, OH_ExtendableStrArrayGetIndex_unitest, TestSize.Level1)
{
    int ret;
    const char *strArray[] = { "a1", "a2", "a3", NULL};
    const char *extendStrArray[] = { "a4", "a5", "a6", NULL};

    // Matched
    ret = OH_ExtendableStrArrayGetIndex(strArray, "a4", 0, extendStrArray);
    EXPECT_EQ(ret, 3);
    ret = OH_ExtendableStrArrayGetIndex(strArray, "a5", 0, extendStrArray);
    EXPECT_EQ(ret, 4);
    ret = OH_ExtendableStrArrayGetIndex(strArray, "a6", 0, extendStrArray);
    EXPECT_EQ(ret, 5);

    // Not matched
    ret = OH_ExtendableStrArrayGetIndex(strArray, "aa1", 0, extendStrArray);
    EXPECT_EQ(ret, -1);
    ret = OH_ExtendableStrArrayGetIndex(strArray, "A4", 0, extendStrArray);
    EXPECT_EQ(ret, -1);
    ret = OH_ExtendableStrArrayGetIndex(strArray, "A5", 0, extendStrArray);
    EXPECT_EQ(ret, -1);
    ret = OH_ExtendableStrArrayGetIndex(strArray, "A6", 0, extendStrArray);
    EXPECT_EQ(ret, -1);

    // Ignore case
    ret = OH_ExtendableStrArrayGetIndex(strArray, "A4", 1, extendStrArray);
    EXPECT_EQ(ret, 3);
    ret = OH_ExtendableStrArrayGetIndex(strArray, "A5", 2, extendStrArray);
    EXPECT_EQ(ret, 4);
    ret = OH_ExtendableStrArrayGetIndex(strArray, "A6", 3, extendStrArray);
    EXPECT_EQ(ret, 5);
}

HWTEST_F(StrUtilUnitTest, StrDictGet_unit_test_str_array, TestSize.Level1)
{
    int ret;
    void *res;
    const char *strArray[] = { "a1", "a2", "a3", NULL};

    // Invalid arguments test
    res = OH_StrDictGet(NULL, 0, "test", 0);
    EXPECT_EQ(res, NULL);
    res = OH_StrDictGet(NULL, 0, NULL, 0);
    EXPECT_EQ(res, NULL);
    res = OH_StrDictGet((void **)strArray, 1, NULL, 0);
    EXPECT_EQ(res, NULL);
    res = OH_StrDictGet((void **)strArray, 3, NULL, 0);
    EXPECT_EQ(res, NULL);

    // Matched
    res = OH_StrDictGet((void **)strArray, sizeof(const char *), "a1", 0);
    ASSERT_NE(res, nullptr);
    ret = strcmp(*(const char **)res, "a1");
    EXPECT_EQ(ret, 0);
    res = OH_StrDictGet((void **)strArray, sizeof(const char *), "a2", 0);
    ASSERT_NE(res, nullptr);
    ret = strcmp(*(const char **)res, "a2");
    EXPECT_EQ(ret, 0);
    res = OH_StrDictGet((void **)strArray, sizeof(const char *), "a3", 0);
    ASSERT_NE(res, nullptr);
    ret = strcmp(*(const char **)res, "a3");
    EXPECT_EQ(ret, 0);

    // Not matched
    res = OH_StrDictGet((void **)strArray, sizeof(const char *), "aa1", 0);
    EXPECT_EQ(res, nullptr);
    res = OH_StrDictGet((void **)strArray, sizeof(const char *), "A1", 0);
    EXPECT_EQ(res, nullptr);
    res = OH_StrDictGet((void **)strArray, sizeof(const char *), "A2", 0);
    EXPECT_EQ(res, nullptr);
    res = OH_StrDictGet((void **)strArray, sizeof(const char *), "A3", 0);
    EXPECT_EQ(res, nullptr);

    // Ignore case
    res = OH_StrDictGet((void **)strArray, sizeof(const char *), "A1", 1);
    ASSERT_NE(res, nullptr);
    ret = strcmp(*(const char **)res, "a1");
    EXPECT_EQ(ret, 0);
    res = OH_StrDictGet((void **)strArray, sizeof(const char *), "A2", 2);
    ASSERT_NE(res, nullptr);
    ret = strcmp(*(const char **)res, "a2");
    EXPECT_EQ(ret, 0);
    res = OH_StrDictGet((void **)strArray, sizeof(const char *), "A3", 3);
    ASSERT_NE(res, nullptr);
    ret = strcmp(*(const char **)res, "a3");
    EXPECT_EQ(ret, 0);
}

using STRING_INT_DICT = struct {
    const char *key;
    int val;
};

HWTEST_F(StrUtilUnitTest, StrDictGet_unit_test_str_int_dict, TestSize.Level1)
{
    const STRING_INT_DICT *res;
    const STRING_INT_DICT strIntDict[] = {
        {"a1", 1},
        {"a2", 2},
        {"a3", 3},
        {NULL, 0}
    };

    // Matched
    res = (const STRING_INT_DICT *)OH_StrDictGet((void **)strIntDict, sizeof(STRING_INT_DICT), "a1", 0);
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->val, 1);
    res = (const STRING_INT_DICT *)OH_StrDictGet((void **)strIntDict, sizeof(STRING_INT_DICT), "a2", 0);
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->val, 2);
    res = (const STRING_INT_DICT *)OH_StrDictGet((void **)strIntDict, sizeof(STRING_INT_DICT), "a3", 0);
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->val, 3);

    // Not matched
    res = (const STRING_INT_DICT *)OH_StrDictGet((void **)strIntDict, sizeof(STRING_INT_DICT), "aa1", 0);
    EXPECT_EQ(res, nullptr);
    res = (const STRING_INT_DICT *)OH_StrDictGet((void **)strIntDict, sizeof(STRING_INT_DICT), "A1", 0);
    EXPECT_EQ(res, nullptr);
    res = (const STRING_INT_DICT *)OH_StrDictGet((void **)strIntDict, sizeof(STRING_INT_DICT), "A2", 0);
    EXPECT_EQ(res, nullptr);
    res = (const STRING_INT_DICT *)OH_StrDictGet((void **)strIntDict, sizeof(STRING_INT_DICT), "A3", 0);
    EXPECT_EQ(res, nullptr);

    // Ignore case
    res = (const STRING_INT_DICT *)OH_StrDictGet((void **)strIntDict, sizeof(STRING_INT_DICT), "A1", 3);
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->val, 1);
    res = (const STRING_INT_DICT *)OH_StrDictGet((void **)strIntDict, sizeof(STRING_INT_DICT), "A2", 2);
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->val, 2);
    res = (const STRING_INT_DICT *)OH_StrDictGet((void **)strIntDict, sizeof(STRING_INT_DICT), "A3", 1);
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->val, 3);
}

using STRING_STRING_DICT = struct {
    const char *key;
    const char *val;
};

HWTEST_F(StrUtilUnitTest, StrDictGet_unit_test_str_str_dict, TestSize.Level1)
{
    int ret;
    const STRING_STRING_DICT *res;
    const STRING_STRING_DICT strStrDict[] = {
        {"a1", "val1"},
        {"a2", "val2"},
        {"a3", "val3"},
        {NULL, NULL}
    };

    // Matched
    res = (const STRING_STRING_DICT *)OH_StrDictGet((void **)strStrDict,
        sizeof(STRING_STRING_DICT), "a1", 0);
    ASSERT_NE(res, nullptr);
    ret = strcmp(res->val, "val1");
    EXPECT_EQ(ret, 0);
    res = (const STRING_STRING_DICT *)OH_StrDictGet((void **)strStrDict,
        sizeof(STRING_STRING_DICT), "a2", 0);
    ASSERT_NE(res, nullptr);
    ret = strcmp(res->val, "val2");
    EXPECT_EQ(ret, 0);
    res = (const STRING_STRING_DICT *)OH_StrDictGet((void **)strStrDict,
        sizeof(STRING_STRING_DICT), "a3", 0);
    ASSERT_NE(res, nullptr);
    ret = strcmp(res->val, "val3");
    EXPECT_EQ(ret, 0);

    // Not matched
    res = (const STRING_STRING_DICT *)OH_StrDictGet((void **)strStrDict,
        sizeof(STRING_STRING_DICT), "aa1", 0);
    EXPECT_EQ(res, nullptr);
    res = (const STRING_STRING_DICT *)OH_StrDictGet((void **)strStrDict,
        sizeof(STRING_STRING_DICT), "A1", 0);
    EXPECT_EQ(res, nullptr);
    res = (const STRING_STRING_DICT *)OH_StrDictGet((void **)strStrDict,
        sizeof(STRING_STRING_DICT), "A2", 0);
    EXPECT_EQ(res, nullptr);
    res = (const STRING_STRING_DICT *)OH_StrDictGet((void **)strStrDict,
        sizeof(STRING_STRING_DICT), "A3", 0);
    EXPECT_EQ(res, nullptr);

    // Ignore case
    res = (const STRING_STRING_DICT *)OH_StrDictGet((void **)strStrDict,
        sizeof(STRING_STRING_DICT), "A1", 3);
    ASSERT_NE(res, nullptr);
    ret = strcmp(res->val, "val1");
    EXPECT_EQ(ret, 0);
    res = (const STRING_STRING_DICT *)OH_StrDictGet((void **)strStrDict,
        sizeof(STRING_STRING_DICT), "A2", 2);
    ASSERT_NE(res, nullptr);
    ret = strcmp(res->val, "val2");
    EXPECT_EQ(ret, 0);
    res = (const STRING_STRING_DICT *)OH_StrDictGet((void **)strStrDict,
        sizeof(STRING_STRING_DICT), "A3", 1);
    ASSERT_NE(res, nullptr);
    ret = strcmp(res->val, "val3");
    EXPECT_EQ(ret, 0);
}

using STRING_HYBRID_DICT = struct {
    const char *key;
    int cnt;
    const char *val;
};

HWTEST_F(StrUtilUnitTest, StrDictGet_unit_test_str_hybrid_dict, TestSize.Level1)
{
    int ret;
    const STRING_HYBRID_DICT *res;
    const STRING_HYBRID_DICT strHybridDict[] = {
        {"a1", 1, "val1"},
        {"a2", 2, "val2"},
        {"a3", 3, "val3"},
        {NULL, 0, NULL}
    };

    // string array Matched
    res = (const STRING_HYBRID_DICT *)OH_StrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "a1", 0);
    ASSERT_NE(res, nullptr);
    ret = strcmp(res->val, "val1");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(res->cnt, 1);
    res = (const STRING_HYBRID_DICT *)OH_StrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "a2", 0);
    ASSERT_NE(res, nullptr);
    ret = strcmp(res->val, "val2");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(res->cnt, 2);
    res = (const STRING_HYBRID_DICT *)OH_StrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "a3", 0);
    ASSERT_NE(res, nullptr);
    ret = strcmp(res->val, "val3");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(res->cnt, 3);

    // Not matched
    res = (const STRING_HYBRID_DICT *)OH_StrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "aa1", 0);
    EXPECT_EQ(res, nullptr);
    res = (const STRING_HYBRID_DICT *)OH_StrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "A1", 0);
    EXPECT_EQ(res, nullptr);
    res = (const STRING_HYBRID_DICT *)OH_StrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "A2", 0);
    EXPECT_EQ(res, nullptr);
    res = (const STRING_HYBRID_DICT *)OH_StrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "A3", 0);
    EXPECT_EQ(res, nullptr);

    // Ignore case
    res = (const STRING_HYBRID_DICT *)OH_StrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "A1", 3);
    ASSERT_NE(res, nullptr);
    ret = strcmp(res->val, "val1");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(res->cnt, 1);
    res = (const STRING_HYBRID_DICT *)OH_StrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "A2", 2);
    ASSERT_NE(res, nullptr);
    ret = strcmp(res->val, "val2");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(res->cnt, 2);
    res = (const STRING_HYBRID_DICT *)OH_StrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "A3", 1);
    ASSERT_NE(res, nullptr);
    ret = strcmp(res->val, "val3");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(res->cnt, 3);
}

HWTEST_F(StrUtilUnitTest, ExtendableStrDictGet_unit_test, TestSize.Level1)
{
    int ret;
    const STRING_HYBRID_DICT *res;
    const STRING_HYBRID_DICT strHybridDict[] = {
        {"a1", 1, "val1"},
        {"a2", 2, "val2"},
        {"a3", 3, "val3"},
        {NULL, 0, NULL}
    };
    const STRING_HYBRID_DICT extendHybridDict[] = {
        {"a4", 4, "val4"},
        {"a5", 5, "val5"},
        {"a6", 6, "val6"},
        {NULL, 0, NULL}
    };

    // string array Matched
    res = (const STRING_HYBRID_DICT *)OH_ExtendableStrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "a4", 0, (void **)extendHybridDict);
    ASSERT_NE(res, nullptr);
    ret = strcmp(res->val, "val4");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(res->cnt, 4);
    res = (const STRING_HYBRID_DICT *)OH_ExtendableStrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "a5", 0, (void **)extendHybridDict);
    ASSERT_NE(res, nullptr);
    ret = strcmp(res->val, "val5");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(res->cnt, 5);
    res = (const STRING_HYBRID_DICT *)OH_ExtendableStrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "a6", 0, (void **)extendHybridDict);
    ASSERT_NE(res, nullptr);
    ret = strcmp(res->val, "val6");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(res->cnt, 6);

    // Not matched
    res = (const STRING_HYBRID_DICT *)OH_ExtendableStrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "aa1", 0, (void **)extendHybridDict);
    EXPECT_EQ(res, nullptr);
    res = (const STRING_HYBRID_DICT *)OH_ExtendableStrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "A4", 0, (void **)extendHybridDict);
    EXPECT_EQ(res, nullptr);
    res = (const STRING_HYBRID_DICT *)OH_ExtendableStrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "A5", 0, (void **)extendHybridDict);
    EXPECT_EQ(res, nullptr);
    res = (const STRING_HYBRID_DICT *)OH_ExtendableStrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "A6", 0, (void **)extendHybridDict);
    EXPECT_EQ(res, nullptr);

    // Ignore case
    res = (const STRING_HYBRID_DICT *)OH_ExtendableStrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "A4", 3, (void **)extendHybridDict);
    ASSERT_NE(res, nullptr);
    ret = strcmp(res->val, "val4");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(res->cnt, 4);
    res = (const STRING_HYBRID_DICT *)OH_ExtendableStrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "A5", 2, (void **)extendHybridDict);
    ASSERT_NE(res, nullptr);
    ret = strcmp(res->val, "val5");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(res->cnt, 5);
    res = (const STRING_HYBRID_DICT *)OH_ExtendableStrDictGet((void **)strHybridDict,
        sizeof(STRING_HYBRID_DICT), "A6", 1, (void **)extendHybridDict);
    ASSERT_NE(res, nullptr);
    ret = strcmp(res->val, "val6");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(res->cnt, 6);
}
} // namespace init_ut
