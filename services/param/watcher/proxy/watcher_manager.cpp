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
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <thread>

#include "param_message.h"
#include "param_message.h"
#include "init_param.h"
#include "system_ability_definition.h"
#include "watcher_utils.h"

namespace OHOS {
namespace init_param {
REGISTER_SYSTEM_ABILITY_BY_ID(WatcherManager, PARAM_WATCHER_DISTRIBUTED_SERVICE_ID, true)

const static int32_t INVALID_SOCKET = -1;
WatcherManager::~WatcherManager()
{
    watchers_.clear();
    watcherGroups_.clear();
    groupMap_.clear();
}

uint32_t WatcherManager::AddWatcher(const std::string &keyPrefix, const sptr<IWatcher> &watcher)
{
    WATCHER_CHECK(watcher != nullptr && deathRecipient_ != nullptr,
        return 0, "Invalid remove watcher for %s", keyPrefix.c_str());
    sptr<IRemoteObject> object = watcher->AsObject();
    if ((object != nullptr) && (object->IsProxyObject())) {
        WATCHER_CHECK(object->AddDeathRecipient(deathRecipient_),
            return 0, "Failed to add death recipient %s", keyPrefix.c_str());
    }

    // check watcher id
    uint32_t watcherId = 0;
    int ret = GetWatcherId(watcherId);
    WATCHER_CHECK(ret == 0, return 0, "Failed to get watcher id for %s", keyPrefix.c_str());

    WATCHER_LOGV("AddWatcher prefix %s watcherId %u", keyPrefix.c_str(), watcherId);
    bool newGroup = false;
    WatcherGroupPtr group = GetWatcherGroup(keyPrefix);
    if (group == nullptr) {
        newGroup = true;
        uint32_t groupId = 0;
        ret = GetGroupId(groupId);
        WATCHER_CHECK(ret == 0, return 0, "Failed to get group id for %s", keyPrefix.c_str());
        group = std::make_shared<WatcherGroup>(groupId, keyPrefix);
        WATCHER_CHECK(group != nullptr, return 0, "Failed to create group for %s", keyPrefix.c_str());
    } else {
        newGroup = group->Emptry();
    }
    ParamWatcherPtr paramWather = std::make_shared<ParamWatcher>(watcherId, watcher, group);
    WATCHER_CHECK(paramWather != nullptr, return 0, "Failed to create watcher for %s", keyPrefix.c_str());
    AddParamWatcher(keyPrefix, group, paramWather);
    if (newGroup) {
        StartLoop();
        SendMessage(group, MSG_ADD_WATCHER);
    }
    SendLocalChange(keyPrefix, paramWather);
    WATCHER_LOGI("AddWatcher %s watcherId: %u success", keyPrefix.c_str(), watcherId);
    return watcherId;
}

int32_t WatcherManager::DelWatcher(const std::string &keyPrefix, uint32_t watcherId)
{
    auto group = GetWatcherGroup(keyPrefix);
    WATCHER_CHECK(group != nullptr, return 0, "Can not find group %s", keyPrefix.c_str());
    auto paramWatcher = GetWatcher(watcherId);
    WATCHER_CHECK(group != nullptr, return 0, "Can not find watcher %s %d", keyPrefix.c_str(), watcherId);
    WATCHER_LOGV("DelWatcher prefix %s watcherId %u", keyPrefix.c_str(), watcherId);
    return DelWatcher(group, paramWatcher);
}

int32_t WatcherManager::DelWatcher(WatcherGroupPtr group, ParamWatcherPtr watcher)
{
    WATCHER_CHECK(watcher != nullptr && group != nullptr, return 0, "Invalid watcher");
    sptr<IRemoteObject> object = watcher->GetRemoteWatcher()->AsObject();
    if (object != nullptr) {
        object->RemoveDeathRecipient(deathRecipient_);
    }
    WATCHER_LOGI("DelWatcher watcherId %u prefix %s", watcher->GetWatcherId(), group->GetKeyPrefix().c_str());
    DelParamWatcher(watcher);
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
    WATCHER_CHECK(request != NULL, return PARAM_CODE_ERROR, "Failed to malloc for watch");
    request->id.watcherId = group->GetGroupId();
    request->msgSize = sizeof(ParamMessage);

    WATCHER_LOGV("SendMessage %s", group->GetKeyPrefix().c_str());
    int ret = PARAM_CODE_ERROR;
    int fd = GetServerFd(false);
    if (fd != INVALID_SOCKET) {
        ssize_t sendLen = send(serverFd_, (char *)request, request->msgSize, 0);
        ret = (sendLen > 0) ? 0 : PARAM_CODE_ERROR;
    }
    free(request);
    WATCHER_CHECK(ret == 0, return ret, "SendMessage key: %s %d fail", group->GetKeyPrefix().c_str(), type);
    return 0;
}

void WatcherManager::ProcessWatcherMessage(const std::vector<char> &buffer, uint32_t dataSize)
{
    ParamMessage *msg = (ParamMessage *)buffer.data();
    WATCHER_CHECK(msg != NULL, return, "Invalid msg");
    WATCHER_LOGV("ProcessWatcherMessage %d", msg->type);
    uint32_t offset = 0;
    if (msg->type != MSG_NOTIFY_PARAM) {
        return;
    }
    WATCHER_CHECK(msg->msgSize <= dataSize, return, "Invalid msg size %d", msg->msgSize);
    ParamMsgContent *valueContent = GetNextContent((const ParamMessage *)msg, &offset);
    WATCHER_CHECK(valueContent != NULL, return, "Invalid msg ");
    WATCHER_LOGV("ProcessWatcherMessage name %s watcherId %u ", msg->key, msg->id.watcherId);
    WatcherGroupPtr group = GetWatcherGroup(msg->id.watcherId);
    if (group != nullptr) {
        std::lock_guard<std::mutex> lock(watcherMutex_);
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
    std::lock_guard<std::mutex> lock(watcherMutex_);
    if (groupMap_.find(keyPrefix) == groupMap_.end()) {
        return nullptr;
    }
    uint32_t groupId = groupMap_[keyPrefix];
    if (watcherGroups_.find(groupId) != watcherGroups_.end()) {
        return watcherGroups_[groupId];
    }
    return nullptr;
}

void WatcherManager::AddParamWatcher(const std::string &keyPrefix, WatcherGroupPtr group, ParamWatcherPtr watcher)
{
    WATCHER_LOGV("AddParamWatcher prefix %s watcherId %u", keyPrefix.c_str(), watcher->GetWatcherId());
    uint32_t groupId = group->GetGroupId();
    std::lock_guard<std::mutex> lock(watcherMutex_);
    groupMap_[keyPrefix] = groupId;
    watchers_[watcher->GetWatcherId()] = watcher;
    OH_ListAddTail(group->GetWatchers(), watcher->GetGroupNode());

    if (watcherGroups_.find(groupId) != watcherGroups_.end()) {
        return;
    }
    watcherGroups_[groupId] = group;
}

void WatcherManager::DelParamWatcher(ParamWatcherPtr watcher)
{
    std::lock_guard<std::mutex> lock(watcherMutex_);
    OH_ListRemove(watcher->GetGroupNode());
    OH_ListInit(watcher->GetGroupNode());
    watchers_.erase(watcher->GetWatcherId());
    WATCHER_LOGV("DelParamWatcher watcherId %u", watcher->GetWatcherId());
}

void WatcherManager::DelWatcherGroup(WatcherGroupPtr group)
{
    std::lock_guard<std::mutex> lock(watcherMutex_);
    groupMap_.erase(group->GetKeyPrefix());
    watcherGroups_.erase(group->GetGroupId());
}

void WatcherManager::WatcherGroup::ProcessParameterChange(const std::string &name, const std::string &value)
{
    // walk watcher
    ListNode *node = nullptr;
    ForEachListEntry(&watchers_, node) {
        ParamWatcher *watcher = (ParamWatcher *)node;
        watcher->ProcessParameterChange(name, value);
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
    WATCHER_LOGV("SendLocalChange key %s  ", keyPrefix.c_str());
    std::vector<char> buffer(PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX);
    struct Context context = {buffer.data(), watcher, keyPrefix};
    // walk watcher
    SystemTraversalParameter("", [](ParamHandle handle, void *cookie) {
            struct Context *context = (struct Context *)(cookie);
            SystemGetParameterName(handle, context->buffer, PARAM_NAME_LEN_MAX);
            if (!FilterParam(context->buffer, context->keyPrefix)) {
                return;
            }
            uint32_t size = PARAM_CONST_VALUE_LEN_MAX;
            SystemGetParameterValue(handle, context->buffer + PARAM_NAME_LEN_MAX, &size);
            WATCHER_LOGV("SendLocalChange key %s value: %s ", context->buffer, context->buffer + PARAM_NAME_LEN_MAX);
            context->watcher->ProcessParameterChange(context->buffer, context->buffer + PARAM_NAME_LEN_MAX);
        }, (void *)&context);
}

void WatcherManager::RunLoop()
{
    const int32_t RECV_BUFFER_MAX = 5 * 1024;
    std::vector<char> buffer(RECV_BUFFER_MAX, 0);
    bool retry = false;
    ssize_t recvLen = 0;
    while (!stop_) {
        int fd = GetServerFd(retry);
        if (fd >= 0) {
            recvLen = recv(fd, buffer.data(), RECV_BUFFER_MAX, 0);
        }
        if (stop_) {
            break;
        }
        if (recvLen <= 0) {
            if (errno == EAGAIN) { // timeout
                continue;
            }
            PARAM_LOGE("Failed to recv msg from server errno %d", errno);
            retry = true;  // re connect
        } else if (recvLen >= (ssize_t)sizeof(ParamMessage)) {
            ProcessWatcherMessage(buffer, recvLen);
        }
    }
    if (serverFd_ > 0) {
        close(serverFd_);
        serverFd_ = INVALID_SOCKET;
    }
    WATCHER_LOGV("Exit runLoop");
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
    const int32_t sleepTime = 200;
    const int32_t maxRetry = 10;
    std::lock_guard<std::mutex> lock(mutex_);
    if (retry && serverFd_ != INVALID_SOCKET) {
        close(serverFd_);
        serverFd_ = INVALID_SOCKET;
    }
    if (serverFd_ != INVALID_SOCKET) {
        return serverFd_;
    }
    int ret = 0;
    int32_t retryCount = 0;
    do {
        serverFd_ = socket(PF_UNIX, SOCK_STREAM, 0);
        int flags = fcntl(serverFd_, F_GETFL, 0);
        (void)fcntl(serverFd_, F_SETFL, flags & ~O_NONBLOCK);
        ret = ConntectServer(serverFd_, CLIENT_PIPE_NAME);
        if (ret == 0) {
            break;
        }
        close(serverFd_);
        serverFd_ = INVALID_SOCKET;
        usleep(sleepTime);
        retryCount++;
    } while (retryCount < maxRetry);
    WATCHER_LOGV("GetServerFd serverFd_ %d retryCount %d ", serverFd_, retryCount);
    return serverFd_;
}

void WatcherManager::OnStart()
{
    WATCHER_LOGI("WatcherManager OnStart");
    bool res = Publish(this);
    if (!res) {
        WATCHER_LOGE("WatcherManager Publish failed");
    }
    if (deathRecipient_ == nullptr) {
        deathRecipient_ = new DeathRecipient(this);
    }
    return;
}

void WatcherManager::StopLoop()
{
    WATCHER_LOGV("WatcherManager StopLoop serverFd_ %d", serverFd_);
    stop_ = true;
    if (serverFd_ > 0) {
        shutdown(serverFd_, SHUT_RDWR);
        close(serverFd_);
        serverFd_ = INVALID_SOCKET;
    }
    if (pRecvThread_ != nullptr) {
        pRecvThread_->join();
        delete pRecvThread_;
        pRecvThread_ = nullptr;
    }
}

void WatcherManager::OnStop()
{
    {
        std::lock_guard<std::mutex> lock(watcherMutex_);
        for (auto iter = watchers_.begin(); iter != watchers_.end(); ++iter) {
            if (iter->second == nullptr) {
                continue;
            }
            sptr<IRemoteObject> object = iter->second->GetRemoteWatcher()->AsObject();
            if (object != nullptr) {
                object->RemoveDeathRecipient(deathRecipient_);
            }
        }
    }
    watchers_.clear();
    watcherGroups_.clear();
    groupMap_.clear();
    StopLoop();
}

void WatcherManager::DeathRecipient::OnRemoteDied(const wptr<IRemoteObject> &remote)
{
    WATCHER_CHECK(remote != nullptr, return, "Invalid remote obj");
    auto paramWatcher = manager_->GetWatcher(remote);
    WATCHER_CHECK(paramWatcher != nullptr, return, "Failed to get remote watcher info ");
    WATCHER_LOGV("OnRemoteDied watcherId %u", paramWatcher->GetWatcherId());
    manager_->DelWatcher(paramWatcher->GetWatcherGroup(), paramWatcher);
}

WatcherManager::ParamWatcherPtr WatcherManager::GetWatcher(uint32_t watcherId)
{
    std::lock_guard<std::mutex> lock(watcherMutex_);
    auto iter = watchers_.find(watcherId);
    if (iter != watchers_.end()) {
        return iter->second;
    }
    return nullptr;
}

WatcherManager::ParamWatcherPtr WatcherManager::GetWatcher(const wptr<IRemoteObject> &remote)
{
    std::lock_guard<std::mutex> lock(watcherMutex_);
    for (auto iter = watchers_.begin(); iter != watchers_.end(); ++iter) {
        if (iter->second == nullptr) {
            continue;
        }
        if (remote == iter->second->GetRemoteWatcher()->AsObject()) {
            return iter->second;
        }
    }
    return nullptr;
}

int WatcherManager::GetWatcherId(uint32_t &watcherId)
{
    std::lock_guard<std::mutex> lock(watcherMutex_);
    watcherId = watcherId_;
    do {
        watcherId_++;
        if (watcherId_ == 0) {
            watcherId_++;
        }
        if (watchers_.find(watcherId_) == watchers_.end()) {
            break;
        }
        WATCHER_CHECK(watcherId_ != watcherId, return -1, "No enough watcherId %u", watcherId);
    } while (1);
    watcherId = watcherId_;
    return 0;
}

int WatcherManager::GetGroupId(uint32_t &groupId)
{
    std::lock_guard<std::mutex> lock(watcherMutex_);
    groupId = groupId_;
    do {
        groupId_++;
        if (watcherGroups_.find(groupId_) == watcherGroups_.end()) {
            break;
        }
        WATCHER_CHECK(groupId_ == groupId, return -1, "No enough groupId %u", groupId);
    } while (1);
    groupId = groupId_;
    return 0;
}
} // namespace init_param
} // namespace OHOS
