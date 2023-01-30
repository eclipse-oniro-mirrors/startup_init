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
#ifndef WATCHER_MANAGER_H_
#define WATCHER_MANAGER_H_
#include <atomic>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "iremote_stub.h"
#include "iwatcher.h"
#include "list.h"
#include "message_parcel.h"
#include "param_utils.h"
#include "parcel.h"
#include "param_message.h"
#include "system_ability.h"
#include "watcher_manager_stub.h"

namespace OHOS {
namespace init_param {
class WatcherNode;
class ParamWatcherList;
class WatcherGroup;
class RemoteWatcher;
using WatcherGroupPtr = WatcherGroup *;
using RemoteWatcherPtr = RemoteWatcher *;
using WatcherNodePtr = WatcherNode *;
using ListNodePtr = ListNode *;
using ListHeadPtr = ListHead *;
using ParamWatcherListPtr = ParamWatcherList *;
using ParamWatcherProcessor = std::function<void(ParamWatcherListPtr list, WatcherNodePtr node, uint32_t index)>;

class WatcherManager : public SystemAbility, public WatcherManagerStub {
public:
    friend class WatcherGroup;
    DECLARE_SYSTEM_ABILITY(WatcherManager);
    DISALLOW_COPY_AND_MOVE(WatcherManager);
    explicit WatcherManager(int32_t systemAbilityId, bool runOnCreate = true)
        : SystemAbility(systemAbilityId, runOnCreate)
    {
    }
    ~WatcherManager() override;

    // For death event procession
    class DeathRecipient final : public IRemoteObject::DeathRecipient {
    public:
        explicit DeathRecipient(WatcherManager *manager) : manager_(manager) {}
        ~DeathRecipient() final = default;
        DISALLOW_COPY_AND_MOVE(DeathRecipient);
        void OnRemoteDied(const wptr<IRemoteObject> &remote) final
        {
            manager_->OnRemoteDied(remote);
        }
    private:
        WatcherManager *manager_;
    };
    sptr<IRemoteObject::DeathRecipient> GetDeathRecipient(void)
    {
        return deathRecipient_;
    }
    uint32_t AddRemoteWatcher(uint32_t id, const sptr<IWatcher> &watcher) override;
    int32_t DelRemoteWatcher(uint32_t remoteWatcherId) override;
    int32_t AddWatcher(const std::string &keyPrefix, uint32_t remoteWatcherId) override;
    int32_t DelWatcher(const std::string &keyPrefix, uint32_t remoteWatcherId) override;
    int32_t RefreshWatcher(const std::string &keyPrefix, uint32_t remoteWatcherId) override;
#ifndef STARTUP_INIT_TEST
protected:
#endif
    void OnStart() override;
    void OnStop() override;
    int Dump(int fd, const std::vector<std::u16string>& args) override;
#ifndef STARTUP_INIT_TEST
private:
#endif
    void RunLoop();
    void StartLoop();
    void StopLoop();
    void SendLocalChange(const std::string &keyPrefix, RemoteWatcherPtr &remoteWatcher);
    int SendMessage(WatcherGroupPtr group, int type);
    int GetServerFd(bool retry);
    int GetRemoteWatcherId(uint32_t &remoteWatcherId);
    int GetGroupId(uint32_t &groupId);
    // for remote watcher
    int AddRemoteWatcher(RemoteWatcherPtr remoteWatcher);
    RemoteWatcherPtr GetRemoteWatcher(uint32_t remoteWatcherId);
    RemoteWatcherPtr GetRemoteWatcher(const wptr<IRemoteObject> &remote);
    void DelRemoteWatcher(RemoteWatcherPtr remoteWatcher);
    // for group
    WatcherGroupPtr AddWatcherGroup(const std::string &keyPrefix);
    WatcherGroupPtr GetWatcherGroup(const std::string &keyPrefix);
    WatcherGroupPtr GetWatcherGroup(uint32_t groupId);
    void DelWatcherGroup(WatcherGroupPtr group);
    // for param watcher
    int AddParamWatcher(WatcherGroupPtr group, RemoteWatcherPtr remoteWatcher);
    int DelParamWatcher(WatcherGroupPtr group, RemoteWatcherPtr remoteWatcher);
    // for process message form init
    void ProcessWatcherMessage(const ParamMessage *msg);
    // for client died
    void OnRemoteDied(const wptr<IRemoteObject> &remote);
    void OnRemoteDied(RemoteWatcherPtr remoteWatcher);
    // clear
    void Clear(void);
    // dump
    void DumpAllGroup(int fd, ParamWatcherProcessor dumpHandle);
private:
    std::atomic<uint32_t> remoteWatcherId_ { 0 };
    std::atomic<uint32_t> groupId_ { 0 };
    std::mutex mutex_;
    std::mutex watcherMutex_;
    int serverFd_ { -1 };
    std::thread *pRecvThread_ { nullptr };
    std::atomic<bool> stop_ { false };
    std::map<std::string, WatcherGroupPtr> groupMap_ {};
    sptr<IRemoteObject::DeathRecipient> deathRecipient_ {};
    ParamWatcherListPtr watcherGroups_ {};
    ParamWatcherListPtr remoteWatchers_ {};
};

class WatcherNode {
public:
    explicit WatcherNode(uint32_t nodeId) : nodeId_(nodeId)
    {
        OH_ListInit(&node_);
    }
    virtual ~WatcherNode(void) = default;
    uint32_t GetNodeId(void) const
    {
        return nodeId_;
    }

    void AddToList(ListHeadPtr list);
    void RemoveFromList(ListHeadPtr list);
    WatcherNodePtr GetNext(ListHeadPtr list);
    static WatcherNodePtr GetFromList(ListHeadPtr list, uint32_t nodeId);
    static WatcherNodePtr GetNextFromList(ListHeadPtr list, uint32_t nodeId);
    static WatcherNodePtr ConvertNodeToBase(ListNodePtr node)
    {
        return reinterpret_cast<WatcherNodePtr>((char *)node - (char*)(&(((WatcherNodePtr)0)->node_)));
    }
    ListNodePtr GetListNode()
    {
        return &node_;
    }
private:
    static int CompareNode(ListNodePtr node, ListNodePtr newNode);
    static int CompareData(ListNodePtr node, void *data);
    static int Greater(ListNodePtr node, void *data);
    ListNode node_;
    uint32_t nodeId_;
};

template<typename T>
inline T *ConvertTo(WatcherNodePtr node)
{
#ifdef PARAM_WATCHER_RTTI_ENABLE
    // when -frtti is enabled, we use dynamic cast directly
    // to achieve the correct base class side-to-side conversion.
    T *obj = dynamic_cast<T *>(node);
#else
    // adjust pointer position when multiple inheritance.
    void *tmp = reinterpret_cast<void *>(node);
    // when -frtti is not enable, we use static cast.
    // static cast is not safe enough, but we have checked before we get here.
    T *obj = static_cast<T *>(tmp);
#endif
    return obj;
}

class ParamWatcher : public WatcherNode {
public:
    explicit ParamWatcher(uint32_t watcherId) : WatcherNode(watcherId) {}
    ~ParamWatcher(void) override  {}
};

class ParamWatcherList {
public:
    ParamWatcherList(void)
    {
        OH_ListInit(&nodeList_);
    }
    ~ParamWatcherList(void) = default;

    bool Empty(void) const
    {
        return nodeList_.next == &nodeList_;
    }
    uint32_t GetNodeCount(void) const
    {
        return nodeCount_;
    }
    int AddNode(WatcherNodePtr node);
    int RemoveNode(WatcherNodePtr node);
    WatcherNodePtr GetNode(uint32_t nodeId);
    WatcherNodePtr GetNextNodeSafe(WatcherNodePtr node);
    WatcherNodePtr GetNextNode(WatcherNodePtr node);
    void TraversalNode(ParamWatcherProcessor handle);
    void TraversalNodeSafe(ParamWatcherProcessor processor);
public:
    ListHead nodeList_ {};
    uint32_t nodeCount_ = 0;
};

class RemoteWatcher : public WatcherNode, public ParamWatcherList {
public:
    RemoteWatcher(uint32_t watcherId, const sptr<IWatcher> &watcher)
        : WatcherNode(watcherId), ParamWatcherList(), watcher_(watcher) {}
    ~RemoteWatcher(void) override
    {
        watcher_ = nullptr;
        TraversalNodeSafe([](ParamWatcherListPtr list, WatcherNodePtr node, uint32_t index) {
            list->RemoveNode(node);
            ParamWatcher *watcher = ConvertTo<ParamWatcher>(node);
            delete watcher;
        });
    }

    uint32_t GetRemoteWatcherId(void) const
    {
        return GetNodeId();
    }
    uint32_t GetAgentId(void) const
    {
        return id_;
    }
    void SetAgentId(uint32_t id)
    {
        id_ = id;
    }
    void ProcessParameterChange(const std::string &prefix, const std::string &name, const std::string &value)
    {
        watcher_->OnParameterChange(prefix, name, value);
    }
    sptr<IWatcher> GetWatcher(void)
    {
        return watcher_;
    }
private:
    uint32_t id_ = { 0 };
    sptr<IWatcher> watcher_ {};
};

class WatcherGroup : public WatcherNode, public ParamWatcherList {
public:
    WatcherGroup(uint32_t groupId, const std::string &key)
        : WatcherNode(groupId), ParamWatcherList(), keyPrefix_(key) {}
    ~WatcherGroup() override
    {
        TraversalNodeSafe([](ParamWatcherListPtr list, WatcherNodePtr node, uint32_t index) {
            list->RemoveNode(node);
            ParamWatcher *watcher = ConvertTo<ParamWatcher>(node);
            delete watcher;
        });
    }
    void ProcessParameterChange(WatcherManager *mananger, const std::string &name, const std::string &value);
    const std::string GetKeyPrefix() const
    {
        return keyPrefix_;
    }
    uint32_t GetGroupId() const
    {
        return GetNodeId();
    }
private:
    std::string keyPrefix_ { };
};
} // namespace init_param
} // namespace OHOS
#endif // WATCHER_MANAGER_H_