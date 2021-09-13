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
#include "watcher_manager.h"
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <thread>

#include "param_message.h"
#include "sys_param.h"
#include "system_ability_definition.h"
#include "watcher_utils.h"

namespace OHOS {
namespace init_param {
REGISTER_SYSTEM_ABILITY_BY_ID(WatcherManager, PARAM_WATCHER_DISTRIBUTED_SERVICE_ID, true)

const static int32_t INVALID_SOCKET = -1;
const static int32_t RECV_BUFFER_MAX = 5 * 1024;
const static int32_t SLEEP_TIME = 2000;
uint32_t WatcherManager::AddWatcher(const std::string &keyPrefix, const sptr<IWatcher> &watcher)
{
    WATCHER_CHECK(watcher != nullptr, return 0, "Invalid remove watcher for %s", keyPrefix.c_str());
    if (watcherId_ == 0) {
        watcherId_++;
    }
    ParamWatcherPtr paramWather = std::make_shared<ParamWatcher>(watcherId_, watcher);
    WATCHER_CHECK(paramWather != nullptr, return 0, "Failed to create watcher for %s", keyPrefix.c_str());
    WatcherGroupPtr group = GetWatcherGroup(keyPrefix);
    if (group == nullptr) {
        group = std::make_shared<WatcherGroup>(++groupId_, keyPrefix);
    }
    WATCHER_CHECK(group != nullptr, return 0, "Failed to create group for %s", keyPrefix.c_str());
    AddWatcherGroup(keyPrefix, group);

    if (group->Emptry()) {
        StartLoop();
        SendMessage(group, MSG_ADD_WATCHER);
    }
    SendLocalChange(keyPrefix, paramWather);
    group->AddWatcher(paramWather);
    WATCHER_LOGD("AddWatcher %s watcherId: %u", keyPrefix.c_str(), paramWather->GetWatcherId());
    return paramWather->GetWatcherId();
}

int32_t WatcherManager::DelWatcher(const std::string &keyPrefix, uint32_t watcherId)
{
    auto group = GetWatcherGroup(keyPrefix);
    if (group == nullptr) {
        WATCHER_LOGE("DelWatcher can not find group %s", keyPrefix.c_str());
        return 0;
    }
    group->DelWatcher(watcherId);
    if (group->Emptry()) {
        SendMessage(group, MSG_DEL_WATCHER);
        DelWatcherGroup(group);
    }
    return 0;
}

int WatcherManager::SendMessage(WatcherGroupPtr group, int type)
{
    ParamMessage *request = nullptr;
    request = (ParamMessage *)CreateParamMessage(type, group->GetKeyPrefix().c_str(), sizeof(ParamMessage));
    PARAM_CHECK(request != NULL, return PARAM_CODE_ERROR, "Failed to malloc for watch");
    request->id.watcherId = group->GetGroupId();
    request->msgSize = sizeof(ParamMessage);
    do {
        int fd = GetServerFd(false);
        if (fd != INVALID_SOCKET) {
            ssize_t sendLen = send(serverFd_, (char *)request, request->msgSize, 0);
            if (sendLen > 0) {
                PARAM_LOGD("SendMessage key: %s %d success", group->GetKeyPrefix().c_str(), type);
                break;
            }
        }
        // fail, try again
        PARAM_LOGE("Failed to connect server %s, try again", PIPE_NAME);
        fd = GetServerFd(true);
    } while (1);
    free(request);
    return 0;
}

void WatcherManager::ProcessWatcherMessage(const char *buffer, uint32_t dataSize)
{
    ParamMessage *msg = (ParamMessage *)buffer;
    uint32_t offset = 0;
    if (msg->type != MSG_NOTIFY_PARAM) {
        return;
    }
    PARAM_CHECK(msg->msgSize <= dataSize, return, "Invalid msg size %d", msg->msgSize);
    ParamMsgContent *valueContent = GetNextContent((const ParamMessage *)msg, &offset);
    PARAM_CHECK(valueContent != NULL, return, "Invalid msg ");
    PARAM_LOGI("ProcessWatcherMessage name %s watcherId %u ", msg->key, msg->id.watcherId);

    WatcherGroupPtr group = GetWatcherGroup(msg->id.watcherId);
    if (group != nullptr) {
        group->ProcessParameterChange(msg->key, valueContent->content);
    }
}

WatcherManager::WatcherGroupPtr WatcherManager::GetWatcherGroup(uint32_t groupId)
{
    std::lock_guard<std::mutex> lock(watcherMutex_);
    if (watcherGroups_.find(groupId) != watcherGroups_.end()) {
        return watcherGroups_[groupId];
    }
    return nullptr;
}

WatcherManager::WatcherGroupPtr WatcherManager::GetWatcherGroup(const std::string &keyPrefix)
{
    if (groupMap_.find(keyPrefix) == groupMap_.end()) {
        return nullptr;
    }
    return GetWatcherGroup(groupMap_[keyPrefix]);
}

void WatcherManager::AddWatcherGroup(const std::string &keyPrefix, WatcherGroupPtr group)
{
    uint32_t groupId = group->GetGroupId();
    groupMap_[keyPrefix] = groupId;

    std::lock_guard<std::mutex> lock(watcherMutex_);
    if (watcherGroups_.find(groupId) != watcherGroups_.end()) {
        return;
    }
    watcherGroups_[groupId] = group;
}

void WatcherManager::DelWatcherGroup(WatcherGroupPtr group)
{
    groupMap_[group->GetKeyPrefix()] = 0;
    std::lock_guard<std::mutex> lock(watcherMutex_);
    watcherGroups_[group->GetGroupId()] = nullptr;
}

void WatcherManager::WatcherGroup::AddWatcher(const ParamWatcherPtr &watcher)
{
    watchers_[watcher->GetWatcherId()] = watcher;
}

void WatcherManager::WatcherGroup::DelWatcher(uint32_t watcherId)
{
    if (watchers_.find(watcherId) != watchers_.end()) {
        watchers_[watcherId] = nullptr;
    }
}

WatcherManager::ParamWatcherPtr WatcherManager::WatcherGroup::GetWatcher(uint32_t watcherId)
{
    if (watchers_.find(watcherId) != watchers_.end()) {
        return watchers_[watcherId];
    }
    return nullptr;
}

void WatcherManager::WatcherGroup::ProcessParameterChange(const std::string &name, const std::string &value)
{
    // walk watcher
    for (auto iter = watchers_.begin(); iter != watchers_.end(); iter++) {
        iter->second->ProcessParameterChange(name, value);
    }
}

static int FilterParam(const char *name, const std::string &keyPrefix)
{
    if (keyPrefix.rfind("*") == keyPrefix.length() - 1) {
        return strncmp(name, keyPrefix.c_str(), keyPrefix.length() - 1) == 0;
    }
    return strcmp(name, keyPrefix.c_str()) == 0;
}

void WatcherManager::SendLocalChange(const std::string &keyPrefix, ParamWatcherPtr watcher)
{
    struct Context {
        char *buffer;
        ParamWatcherPtr watcher;
        std::string keyPrefix;
    };
    std::vector<char> buffer(PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX);
    struct Context context = {buffer.data(), watcher, keyPrefix};
    // walk watcher
    SystemTraversalParameter(
        [](ParamHandle handle, void *cookie) {
            struct Context *context = (struct Context *)(cookie);
            SystemGetParameterName(handle, context->buffer, PARAM_NAME_LEN_MAX);
            if (!FilterParam(context->buffer, context->keyPrefix)) {
                return;
            }
            uint32_t size = PARAM_CONST_VALUE_LEN_MAX;
            SystemGetParameterValue(handle, context->buffer + PARAM_NAME_LEN_MAX, &size);
            PARAM_LOGD("SendLocalChange key %s value: %s ", context->buffer, context->buffer + PARAM_NAME_LEN_MAX);
            context->watcher->ProcessParameterChange(context->buffer, context->buffer + PARAM_NAME_LEN_MAX);
        }, (void *)&context);
}

void WatcherManager::RunLoop()
{
    char *buffer = (char *)malloc(RECV_BUFFER_MAX);
    PARAM_CHECK(buffer != NULL, return, "Failed to create buffer for recv");
    while (!stop) {
        int fd = GetServerFd(false);
        ssize_t recvLen = recv(fd, buffer, RECV_BUFFER_MAX, 0);
        if (recvLen <= 0) {
            if (errno == EAGAIN) { // 超时，继续等待
                continue;
            }
            fd = GetServerFd(true);
            PARAM_LOGE("Failed to recv msg from server %d errno %d", recvLen, errno);
        }
        PARAM_LOGD("Recv msg from server %d", recvLen);
        if (recvLen >= (ssize_t)sizeof(ParamMessage)) {
            ProcessWatcherMessage(buffer, recvLen);
        }
    }
    PARAM_LOGI("Exit runLoop");
}

void WatcherManager::StartLoop()
{
    if (pRecvThread_ == nullptr) {
        pRecvThread_ = new (std::nothrow)std::thread(&WatcherManager::RunLoop, this);
        WATCHER_CHECK(pRecvThread_ != nullptr, return, "Failed to create thread");
    }
}

int WatcherManager::GetServerFd(bool retry)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (retry && serverFd_ != INVALID_SOCKET) {
        close(serverFd_);
        serverFd_ = INVALID_SOCKET;
    }
    if (serverFd_ != INVALID_SOCKET) {
        return serverFd_;
    }
    struct timeval time;
    time.tv_sec = 1;
    time.tv_usec = 0;
    do {
        serverFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        int ret = ConntectServer(serverFd_, PIPE_NAME);
        if (ret != 0) {
            close(serverFd_);
            serverFd_ = INVALID_SOCKET;
            usleep(SLEEP_TIME);
        } else {
            (void)setsockopt(serverFd_, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(struct timeval));
            break;
        }
    } while (1);
    return serverFd_;
}

void WatcherManager::OnStart()
{
    PARAM_LOGI("WatcherManager OnStart");
    bool res = Publish(this);
    if (!res) {
        PARAM_LOGI("WatcherManager OnStart failed");
    }
    return;
}

void WatcherManager::OnStop()
{
    PARAM_LOGI("WatcherManager OnStop");
    stop = true;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        close(serverFd_);
        serverFd_ = INVALID_SOCKET;
    }
    if (pRecvThread_ != nullptr) {
        pRecvThread_->join();
        delete pRecvThread_;
        pRecvThread_ = nullptr;
    }
}
} // namespace init_param
} // namespace OHOS
