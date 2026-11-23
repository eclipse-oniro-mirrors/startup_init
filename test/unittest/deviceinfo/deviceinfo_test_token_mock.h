/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef DEVICEINFO_TEST_TOKEN_MOCK_H
#define DEVICEINFO_TEST_TOKEN_MOCK_H

// g_tokenVerifyResult is defined in DeviceInfoUnittest.cpp together with the
// AccessTokenKit::VerifyAccessToken mock (which returns this value). The
// deviceinfo unit-test cases set it to PERMISSION_GRANTED / PERMISSION_DENIED
// to drive CheckPermission's outcome in the test process.
//
// Unit-test sources must include this header instead of writing a bare
// `extern int g_tokenVerifyResult;` declaration (G.EXP.05-CPP: do not reference
// external variables by using a declaration in a source file).

extern int g_tokenVerifyResult;

#endif  // DEVICEINFO_TEST_TOKEN_MOCK_H
