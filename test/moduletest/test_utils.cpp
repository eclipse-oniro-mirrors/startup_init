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

#include "test_utils.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <list>
#include <thread>
#include <memory>
#include <fcntl.h>
#include <cerrno>
#include <sys/stat.h>

#include "parameters.h"
#include "service_control.h"

namespace initModuleTest {
namespace {
constexpr size_t MAX_BUFFER_SIZE = 4096;
}

// File operator
int ReadFileContent(const std::string &fileName, std::string &content)
{
    content.clear();
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen(fileName.c_str(), "r"), fclose);
    if (fp == nullptr) {
        std::cout << "Cannot open file " << fileName << std::endl;
        return -1;
    }

    struct stat st {};
    if (stat(fileName.c_str(), &st) < 0) {
        std::cout <<  "Cannot get " << fileName << " stat\n";
        return -1;
    }

    ssize_t n = 0;
    char buffer[MAX_BUFFER_SIZE] = {};
    while ((n = fread(buffer, 1, MAX_BUFFER_SIZE, fp.get())) > 0) {
            content.append(buffer, n);
    }
    return feof(fp.get()) ? 0 : -1;
}

bool StartsWith(const std::string &str, const std::string &prefix)
{
    return ((str.size() > prefix.size()) && (str.substr(0, prefix.size()) == prefix));
}

bool EndsWith(const std::string &str, const std::string &suffix)
{
    return ((str.size() > suffix.size()) && (str.substr(str.size() - suffix.size(), suffix.size()) == suffix));
}

std::string Trim(const std::string &str)
{
    size_t start = 0;
    size_t end = str.size() - 1;

    while (start < str.size()) {
        if (!isspace(str[start])) {
            break;
        }
        start++;
    }

    while (start < end) {
        if (!isspace(str[end])) {
            break;
        }
        end--;
    }

    if (end < start) {
        return "";
    }

    return str.substr(start, end - start + 1);
}

std::vector<std::string> Split(const std::string &str, const std::string &pattern)
{
    std::vector<std::string> result {};
    size_t pos = std::string::npos;
    size_t start = 0;
    while (true) {
        pos = str.find_first_of(pattern, start);
        result.push_back(str.substr(start, pos - start));
        if (pos == std::string::npos) {
            break;
        }
        start = pos + 1;
    }
    return result;
}

static std::map<ServiceStatus, std::string> g_serviceStatusMap = {
    { SERVICE_IDLE, "idle"},
    { SERVICE_STARTING, "starting"},
    { SERVICE_STARTED, "running"},
    { SERVICE_READY, "ready"},
    { SERVICE_STOPPING, "stopping"},
    { SERVICE_STOPPED, "stopped"},
    { SERVICE_ERROR, "error" },
    { SERVICE_SUSPENDED, "suspended" },
    { SERVICE_FREEZED, "freezed" },
    { SERVICE_DISABLED, "disabled" },
    { SERVICE_CRITICAL, "critical" },
};

static inline bool ValidStatus(ServiceStatus status)
{
    return status >= SERVICE_IDLE && status <= SERVICE_CRITICAL;
}

std::string GetServiceStatus(const std::string &serviceName)
{
    if (serviceName.empty()) {
        return "";
    }
    const std::string serviceCtlPrefix = "startup.service.ctl.";
    const std::string serviceCtlName = serviceCtlPrefix + serviceName;
    uint32_t ret = OHOS::system::GetUintParameter<uint32_t>(serviceCtlName, 0);
    ServiceStatus status = static_cast<ServiceStatus>(ret);
    if (!ValidStatus(status)) {
        return "";
    }
    return g_serviceStatusMap[status];
}

} // initModuleTest
