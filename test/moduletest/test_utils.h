/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef INIT_MODULE_TEST_UTILS_H
#define INIT_MODULE_TEST_UTILS_H
#include <string>
#include <vector>

namespace initModuleTest {
// File operator
int ReadFileContent(const std::string &fileName, std::string &content);
std::string Trim(const std::string &str);
std::vector<std::string> Split(const std::string &str, const std::string &pattern);
std::string GetServiceStatus(const std::string &serviceName);
} // initModuleTest

#endif // INIT_MODULE_TEST_UTILS_H
