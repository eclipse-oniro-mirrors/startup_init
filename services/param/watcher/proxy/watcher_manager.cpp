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

#include "init_param.h"
#include "system_ability_definition.h"
#include "string_ex.h"
#include "watcher_utils.h"

namespace OHOS {
namespace init_param {
REGISTER_SYSTEM_ABILITY_BY_ID(WatcherManager, PARAM_WATCHER_DISTRIBUTED_SERVICE_ID, true)

const static int32_t INVALID_SOCKET = -1;
WatcherManager::~WatcherManager()
{
    Clear();
}

uint32_t WatcherManager::AddRemoteWatcher(uint32_t id, const sptr<IWatcher> &watcher)
{
    if (id == static_cast<uint32_t>(getpid())) {
        WATCHER_LOGE("Failed to add remote watcher %u", id);
        return 0;
    }
    WATCHER_CHECK(watcher != nullptr && deathRecipient_ != nullptr, return 0, "Invalid remove watcher");
    sptr<IRemoteObject> object = watcher->AsObject();
    if ((object != nullptr) && (object->IsProxyObject())) {
        WATCHER_CHECK(object->AddDeathRecipient(deathRecipient_), return 0, "Failed to add death recipient %u", id);
    }
    uint32_t remoteWatcherId = 0;
    {
        std::lock_guard<std::mutex> lock(watcherMutex_);
        // check watcher id
        int ret = GetRemoteWatcherId(remoteWatcherId);
        WATCHER_CHECK(ret == 0, return 0, "Failed to get watcher id for %u", id);
        // create remote watcher
        RemoteWatcher *remoteWatcher = new RemoteWatcher(remoteWatcherId, watcher);
        WATCHER_CHECK(remoteWatcher != nullptr, return 0, "Failed to create watcher for %u", id);
        remoteWatcher->SetAgentId(id);
        AddRemoteWatcher(remoteWatcher);
    }
    WATCHER_LOGI("Add remote watcher remoteWatcherId %u %u success", remoteWatcherId, id);
    return remoteWatcherId;
}

int32_t WatcherManager::DelRemoteWatcher(uint32_t remoteWatcherId)
{
    sptr<IWatcher> watcher = {0};
    {
        std::lock_guard<std::mutex> lock(watcherMutex_);
        RemoteWatcher *remoteWatcher = GetRemoteWatcher(remoteWatcherId);
        WATCHER_CHECK(remoteWatcher != nullptr, return 0, "Can not find watcher %u", remoteWatcherId);
        WATCHER_LOGI("Del remote watcher remoteWatcherId %u", remoteWatcherId);
        watcher = remoteWatcher->GetWatcher();
        DelRemoteWatcher(remoteWatcher);
    }
    sptr<IRemoteObject> object = watcher->AsObject();
    if (object != nullptr) {
        object->RemoveDeathRecipient(deathRecipient_);
    }
    return 0;
}

int32_t WatcherManager::AddWatcher(const std::string &keyPrefix, uint32_t remoteWatcherId)
{
    // get remote watcher and group
    auto remoteWatcher = GetRemoteWatcher(remoteWatcherId);
    WATCHER_CHECK(remoteWatcher != nullptr, return -1, "Can not find remote watcher %d", remoteWatcherId);
    auto group = AddWatcherGroup(keyPrefix);
    WATCHER_CHECK(group != nullptr, return -1, "Failed to create group for %s", keyPrefix.c_str());
    {
        // add watcher to agent and group
        std::lock_guard<std::mutex> lock(watcherMutex_);
        bool newGroup = group->Empty();
        AddParamWatcher(group, remoteWatcher);
        if (newGroup) {
            StartLoop();
            SendMessage(group, MSG_ADD_WATCHER);
        }
    }
    SendLocalChange(keyPrefix, remoteWatcher);
    WATCHER_LOGI("Add watcher %s remoteWatcherId: %u groupId %u success",
        keyPrefix.c_str(), remoteWatcherId, group->GetGroupId());
    return 0;
}

int32_t WatcherManager::DelWatcher(const std::string &keyPrefix, uint32_t remoteWatcherId)
{
    auto group = GetWatcherGroup(keyPrefix);
    WATCHER_CHECK(group != nullptr, return 0, "Can not find group %s", keyPrefix.c_str());
    auto remoteWatcher = GetRemoteWatcher(remoteWatcherId);
    WATCHER_CHECK(remoteWatcher != nullptr, return 0, "Can not find watcher %s %d", keyPrefix.c_str(), remoteWatcherId);
    WATCHER_LOGI("Delete watcher prefix %s remoteWatcherId %u", keyPrefix.c_str(), remoteWatcherId);
    {
        // remove watcher from agent and group
        std::lock_guard<std::mutex> lock(watcherMutex_);
        DelParamWatcher(group, remoteWatcher);
        if (group->Empty()) { // no watcher, so delete it
            SendMessage(group, MSG_DEL_WATCHER);
            DelWatcherGroup(group);
        }
    }
    return 0;
}

int32_t WatcherManager::RefreshWatcher(const std::string &keyPrefix, uint32_t remoteWatcherId)
{
    WATCHER_LOGV("Refresh watcher %s remoteWatcherId: %u", keyPrefix.c_str(), remoteWatcherId);
    auto group = GetWatcherGroup(keyPrefix);
    WATCHER_CHECK(group != nullptr, return 0, "Can not find group %s", keyPrefix.c_str());
    auto remoteWatcher = GetRemoteWatcher(remoteWatcherId);
    WATCHER_CHECK(remoteWatcher != nullptr, return 0, "Can not find watcher %s %d", keyPrefix.c_str(), remoteWatcherId);
    SendLocalChange(keyPrefix, remoteWatcher);
    return 0;
}

int WatcherManager::SendMessage(WatcherGroupPtr group, int type)
{
    ParamMessage *request = nullptr;
    std::string key(group->GetKeyPrefix());
    if (key.rfind("*") == key.length() - 1) {
        key = key.substr(0, key.length() - 1);
    }
    request = (ParamMessage *)CreateParamMessage(type, key.c_str(), sizeof(ParamMessage));
    WATCHER_CHECK(request != NULL, return PARAM_CODE_ERROR, "Failed to malloc for watch");
    request->id.watcherId = group->GetGroupId();
    request->msgSize = sizeof(ParamMessage);
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

void WatcherGroup::ProcessParameterChange(
    WatcherManager *mananger, const std::string &name, const std::string &value)
{
    WATCHER_LOGV("ProcessParameterChange key '%s' '%s'", GetKeyPrefix().c_str(), name.c_str());
    // walk watcher
    TraversalNode([this, mananger, name, value](ParamWatcherListPtr list, WatcherNodePtr node, uint32_t index) {
        auto remoteWatcher = mananger->GetRemoteWatcher(node->GetNodeId());
        if (remoteWatcher == nullptr) {
            return;
        }
        remoteWatcher->ProcessParameterChange(GetKeyPrefix(), name, value);
    });
}

static int FilterParam(const char *name, const std::string &keyPrefix)
{
    if (keyPrefix.rfind("*") == keyPrefix.length() - 1) {
        return strncmp(name, keyPrefix.c_str(), keyPrefix.length() - 1) == 0;
    }
    if (keyPrefix.rfind(".") == keyPrefix.length() - 1) {
        return strncmp(name, keyPrefix.c_str(), keyPrefix.length() - 1) == 0;
    }
    return strcmp(name, keyPrefix.c_str()) == 0;
}

void WatcherManager::ProcessWatcherMessage(const ParamMessage *msg)
{
    uint32_t offset = 0;
    if (msg->type != MSG_NOTIFY_PARAM) {
        return;
    }
    ParamMsgContent *valueContent = GetNextContent(msg, &offset);
    WATCHER_CHECK(valueContent != NULL, return, "Invalid msg ");
    WATCHER_LOGV("Process watcher message name '%s' group id %u ", msg->key, msg->id.watcherId);
    {
        std::lock_guard<std::mutex> lock(watcherMutex_);
        WatcherGroupPtr group = GetWatcherGroup(msg->id.watcherId);
        WATCHER_CHECK(group != NULL, return, "Can not find group for %u %s", msg->id.watcherId, msg->key);
        if (!FilterParam(msg->key, group->GetKeyPrefix())) {
            WATCHER_LOGV("Invalid message name '%s' group '%s' ", msg->key, group->GetKeyPrefix().c_str());
            return;
        }
        group->ProcessParameterChange(this, msg->key, valueContent->content);
    }
}

void WatcherManager::SendLocalChange(const std::string &keyPrefix, RemoteWatcherPtr &remoteWatcher)
{
    struct Context {
        char *buffer;
        RemoteWatcherPtr remoteWatcher;
        std::string keyPrefix;
    };
    std::vector<char> buffer(PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX);
    struct Context context = {buffer.data(), remoteWatcher, keyPrefix};
    // walk watcher
    SystemTraversalParameter("", [](ParamHandle handle, void *cookie) {
            struct Context *context = (struct Context *)(cookie);
            SystemGetParameterName(handle, context->buffer, PARAM_NAME_LEN_MAX);
            if (!FilterParam(context->buffer, context->keyPrefix)) {
                return;
            }
            uint32_t size = PARAM_CONST_VALUE_LEN_MAX;
            SystemGetParameterValue(handle, context->buffer + PARAM_NAME_LEN_MAX, &size);
            context->remoteWatcher->ProcessParameterChange(
                context->keyPrefix, context->buffer, context->buffer + PARAM_NAME_LEN_MAX);
        }, reinterpret_cast<void *>(&context));
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
            continue;
        }
        uint32_t curr = 0;
        uint32_t dataLen = static_cast<uint32_t>(recvLen);
        while (curr < dataLen) {
            if ((curr + sizeof(ParamMessage)) >= dataLen) {
                break;
            }
            ParamMessage *msg = (ParamMessage *)(buffer.data() + curr);
            if ((msg->msgSize > dataLen) || ((curr + msg->msgSize) > dataLen)) {
                break;
            }
            ProcessWatcherMessage(msg);
            curr += msg->msgSize;
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
    int32_t retryCount = 0;
    do {
        serverFd_ = socket(PF_UNIX, SOCK_STREAM, 0);
        int flags = fcntl(serverFd_, F_GETFL, 0);
        (void)fcntl(serverFd_, F_SETFL, flags & ~O_NONBLOCK);
        int ret = ConnectServer(serverFd_, CLIENT_PIPE_NAME);
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
    WATCHER_LOGI("Watcher manager OnStart");
    bool res = Publish(this);
    if (!res) {
        WATCHER_LOGE("WatcherManager Publish failed");
#ifndef STARTUP_INIT_TEST
        return;
#endif
    }
    if (deathRecipient_ == nullptr) {
        deathRecipient_ = new DeathRecipient(this);
    }
    SystemSetParameter("bootevent.param_watcher.started", "true");
    return;
}

void WatcherManager::StopLoop()
{
    WATCHER_LOGI("Watcher manager StopLoop serverFd_ %d", serverFd_);
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
    if (remoteWatchers_ != nullptr) {
        std::lock_guard<std::mutex> lock(watcherMutex_);
        remoteWatchers_->TraversalNodeSafe([this](ParamWatcherListPtr list, WatcherNodePtr node, uint32_t index) {
            RemoteWatcherPtr remoteWatcher = ConvertTo<RemoteWatcher>(node);
            OnRemoteDied(remoteWatcher);
        });
    }
    Clear();
    StopLoop();
}

void WatcherManager::OnRemoteDied(const wptr<IRemoteObject> &remote)
{
    WATCHER_CHECK(remote != nullptr, return, "Invalid remote obj");
    auto remoteWatcher = GetRemoteWatcher(remote);
    WATCHER_CHECK(remoteWatcher != nullptr, return, "Failed to get remote watcher info ");
    {
        std::lock_guard<std::mutex> lock(watcherMutex_);
        OnRemoteDied(remoteWatcher);
    }
}

void WatcherManager::OnRemoteDied(RemoteWatcherPtr remoteWatcher)
{
    WATCHER_CHECK(remoteWatcher != nullptr, return, "Invalid remote obj");
    WATCHER_LOGI("Agent died %u %u", remoteWatcher->GetRemoteWatcherId(), remoteWatcher->GetAgentId());
    remoteWatcher->TraversalNodeSafe(
        [this, remoteWatcher](ParamWatcherListPtr list, WatcherNodePtr node, uint32_t index) {
            auto group = GetWatcherGroup(node->GetNodeId());
            if (group == nullptr) {
                return;
            }
            // delete node from group and remote
            DelParamWatcher(group, remoteWatcher);
            if (group->Empty()) { // no watcher, so delete it
                SendMessage(group, MSG_DEL_WATCHER);
                DelWatcherGroup(group);
            }
        });
    DelRemoteWatcher(remoteWatcher);
}

RemoteWatcherPtr WatcherManager::GetRemoteWatcher(const wptr<IRemoteObject> &remote)
{
    std::lock_guard<std::mutex> lock(watcherMutex_);
    WatcherNodePtr node = remoteWatchers_->GetNextNode(nullptr);
    while (node != nullptr) {
        RemoteWatcherPtr remoteWatcher = ConvertTo<RemoteWatcher>(node);
        if (remoteWatcher == nullptr) {
            continue;
        }
        if (remote == remoteWatcher->GetWatcher()->AsObject()) {
            return remoteWatcher;
        }
        node = remoteWatchers_->GetNextNode(node);
    }
    return nullptr;
}

int WatcherManager::GetRemoteWatcherId(uint32_t &remoteWatcherId)
{
    remoteWatcherId_++;
    if (remoteWatcherId_ == 0) {
        remoteWatcherId_++;
    }
    remoteWatcherId = remoteWatcherId_;
    return 0;
}

int WatcherManager::GetGroupId(uint32_t &groupId)
{
    groupId = groupId_;
    do {
        groupId_++;
        if (watcherGroups_->GetNode(groupId_) == nullptr) {
            break;
        }
        WATCHER_CHECK(groupId_ == groupId, return -1, "No enough groupId %u", groupId);
    } while (1);
    groupId = groupId_;
    return 0;
}

void WatcherManager::DumpAllGroup(int fd, ParamWatcherProcessor dumpHandle)
{
    // all output
    uint32_t count = 0;
    std::lock_guard<std::mutex> lock(watcherMutex_);
    for (auto it = groupMap_.begin(); it != groupMap_.end(); ++it) {
        auto group = it->second;
        if (group == nullptr) {
            continue;
        }
        dprintf(fd, "Watch prefix   : %s \n", group->GetKeyPrefix().c_str());
        dprintf(fd, "Watch group id : %u \n", group->GetGroupId());
        dprintf(fd, "Watch count    : %u \n", group->GetNodeCount());
        group->TraversalNode(dumpHandle);
        count += group->GetNodeCount();
        dprintf(fd, "\n");
    }

    dprintf(fd, "Watch prefix count : %u [%zu  %zu  %zu]\n", watcherGroups_->GetNodeCount(),
        sizeof(RemoteWatcher), sizeof(WatcherGroup), sizeof(WatcherNode));
    dprintf(fd, "Watch agent  count : %u \n", remoteWatchers_->GetNodeCount());
    dprintf(fd, "Watch count        : %u \n", count);
}

int WatcherManager::Dump(int fd, const std::vector<std::u16string>& args)
{
    WATCHER_CHECK(fd >= 0, return -1, "Invalid fd for dump %d", fd);
    WATCHER_CHECK(remoteWatchers_ != 0, return -1, "Invalid remote watcher");
    WATCHER_CHECK(watcherGroups_ != 0, return -1, "Invalid watcher group");
    std::vector<std::string> params;
    for (auto& arg : args) {
        params.emplace_back(Str16ToStr8(arg));
    }
    if (params.size() >= 1 && params[0] == "-h") {
        std::string dumpInfo = {};
        dumpInfo.append("Usage:\n")
            .append(" -h                    ")
            .append("|help text for the tool\n")
            .append(" -k                    ")
            .append("|dump watcher infomation for key prefix\n");
        dprintf(fd, "%s\n", dumpInfo.c_str());
        return 0;
    }
    auto dumpParamWatcher = [this, fd](ParamWatcherListPtr list, WatcherNodePtr node, uint32_t index) {
        auto remoteWatcher = GetRemoteWatcher(node->GetNodeId());
        if (remoteWatcher != nullptr) {
            dprintf(fd, "%s%u(%u)", (index == 0) ? "Watch id list  : " : ", ",
                node->GetNodeId(), remoteWatcher->GetAgentId());
        } else {
            dprintf(fd, "%s%u", (index == 0) ? "Watch id list  : " : ", ", node->GetNodeId());
        }
    };

    if (params.size() > 1 && params[0] == "-k") {
        auto group = GetWatcherGroup(params[1]);
        if (group == NULL) {
            dprintf(fd, "Prefix %s not found in watcher list\n", params[1].c_str());
            return 0;
        }
        {
            std::lock_guard<std::mutex> lock(watcherMutex_);
            group->TraversalNode(dumpParamWatcher);
        }
        return 0;
    }
    DumpAllGroup(fd, dumpParamWatcher);
    return 0;
}

void WatcherManager::Clear(void)
{
    std::lock_guard<std::mutex> lock(watcherMutex_);
    remoteWatchers_->TraversalNodeSafe([](ParamWatcherListPtr list, WatcherNodePtr node, uint32_t index) {
        list->RemoveNode(node);
        auto group = ConvertTo<WatcherGroup>(node);
        delete group;
    });
    watcherGroups_->TraversalNodeSafe([](ParamWatcherListPtr list, WatcherNodePtr node, uint32_t index) {
        list->RemoveNode(node);
        auto remoteWatcher = ConvertTo<RemoteWatcher>(node);
        delete remoteWatcher;
    });
    delete remoteWatchers_;
    remoteWatchers_ = nullptr;
    delete watcherGroups_;
    watcherGroups_ = nullptr;
}

int WatcherManager::AddRemoteWatcher(RemoteWatcherPtr remoteWatcher)
{
    if (remoteWatchers_ == nullptr) {
        remoteWatchers_ = new ParamWatcherList();
        WATCHER_CHECK(remoteWatchers_ != nullptr, return -1, "Failed to create watcher");
    }
    return remoteWatchers_->AddNode(ConvertTo<WatcherNode>(remoteWatcher));
}

RemoteWatcherPtr WatcherManager::GetRemoteWatcher(uint32_t remoteWatcherId)
{
    WATCHER_CHECK(remoteWatchers_ != nullptr, return nullptr, "Invalid remote watcher");
    WatcherNodePtr node = remoteWatchers_->GetNode(remoteWatcherId);
    if (node == nullptr) {
        return nullptr;
    }
    return ConvertTo<RemoteWatcher>(node);
}

void WatcherManager::DelRemoteWatcher(RemoteWatcherPtr remoteWatcher)
{
    WATCHER_CHECK(remoteWatchers_ != nullptr, return, "Invalid remote watcher");
    remoteWatchers_->RemoveNode(ConvertTo<WatcherNode>(remoteWatcher));
    delete remoteWatcher;
}

int WatcherManager::AddParamWatcher(WatcherGroupPtr group, RemoteWatcherPtr remoteWatcher)
{
    WatcherNodePtr nodeGroup = new ParamWatcher(group->GetGroupId());
    WATCHER_CHECK(nodeGroup != nullptr, return -1, "Failed to create watcher node for group");
    WatcherNodePtr nodeRemote = new ParamWatcher(remoteWatcher->GetRemoteWatcherId());
    WATCHER_CHECK(nodeRemote != nullptr, delete nodeGroup;
        return -1, "Failed to create watcher node for remote watcher");
    group->AddNode(nodeRemote);
    remoteWatcher->AddNode(nodeGroup);
    return 0;
}

int WatcherManager::DelParamWatcher(WatcherGroupPtr group, RemoteWatcherPtr remoteWatcher)
{
    WATCHER_LOGI("Delete param watcher remoteWatcherId %u group %u",
        remoteWatcher->GetRemoteWatcherId(), group->GetGroupId());
    WatcherNodePtr node = group->GetNode(remoteWatcher->GetRemoteWatcherId());
    if (node != nullptr) {
        group->RemoveNode(node);
        delete node;
    }
    node = remoteWatcher->GetNode(group->GetGroupId());
    if (node != nullptr) {
        remoteWatcher->RemoveNode(node);
        delete node;
    }
    return 0;
}

WatcherGroupPtr WatcherManager::AddWatcherGroup(const std::string &keyPrefix)
{
    std::lock_guard<std::mutex> lock(watcherMutex_);
    if (watcherGroups_ == nullptr) {
        watcherGroups_ = new ParamWatcherList();
        WATCHER_CHECK(watcherGroups_ != nullptr, return nullptr, "Failed to create watcher");
    }
    // get group
    auto it = groupMap_.find(keyPrefix);
    if (it != groupMap_.end()) {
        return it->second;
    }
    // create group
    uint32_t groupId = 0;
    int ret = GetGroupId(groupId);
    WATCHER_CHECK(ret == 0, return nullptr, "Failed to get group id for %s", keyPrefix.c_str());
    WatcherGroupPtr group = new WatcherGroup(groupId, keyPrefix);
    WATCHER_CHECK(group != nullptr, return nullptr, "Failed to create group for %s", keyPrefix.c_str());
    watcherGroups_->AddNode(ConvertTo<WatcherNode>(group));
    groupMap_[keyPrefix] = group;
    return group;
}

WatcherGroupPtr WatcherManager::GetWatcherGroup(uint32_t groupId)
{
    WATCHER_CHECK(watcherGroups_ != nullptr, return nullptr, "Invalid watcher groups");
    WatcherNodePtr node = watcherGroups_->GetNode(groupId);
    if (node == nullptr) {
        return nullptr;
    }
    return ConvertTo<WatcherGroup>(node);
}

WatcherGroupPtr WatcherManager::GetWatcherGroup(const std::string &keyPrefix)
{
    std::lock_guard<std::mutex> lock(watcherMutex_);
    // get group
    auto it = groupMap_.find(keyPrefix);
    if (it != groupMap_.end()) {
        return it->second;
    }
    return nullptr;
}

void WatcherManager::DelWatcherGroup(WatcherGroupPtr group)
{
    WATCHER_CHECK(watcherGroups_ != nullptr, return, "Invalid watcher groups");
    WATCHER_LOGI("Delete watcher group %s %u", group->GetKeyPrefix().c_str(), group->GetGroupId());
    watcherGroups_->RemoveNode(ConvertTo<WatcherNode>(group));
    auto it = groupMap_.find(group->GetKeyPrefix());
    if (it != groupMap_.end()) {
        groupMap_.erase(it);
    }
    delete group;
}

int ParamWatcherList::AddNode(WatcherNodePtr node)
{
    WATCHER_CHECK(node, return -1, "Invalid input node");
    node->AddToList(&nodeList_);
    nodeCount_++;
    return 0;
}

int ParamWatcherList::RemoveNode(WatcherNodePtr node)
{
    WATCHER_CHECK(node, return -1, "Invalid input node");
    node->RemoveFromList(&nodeList_);
    nodeCount_--;
    return 0;
}

WatcherNodePtr ParamWatcherList::GetNode(uint32_t nodeId)
{
    return WatcherNode::GetFromList(&nodeList_, nodeId);
}

WatcherNodePtr ParamWatcherList::GetNextNodeSafe(WatcherNodePtr node)
{
    if (node == nullptr) { // get first
        return WatcherNode::GetNextFromList(&nodeList_, 0);
    }
    return WatcherNode::GetNextFromList(&nodeList_, node->GetNodeId());
}

WatcherNodePtr ParamWatcherList::GetNextNode(WatcherNodePtr node)
{
    if (node == nullptr) { // get first
        return WatcherNode::GetNextFromList(&nodeList_, 0);
    }
    return node->GetNext(&nodeList_);
}

void ParamWatcherList::TraversalNode(ParamWatcherProcessor handle)
{
    uint32_t index = 0;
    // get first
    WatcherNodePtr node = WatcherNode::GetNextFromList(&nodeList_, 0);
    while (node != nullptr) {
        WatcherNodePtr next = node->GetNext(&nodeList_);
        handle(this, node, index);
        node = next;
        index++;
    }
}

void ParamWatcherList::TraversalNodeSafe(ParamWatcherProcessor processor)
{
    uint32_t index = 0;
    // get first
    WatcherNodePtr node = WatcherNode::GetNextFromList(&nodeList_, 0);
    while (node != nullptr) {
        uint32_t nodeId = node->GetNodeId();
        // notify free, must be free
        processor(this, node, index);
        node = WatcherNode::GetNextFromList(&nodeList_, nodeId);
        index++;
    }
}

void WatcherNode::AddToList(ListHead *list)
{
    OH_ListAddWithOrder(list, &node_, CompareNode);
}

void WatcherNode::RemoveFromList(ListHead *list)
{
    OH_ListRemove(&node_);
}

WatcherNodePtr WatcherNode::GetFromList(ListHead *list, uint32_t nodeId)
{
    ListNodePtr node = OH_ListFind(list, &nodeId, CompareData);
    if (node == nullptr) {
        return nullptr;
    }
    return WatcherNode::ConvertNodeToBase(node);
}

WatcherNodePtr WatcherNode::GetNextFromList(ListHead *list, uint32_t nodeId)
{
    ListNodePtr node = OH_ListFind(list, &nodeId, Greater);
    if (node == nullptr) {
        return nullptr;
    }
    return WatcherNode::ConvertNodeToBase(node);
}

WatcherNodePtr WatcherNode::GetNext(ListHead *list)
{
    if (node_.next == list) {
        return nullptr;
    }
    return WatcherNode::ConvertNodeToBase(node_.next);
}

int WatcherNode::CompareNode(ListNodePtr node, ListNodePtr newNode)
{
    WatcherNodePtr watcher = WatcherNode::ConvertNodeToBase(node);
    WatcherNodePtr newWatcher = WatcherNode::ConvertNodeToBase(node);
    return watcher->nodeId_ - newWatcher->nodeId_;
}

int WatcherNode::CompareData(ListNodePtr node, void *data)
{
    WatcherNodePtr watcher =  WatcherNode::ConvertNodeToBase(node);
    uint32_t id = *(uint32_t *)data;
    return watcher->nodeId_ - id;
}

int WatcherNode::Greater(ListNodePtr node, void *data)
{
    WatcherNodePtr watcher =  WatcherNode::ConvertNodeToBase(node);
    uint32_t id = *(uint32_t *)data;
    return (watcher->nodeId_ > id) ? 0 : 1;
}
} // namespace init_param
} // namespace OHOS
