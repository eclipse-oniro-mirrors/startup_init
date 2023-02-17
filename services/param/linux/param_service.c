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
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "init_log.h"
#include "init_param.h"
#include "init_utils.h"
#include "loop_event.h"
#include "param_base.h"
#include "param_manager.h"
#include "param_message.h"
#include "trigger_manager.h"
#include "securec.h"
#ifdef PARAM_SUPPORT_SELINUX
#include "selinux_parameter.h"
#include <policycoreutils.h>
#include <selinux/selinux.h>
#endif

static ParamService g_paramService = {};

PARAM_STATIC void OnClose(ParamTaskPtr client)
{
    PARAM_LOGV("OnClose client");
    ParamWatcher *watcher = (ParamWatcher *)ParamGetTaskUserData(client);
    if (client == g_paramService.watcherTask) {
        ClearWatchTrigger(watcher, TRIGGER_PARAM_WATCH);
        g_paramService.watcherTask = NULL;
    } else {
        ClearWatchTrigger(watcher, TRIGGER_PARAM_WAIT);
    }
}

static void TimerCallback(const ParamTaskPtr timer, void *context)
{
    UNUSED(context);
    UNUSED(timer);
    int ret = CheckWatchTriggerTimeout();
    // no wait node
    if (ret == 0 && g_paramService.timer != NULL) {
        ParamTaskClose(g_paramService.timer);
        g_paramService.timer = NULL;
    }
}

static void CheckAndSendTrigger(uint32_t dataIndex, const char *name, const char *value)
{
    ParamNode *entry = (ParamNode *)GetTrieNode(GetWorkSpace(name), dataIndex);
    PARAM_CHECK(entry != NULL, return, "Failed to get data %s ", name);
    uint32_t trigger = 1;
    if ((ATOMIC_LOAD_EXPLICIT(&entry->commitId, memory_order_relaxed) & PARAM_FLAGS_TRIGGED) != PARAM_FLAGS_TRIGGED) {
        trigger = (CheckAndMarkTrigger(TRIGGER_PARAM, name) != 0) ? 1 : 0;
    }
    if (trigger) {
        ATOMIC_STORE_EXPLICIT(&entry->commitId,
            ATOMIC_LOAD_EXPLICIT(&entry->commitId, memory_order_relaxed) | PARAM_FLAGS_TRIGGED, memory_order_release);
        // notify event to process trigger
        PostParamTrigger(EVENT_TRIGGER_PARAM, name, value);
    }

    int wait = 1;
    if ((ATOMIC_LOAD_EXPLICIT(&entry->commitId, memory_order_relaxed) & PARAM_FLAGS_WAITED) != PARAM_FLAGS_WAITED) {
        wait = (CheckAndMarkTrigger(TRIGGER_PARAM_WAIT, name) != 0) ? 1 : 0;
    }
    if (wait) {
        ATOMIC_STORE_EXPLICIT(&entry->commitId,
            ATOMIC_LOAD_EXPLICIT(&entry->commitId, memory_order_relaxed) | PARAM_FLAGS_WAITED, memory_order_release);
        PostParamTrigger(EVENT_TRIGGER_PARAM_WAIT, name, value);
    }
    PostParamTrigger(EVENT_TRIGGER_PARAM_WATCH, name, value);
}

static int SendResponseMsg(ParamTaskPtr worker, const ParamMessage *msg, int result)
{
    ParamResponseMessage *response = NULL;
    response = (ParamResponseMessage *)CreateParamMessage(msg->type, msg->key, sizeof(ParamResponseMessage));
    PARAM_CHECK(response != NULL, return PARAM_CODE_ERROR, "Failed to alloc memory for response");
    response->msg.id.msgId = msg->id.msgId;
    response->result = result;
    response->msg.msgSize = sizeof(ParamResponseMessage);
    ParamTaskSendMsg(worker, (ParamMessage *)response);
    PARAM_LOGV("Send response msg msgId %d result %d", msg->id.msgId, result);
    return 0;
}

static int SendWatcherNotifyMessage(const TriggerExtInfo *extData, const char *content, uint32_t size)
{
    PARAM_CHECK(content != NULL, return -1, "Invalid content");
    PARAM_CHECK(extData != NULL && extData->stream != NULL, return -1, "Invalid extData");
    uint32_t msgSize = sizeof(ParamMessage) + PARAM_ALIGN(strlen(content) + 1);
    ParamMessage *msg = (ParamMessage *)CreateParamMessage(MSG_NOTIFY_PARAM, "*", msgSize);
    PARAM_CHECK(msg != NULL, return -1, "Failed to create msg ");

    uint32_t offset = 0;
    int ret;
    char *tmp = strstr(content, "=");
    if (tmp != NULL) {
        ret = strncpy_s(msg->key, sizeof(msg->key) - 1, content, tmp - content);
        PARAM_CHECK(ret == 0, free(msg);
            return -1, "Failed to fill value");
        tmp++;
        ret = FillParamMsgContent(msg, &offset, PARAM_VALUE, tmp, strlen(tmp));
        PARAM_CHECK(ret == 0, free(msg);
            return -1, "Failed to fill value");
    } else if (content != NULL && strlen(content) > 0) {
        ret = FillParamMsgContent(msg, &offset, PARAM_VALUE, content, strlen(content));
        PARAM_CHECK(ret == 0, free(msg);
            return -1, "Failed to fill value");
    }

    msg->id.msgId = 0;
    if (extData->type == TRIGGER_PARAM_WAIT) {
        msg->id.msgId = extData->info.waitInfo.waitId;
    } else {
        msg->id.msgId = extData->info.watchInfo.watchId;
    }
    msg->msgSize = sizeof(ParamMessage) + offset;
    PARAM_LOGV("SendWatcherNotifyMessage cmd %s, id %d msgSize %d para: %s",
        (extData->type == TRIGGER_PARAM_WAIT) ? "wait" : "watcher",
        msg->id.msgId, msg->msgSize, content);
    ParamTaskSendMsg(extData->stream, msg);
    return 0;
}

static int SystemSetParam(const char *name, const char *value, const ParamSecurityLabel *srcLabel)
{
    PARAM_LOGV("SystemWriteParam name %s value: %s", name, value);
    int ctrlService = 0;
    int ret = CheckParameterSet(name, value, srcLabel, &ctrlService);
    PARAM_CHECK(ret == 0, return ret, "Forbid to set parameter %s", name);

    if ((ctrlService & PARAM_CTRL_SERVICE) != PARAM_CTRL_SERVICE) { // ctrl param
        uint32_t dataIndex = 0;
        ret = WriteParam(name, value, &dataIndex, 0);
        PARAM_CHECK(ret == 0, return ret, "Failed to set param %d name %s %s", ret, name, value);
        ret = WritePersistParam(name, value);
        PARAM_CHECK(ret == 0, return ret, "Failed to set persist param name %s", name);
        CheckAndSendTrigger(dataIndex, name, value);
    }
    return ret;
}

static int HandleParamSet(const ParamTaskPtr worker, const ParamMessage *msg)
{
    uint32_t offset = 0;
    ParamMsgContent *valueContent = GetNextContent(msg, &offset);
    PARAM_CHECK(valueContent != NULL, return -1, "Invalid msg for %s", msg->key);
    ParamSecurityLabel srcLabel = {0};
    struct ucred cr = {-1, -1, -1};
    socklen_t crSize = sizeof(cr);
    if (getsockopt(LE_GetSocketFd(worker), SOL_SOCKET, SO_PEERCRED, &cr, &crSize) < 0) {
        PARAM_LOGE("Failed to get opt %d", errno);
        return SendResponseMsg(worker, msg, -1);
    }
    srcLabel.sockFd = LE_GetSocketFd(worker);
    srcLabel.cred.uid = cr.uid;
    srcLabel.cred.pid = cr.pid;
    srcLabel.cred.gid = cr.gid;
    PARAM_LOGI("Handle set param msgId %d pid %d key: %s", msg->id.msgId, cr.pid, msg->key);
    int ret = SystemSetParam(msg->key, valueContent->content, &srcLabel);
    return SendResponseMsg(worker, msg, ret);
}

static int32_t AddWatchNode(struct tagTriggerNode_ *trigger, const struct TriggerExtInfo_ *extInfo)
{
    ParamWatcher *watcher = NULL;
    if (extInfo != NULL && extInfo->stream != NULL) {
        watcher = (ParamWatcher *)ParamGetTaskUserData(extInfo->stream);
    }
    PARAM_CHECK(watcher != NULL, return -1, "Failed to get param watcher data");
    if (extInfo->type == TRIGGER_PARAM_WAIT) {
        WaitNode *node = (WaitNode *)trigger;
        OH_ListInit(&node->item);
        node->timeout = extInfo->info.waitInfo.timeout;
        node->stream = extInfo->stream;
        node->waitId = extInfo->info.waitInfo.waitId;
        OH_ListAddTail(&watcher->triggerHead, &node->item);
    } else {
        WatchNode *node = (WatchNode *)trigger;
        OH_ListInit(&node->item);
        node->watchId = extInfo->info.watchInfo.watchId;
        OH_ListAddTail(&watcher->triggerHead, &node->item);
    }
    return 0;
}

static TriggerNode *AddWatcherTrigger(int triggerType, const char *condition, const TriggerExtInfo *extData)
{
    TriggerWorkSpace *workSpace = GetTriggerWorkSpace();
    TriggerHeader *header = (TriggerHeader *)&workSpace->triggerHead[extData->type];
    return header->addTrigger(workSpace, condition, extData);
}

static int32_t ExecuteWatchTrigger_(const struct tagTriggerNode_ *trigger, const char *content, uint32_t size)
{
    TriggerExtInfo extData = {};
    extData.type = trigger->type;
    if (trigger->type == TRIGGER_PARAM_WAIT) {
        WaitNode *node = (WaitNode *)trigger;
        extData.stream = node->stream;
        extData.info.waitInfo.waitId = node->waitId;
        extData.info.waitInfo.timeout = node->timeout;
    } else {
        WatchNode *node = (WatchNode *)trigger;
        extData.stream = g_paramService.watcherTask;
        extData.info.watchInfo.watchId = node->watchId;
    }
    if (content == NULL) {
        return SendWatcherNotifyMessage(&extData, "", 0);
    }
    return SendWatcherNotifyMessage(&extData, content, size);
}

static int HandleParamWaitAdd(const ParamTaskPtr worker, const ParamMessage *msg)
{
    PARAM_CHECK(msg != NULL, return -1, "Invalid message");
    uint32_t offset = 0;
#ifndef STARTUP_INIT_TEST
    uint32_t timeout = DEFAULT_PARAM_WAIT_TIMEOUT;
#else
    uint32_t timeout = 0;
#endif
    ParamMsgContent *valueContent = GetNextContent(msg, &offset);
    PARAM_CHECK(valueContent != NULL, return -1, "Invalid msg");
    PARAM_CHECK(valueContent->contentSize <= PARAM_CONST_VALUE_LEN_MAX, return -1, "Invalid msg");
    ParamMsgContent *timeoutContent = GetNextContent(msg, &offset);
    if (timeoutContent != NULL) {
        timeout = *((uint32_t *)(timeoutContent->content));
    }

    PARAM_LOGV("HandleParamWaitAdd name %s timeout %d", msg->key, timeout);
    TriggerExtInfo extData = {};
    extData.addNode = AddWatchNode;
    extData.type = TRIGGER_PARAM_WAIT;
    extData.stream = worker;
    extData.info.waitInfo.waitId = msg->id.watcherId;
    extData.info.waitInfo.timeout = timeout;
    // first check match, if match send response to client
    ParamNode *param = SystemCheckMatchParamWait(msg->key, valueContent->content);
    if (param != NULL) {
        SendWatcherNotifyMessage(&extData, param->data, param->valueLength);
        return 0;
    }

    uint32_t buffSize = strlen(msg->key) + valueContent->contentSize + 1 + 1;
    char *condition = calloc(1, buffSize);
    PARAM_CHECK(condition != NULL, return -1, "Failed to create condition for %s", msg->key);
    int ret = sprintf_s(condition, buffSize - 1, "%s=%s", msg->key, valueContent->content);
    PARAM_CHECK(ret > EOK, free(condition);
        return -1, "Failed to copy name for %s", msg->key);
    TriggerNode *trigger = AddWatcherTrigger(TRIGGER_PARAM_WAIT, condition, &extData);
    PARAM_CHECK(trigger != NULL, free(condition);
        return -1, "Failed to add trigger for %s", msg->key);
    free(condition);
    if (g_paramService.timer == NULL) {
        ret = ParamTimerCreate(&g_paramService.timer, TimerCallback, &g_paramService);
        PARAM_CHECK(ret == 0, return 0, "Failed to create timer %s", msg->key);
        ret = ParamTimerStart(g_paramService.timer, MS_UNIT, (uint64_t)-1);
        PARAM_CHECK(ret == 0,
            ParamTaskClose(g_paramService.timer);
            g_paramService.timer = NULL;
            return 0, "Failed to set timer %s", msg->key);
        PARAM_LOGI("Start timer %p", g_paramService.timer);
    }
    return 0;
}

static int HandleParamWatcherAdd(const ParamTaskPtr worker, const ParamMessage *msg)
{
    PARAM_CHECK(msg != NULL, return -1, "Invalid message");
    PARAM_CHECK((g_paramService.watcherTask == NULL) ||
        (g_paramService.watcherTask == worker), return -1, "Invalid watcher worker");
    g_paramService.watcherTask = worker;
    TriggerExtInfo extData = {};
    extData.type = TRIGGER_PARAM_WATCH;
    extData.addNode = AddWatchNode;
    extData.stream = worker;
    extData.info.watchInfo.watchId = msg->id.watcherId;
    TriggerNode *trigger = AddWatcherTrigger(TRIGGER_PARAM_WATCH, msg->key, &extData);
    if (trigger == NULL) {
        PARAM_LOGE("Failed to add trigger for %s", msg->key);
        return SendResponseMsg(worker, msg, -1);
    }
    PARAM_LOGV("HandleParamWatcherAdd name %s watcher: %d", msg->key, msg->id.watcherId);
    return SendResponseMsg(worker, msg, 0);
}

static int HandleParamWatcherDel(const ParamTaskPtr worker, const ParamMessage *msg)
{
    PARAM_CHECK(msg != NULL, return -1, "Invalid message");
    PARAM_LOGV("HandleParamWatcherDel name %s watcher: %d", msg->key, msg->id.watcherId);
    DelWatchTrigger(TRIGGER_PARAM_WATCH, (const void *)&msg->id.watcherId);
    return SendResponseMsg(worker, msg, 0);
}

PARAM_STATIC int ProcessMessage(const ParamTaskPtr worker, const ParamMessage *msg)
{
    PARAM_CHECK((msg != NULL) && (msg->msgSize >= sizeof(ParamMessage)), return -1, "Invalid msg");
    PARAM_CHECK(worker != NULL, return -1, "Invalid worker");
    int ret = PARAM_CODE_INVALID_PARAM;
    switch (msg->type) {
        case MSG_SET_PARAM:
            ret = HandleParamSet(worker, msg);
            break;
        case MSG_WAIT_PARAM:
            ret = HandleParamWaitAdd(worker, msg);
            break;
        case MSG_ADD_WATCHER:
            ret = HandleParamWatcherAdd(worker, msg);
            break;
        case MSG_DEL_WATCHER:
            ret = HandleParamWatcherDel(worker, msg);
            break;
        default:
            break;
    }
    PARAM_CHECK(ret == 0, return -1, "Failed to process message ret %d", ret);
    return 0;
}

PARAM_STATIC int OnIncomingConnect(LoopHandle loop, TaskHandle server)
{
    ParamStreamInfo info = {};
    info.server = NULL;
    info.close = OnClose;
    info.recvMessage = ProcessMessage;
    info.incomingConnect = NULL;
    ParamTaskPtr client = NULL;
    int ret = ParamStreamCreate(&client, server, &info, sizeof(ParamWatcher));
    PARAM_CHECK(ret == 0, return -1, "Failed to create client");

    ParamWatcher *watcher = (ParamWatcher *)ParamGetTaskUserData(client);
    PARAM_CHECK(watcher != NULL, return -1, "Failed to get watcher");
    OH_ListInit(&watcher->triggerHead);
    watcher->stream = client;
#ifdef STARTUP_INIT_TEST
    g_paramService.watcherTask = client;
#endif
    return 0;
}

static void LoadSelinuxLabel(void)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return, "Invalid space");

    // load security label
#ifdef PARAM_SUPPORT_SELINUX
    ParamSecurityOps *ops = GetParamSecurityOps(PARAM_SECURITY_SELINUX);
    if (ops != NULL && ops->securityGetLabel != NULL) {
        ops->securityGetLabel(NULL);
    }
#endif
}

void InitParamService(void)
{
    PARAM_LOGI("InitParamService pipe: %s.", PIPE_NAME);
    CheckAndCreateDir(PIPE_NAME);
    CheckAndCreateDir(PARAM_STORAGE_PATH"/");
    // param space
    PARAM_WORKSPACE_OPS ops = {0};
    ops.updaterMode = InUpdaterMode();
    // init open log
    ops.logFunc = InitLog;
#ifdef PARAM_SUPPORT_SELINUX
    ops.setfilecon = setfilecon;
#endif
    int ret = InitParamWorkSpace(0, &ops);
    PARAM_CHECK(ret == 0, return, "Init parameter workspace fail");
    ret = InitPersistParamWorkSpace();
    PARAM_CHECK(ret == 0, return, "Init persist parameter workspace fail");
    // param server
    if (g_paramService.serverTask == NULL) {
        ParamStreamInfo info = {};
        info.server = PIPE_NAME;
        info.close = NULL;
        info.recvMessage = NULL;
        info.incomingConnect = OnIncomingConnect;
        ret = ParamServerCreate(&g_paramService.serverTask, &info);
        PARAM_CHECK(ret == 0, return, "Failed to create server");
    }

    // init trigger space
    ret = InitTriggerWorkSpace();
    PARAM_CHECK(ret == 0, return, "Failed to init trigger");
    RegisterTriggerExec(TRIGGER_PARAM_WAIT, ExecuteWatchTrigger_);
    RegisterTriggerExec(TRIGGER_PARAM_WATCH, ExecuteWatchTrigger_);
}

void LoadSpecialParam(void)
{
    // read param area size from cfg and save to dac
    LoadParamAreaSize();
    // read selinux label
    LoadSelinuxLabel();
    // from cmdline
    LoadParamFromCmdLine();
    // from build
    LoadParamFromBuild();
}

int StartParamService(void)
{
    return ParamServiceStart();
}

void StopParamService(void)
{
    PARAM_LOGI("StopParamService.");
    ClosePersistParamWorkSpace();
    CloseParamWorkSpace();
    CloseTriggerWorkSpace();
    ParamTaskClose(g_paramService.serverTask);
    g_paramService.serverTask = NULL;
    ParamServiceStop();
}

int SystemWriteParam(const char *name, const char *value)
{
    return SystemSetParam(name, value, GetParamSecurityLabel());
}

#ifdef STARTUP_INIT_TEST
ParamService *GetParamService()
{
    return &g_paramService;
}
#endif
