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

#include "param_service.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "init_param.h"
#include "init_utils.h"
#include "loop_event.h"
#include "param_message.h"
#include "param_manager.h"
#include "param_request.h"
#include "trigger_manager.h"

static ParamWorkSpace g_paramWorkSpace = { 0, {}, NULL, {}, NULL, NULL };

static void OnClose(ParamTaskPtr client)
{
    PARAM_LOGV("OnClose %p", client);
    ParamWatcher *watcher = (ParamWatcher *)ParamGetTaskUserData(client);
    if (client == g_paramWorkSpace.watcherTask) {
        ClearWatchTrigger(watcher, TRIGGER_PARAM_WATCH);
        g_paramWorkSpace.watcherTask = NULL;
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
    if (ret == 0 && g_paramWorkSpace.timer != NULL) {
        ParamTaskClose(g_paramWorkSpace.timer);
        g_paramWorkSpace.timer = NULL;
    }
}

static int AddParam(WorkSpace *workSpace, const char *name, const char *value, uint32_t *dataIndex)
{
    ParamTrieNode *node = AddTrieNode(workSpace, name, strlen(name));
    PARAM_CHECK(node != NULL, return PARAM_CODE_REACHED_MAX, "Failed to add node");
    ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, node->dataIndex);
    if (entry == NULL) {
        uint32_t offset = AddParamNode(workSpace, name, strlen(name), value, strlen(value));
        PARAM_CHECK(offset > 0, return PARAM_CODE_REACHED_MAX, "Failed to allocate name %s", name);
        SaveIndex(&node->dataIndex, offset);
    }
    if (dataIndex != NULL) {
        *dataIndex = node->dataIndex;
    }
    return 0;
}

static int UpdateParam(const WorkSpace *workSpace, uint32_t *dataIndex, const char *name, const char *value)
{
    ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, *dataIndex);
    PARAM_CHECK(entry != NULL, return PARAM_CODE_REACHED_MAX, "Failed to update param value %s %u", name, *dataIndex);
    PARAM_CHECK(entry->keyLength == strlen(name), return PARAM_CODE_INVALID_NAME, "Failed to check name len %s", name);

    uint32_t valueLen = strlen(value);
    uint32_t commitId = atomic_load_explicit(&entry->commitId, memory_order_relaxed);
    atomic_store_explicit(&entry->commitId, commitId | PARAM_FLAGS_MODIFY, memory_order_relaxed);

    if (entry->valueLength < PARAM_VALUE_LEN_MAX && valueLen < PARAM_VALUE_LEN_MAX) {
        int ret = memcpy_s(entry->data + entry->keyLength + 1, PARAM_VALUE_LEN_MAX, value, valueLen + 1);
        PARAM_CHECK(ret == EOK, return PARAM_CODE_INVALID_VALUE, "Failed to copy value");
        entry->valueLength = valueLen;
    }
    uint32_t flags = commitId & ~PARAM_FLAGS_COMMITID;
    atomic_store_explicit(&entry->commitId, (++commitId) | flags, memory_order_release);
    futex_wake(&entry->commitId, INT_MAX);
    return 0;
}

int WriteParam(const WorkSpace *workSpace, const char *name, const char *value, uint32_t *dataIndex, int onlyAdd)
{
    PARAM_CHECK(workSpace != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid workSpace");
    PARAM_CHECK(value != NULL && name != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid name or value");
    ParamTrieNode *node = FindTrieNode(workSpace, name, strlen(name), NULL);
    int ret = CheckParamValue(workSpace, node, name, value);
    PARAM_CHECK(ret == 0, return ret, "Invalid param value param: %s=%s", name, value);
    if (node != NULL && node->dataIndex != 0) {
        if (dataIndex != NULL) {
            *dataIndex = node->dataIndex;
        }
        if (onlyAdd) {
            return 0;
        }
        return UpdateParam(workSpace, &node->dataIndex, name, value);
    }
    return AddParam((WorkSpace *)workSpace, name, value, dataIndex);
}

PARAM_STATIC int AddSecurityLabel(const ParamAuditData *auditData, void *context)
{
    PARAM_CHECK(auditData != NULL && auditData->name != NULL, return -1, "Invalid auditData");
    PARAM_CHECK(context != NULL, return -1, "Invalid context");
    ParamWorkSpace *workSpace = (ParamWorkSpace *)context;
    int ret = CheckParamName(auditData->name, 1);
    PARAM_CHECK(ret == 0, return ret, "Illegal param name %s", auditData->name);

    ParamTrieNode *node = FindTrieNode(&workSpace->paramSpace, auditData->name, strlen(auditData->name), NULL);
    if (node == NULL) {
        node = AddTrieNode(&workSpace->paramSpace, auditData->name, strlen(auditData->name));
    }
    PARAM_CHECK(node != NULL, return PARAM_CODE_REACHED_MAX, "Failed to add node %s", auditData->name);
    if (node->labelIndex == 0) { // can not support update for label
        uint32_t offset = AddParamSecruityNode(&workSpace->paramSpace, auditData);
        PARAM_CHECK(offset != 0, return PARAM_CODE_REACHED_MAX, "Failed to add label");
        SaveIndex(&node->labelIndex, offset);
    } else {
#ifdef STARTUP_INIT_TEST
        ParamSecruityNode *label = (ParamSecruityNode *)GetTrieNode(&workSpace->paramSpace, node->labelIndex);
        label->mode = auditData->dacData.mode;
        label->uid = auditData->dacData.uid;
        label->gid = auditData->dacData.gid;
#endif
        PARAM_LOGE("Error, repeate to add label for name %s", auditData->name);
    }
    PARAM_LOGV("AddSecurityLabel label gid %d uid %d mode %o name: %s", auditData->dacData.gid, auditData->dacData.uid,
               auditData->dacData.mode, auditData->name);
    return 0;
}

static char *BuildKey(ParamWorkSpace *workSpace, const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    size_t buffSize = sizeof(workSpace->buffer);
    int len = vsnprintf_s(workSpace->buffer, buffSize, buffSize - 1, format, vargs);
    va_end(vargs);
    if (len > 0 && (size_t)len < buffSize) {
        workSpace->buffer[len] = '\0';
        for (int i = 0; i < len; i++) {
            if (workSpace->buffer[i] == '|') {
                workSpace->buffer[i] = '\0';
            }
        }
        return workSpace->buffer;
    }
    return NULL;
}

static char *GetServiceCtrlName(ParamWorkSpace *workSpace, const char *name, const char *value)
{
    static char *ctrlParam[] = {
        "ohos.ctl.start",
        "ohos.ctl.stop"
    };
    static char *installParam[] = {
        "ohos.servicectrl."
    };
    static char *powerCtrlArg[][2] = {
        {"reboot,shutdown", "reboot.shutdown"},
        {"reboot,updater", "reboot.updater"},
        {"reboot,flashd", "reboot.flashd"},
        {"reboot", "reboot"},
    };
    char *key = NULL;
    if (strcmp("ohos.startup.powerctrl", name) == 0) {
        for (size_t i = 0; i < ARRAY_LENGTH(powerCtrlArg); i++) {
            if (strncmp(value, powerCtrlArg[i][0], strlen(powerCtrlArg[i][0])) == 0) {
                return BuildKey(workSpace, "%s%s", OHOS_SERVICE_CTRL_PREFIX, powerCtrlArg[i][1]);
            }
        }
        return key;
    }
    for (size_t i = 0; i < ARRAY_LENGTH(ctrlParam); i++) {
        if (strcmp(name, ctrlParam[i]) == 0) {
            return BuildKey(workSpace, "%s%s", OHOS_SERVICE_CTRL_PREFIX, value);
        }
    }

    for (size_t i = 0; i < ARRAY_LENGTH(installParam); i++) {
        if (strncmp(name, installParam[i], strlen(installParam[i])) == 0) {
            return BuildKey(workSpace, "%s.%s", name, value);
        }
    }
    return key;
}

static void CheckAndSendTrigger(ParamWorkSpace *workSpace, uint32_t dataIndex, const char *name, const char *value)
{
    ParamNode *entry = (ParamNode *)GetTrieNode(&workSpace->paramSpace, dataIndex);
    PARAM_CHECK(entry != NULL, return, "Failed to get data %s ", name);
    uint32_t trigger = 1;
    if ((atomic_load_explicit(&entry->commitId, memory_order_relaxed) & PARAM_FLAGS_TRIGGED) != PARAM_FLAGS_TRIGGED) {
        trigger = (CheckAndMarkTrigger(TRIGGER_PARAM, name) != 0) ? 1 : 0;
    }
    if (trigger) {
        atomic_store_explicit(&entry->commitId,
            atomic_load_explicit(&entry->commitId, memory_order_relaxed) | PARAM_FLAGS_TRIGGED, memory_order_release);
        // notify event to process trigger
        PostParamTrigger(EVENT_TRIGGER_PARAM, name, value);
    }

    int wait = 1;
    if ((atomic_load_explicit(&entry->commitId, memory_order_relaxed) & PARAM_FLAGS_WAITED) != PARAM_FLAGS_WAITED) {
        wait = (CheckAndMarkTrigger(TRIGGER_PARAM_WAIT, name) != 0) ? 1 : 0;
    }
    if (wait) {
        atomic_store_explicit(&entry->commitId,
            atomic_load_explicit(&entry->commitId, memory_order_relaxed) | PARAM_FLAGS_WAITED, memory_order_release);
        PostParamTrigger(EVENT_TRIGGER_PARAM_WAIT, name, value);
    }
    PostParamTrigger(EVENT_TRIGGER_PARAM_WATCH, name, value);
}

static int SystemSetParam(const char *name, const char *value, const ParamSecurityLabel *srcLabel)
{
    PARAM_LOGV("SystemSetParam name %s value: %s", name, value);
    int ret = CheckParamName(name, 0);
    PARAM_CHECK(ret == 0, return ret, "Illegal param name %s", name);
    char *key = GetServiceCtrlName(&g_paramWorkSpace, name, value);
    if (srcLabel != NULL) {
        ret = CheckParamPermission(&g_paramWorkSpace, srcLabel, (key == NULL) ? name : key, DAC_WRITE);
    }
    PARAM_CHECK(ret == 0, return ret, "Forbit to set parameter %s", name);

    if (key != NULL) { // ctrl param
        ret = CheckParamValue(&g_paramWorkSpace.paramSpace, NULL, name, value);
        PARAM_CHECK(ret == 0, return ret, "Invalid param value param: %s=%s", name, value);
        PostParamTrigger(EVENT_TRIGGER_PARAM, name, value);
    } else {
        uint32_t dataIndex = 0;
        ret = WriteParam(&g_paramWorkSpace.paramSpace, name, value, &dataIndex, 0);
        PARAM_CHECK(ret == 0, return ret, "Failed to set param %d name %s %s", ret, name, value);
        ret = WritePersistParam(&g_paramWorkSpace, name, value);
        PARAM_CHECK(ret == 0, return ret, "Failed to set persist param name %s", name);
        CheckAndSendTrigger(&g_paramWorkSpace, dataIndex, name, value);
    }
    return ret;
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

static int HandleParamSet(const ParamTaskPtr worker, const ParamMessage *msg)
{
    uint32_t offset = 0;
    ParamMsgContent *valueContent = GetNextContent(msg, &offset);
    PARAM_CHECK(valueContent != NULL, return -1, "Invalid msg for %s", msg->key);
    int ret;
    ParamMsgContent *lableContent =  GetNextContent(msg, &offset);
    ParamSecurityLabel *srcLabel = NULL;
    if (lableContent != NULL && lableContent->contentSize != 0) {
        PARAM_CHECK(g_paramWorkSpace.paramSecurityOps.securityDecodeLabel != NULL,
            return -1, "Can not get decode function");
        ret = g_paramWorkSpace.paramSecurityOps.securityDecodeLabel(&srcLabel,
            lableContent->content, lableContent->contentSize);
        PARAM_CHECK(ret == 0, return ret,
            "Failed to decode param %d name %s %s", ret, msg->key, valueContent->content);
    }
    if (srcLabel != NULL) {
        struct ucred cr = {-1, -1, -1};
        socklen_t crSize = sizeof(cr);
        if (getsockopt(LE_GetSocketFd(worker), SOL_SOCKET, SO_PEERCRED, &cr, &crSize) < 0) {
            PARAM_LOGE("Failed to get opt %d", errno);
            return SendResponseMsg(worker, msg, -1);
        }
        srcLabel->cred.uid = cr.uid;
        srcLabel->cred.pid = cr.pid;
        srcLabel->cred.gid = cr.gid;
    }
    ret = SystemSetParam(msg->key, valueContent->content, srcLabel);
    if (srcLabel != NULL && g_paramWorkSpace.paramSecurityOps.securityFreeLabel != NULL) {
        g_paramWorkSpace.paramSecurityOps.securityFreeLabel(srcLabel);
    }
    return SendResponseMsg(worker, msg, ret);
}

static ParamNode *CheckMatchParamWait(const ParamWorkSpace *worksapce, const char *name, const char *value)
{
    uint32_t nameLength = strlen(name);
    ParamTrieNode *node = FindTrieNode(&worksapce->paramSpace, name, nameLength, NULL);
    if (node == NULL || node->dataIndex == 0) {
        return NULL;
    }
    ParamNode *param = (ParamNode *)GetTrieNode(&worksapce->paramSpace, node->dataIndex);
    if (param == NULL) {
        return NULL;
    }
    if ((param->keyLength != nameLength) || (strncmp(param->data, name, nameLength) != 0)) { // compare name
        return NULL;
    }
    atomic_store_explicit(&param->commitId,
        atomic_load_explicit(&param->commitId, memory_order_relaxed) | PARAM_FLAGS_WAITED, memory_order_release);
    if ((strncmp(value, "*", 1) == 0) || (strcmp(param->data + nameLength + 1, value) == 0)) { // compare value
        return param;
    }
    char *tmp = strstr(value, "*");
    if (tmp != NULL && (strncmp(param->data + nameLength + 1, value, tmp - value) == 0)) {
        return param;
    }
    return NULL;
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
        ListInit(&node->item);
        node->timeout = extInfo->info.waitInfo.timeout;
        node->stream = extInfo->stream;
        node->waitId = extInfo->info.waitInfo.waitId;
        ListAddTail(&watcher->triggerHead, &node->item);
    } else {
        WatchNode *node = (WatchNode *)trigger;
        ListInit(&node->item);
        node->watchId = extInfo->info.watchInfo.watchId;
        ListAddTail(&watcher->triggerHead, &node->item);
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
        extData.stream = g_paramWorkSpace.watcherTask;
        extData.info.watchInfo.watchId = node->watchId;
    }
    if (content == NULL) {
        return SendWatcherNotifyMessage(&extData, "", 0);
    }
    return SendWatcherNotifyMessage(&extData, content, size);
}

static int HandleParamWaitAdd(const ParamWorkSpace *worksapce, const ParamTaskPtr worker, const ParamMessage *msg)
{
    PARAM_CHECK(msg != NULL, return -1, "Invalid message");
    uint32_t offset = 0;
    uint32_t timeout = DEFAULT_PARAM_WAIT_TIMEOUT;
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
    ParamNode *param = CheckMatchParamWait(worksapce, msg->key, valueContent->content);
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
    if (g_paramWorkSpace.timer == NULL) {
        ret = ParamTimerCreate(&g_paramWorkSpace.timer, TimerCallback, &g_paramWorkSpace);
        PARAM_CHECK(ret == 0, return 0, "Failed to create timer %s", msg->key);
        ret = ParamTimerStart(g_paramWorkSpace.timer, MS_UNIT, (uint64_t)-1);
        PARAM_CHECK(ret == 0,
            ParamTaskClose(g_paramWorkSpace.timer);
            g_paramWorkSpace.timer = NULL;
            return 0, "Failed to set timer %s", msg->key);
        PARAM_LOGI("Start timer %p", g_paramWorkSpace.timer);
    }
    return 0;
}

static int HandleParamWatcherAdd(ParamWorkSpace *workSpace, const ParamTaskPtr worker, const ParamMessage *msg)
{
    PARAM_CHECK(msg != NULL, return -1, "Invalid message");
    PARAM_CHECK((g_paramWorkSpace.watcherTask == NULL) ||
        (g_paramWorkSpace.watcherTask == worker), return -1, "Invalid watcher worker");
    g_paramWorkSpace.watcherTask = worker;
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

static int HandleParamWatcherDel(ParamWorkSpace *workSpace, const ParamTaskPtr worker, const ParamMessage *msg)
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
            ret = HandleParamWaitAdd(&g_paramWorkSpace, worker, msg);
            break;
        case MSG_ADD_WATCHER:
            ret = HandleParamWatcherAdd(&g_paramWorkSpace, worker, msg);
            break;
        case MSG_DEL_WATCHER:
            ret = HandleParamWatcherDel(&g_paramWorkSpace, worker, msg);
            break;
        default:
            break;
    }
    PARAM_CHECK(ret == 0, return -1, "Failed to process message ret %d", ret);
    return 0;
}

static int LoadOneParam_ (const uint32_t *context, const char *name, const char *value)
{
    uint32_t mode = *(uint32_t *)context;
    int ret = CheckParamName(name, 0);
    if (ret != 0) {
        return 0;
    }
    PARAM_LOGV("Add default parameter [%s] [%s]", name, value);
    return WriteParam(&g_paramWorkSpace.paramSpace,
        name, value, NULL, mode & LOAD_PARAM_ONLY_ADD);
}

static int LoadDefaultParam_ (const char *fileName, uint32_t mode, const char *exclude[], uint32_t count)
{
    uint32_t paramNum = 0;
    FILE *fp = fopen(fileName, "r");
    if (fp == NULL) {
        return -1;
    }

    while (fgets(g_paramWorkSpace.buffer, sizeof(g_paramWorkSpace.buffer), fp) != NULL) {
        g_paramWorkSpace.buffer[sizeof(g_paramWorkSpace.buffer) - 1] = '\0';
        int ret = SpliteString(g_paramWorkSpace.buffer, exclude, count, LoadOneParam_, &mode);
        PARAM_CHECK(ret == 0, continue, "Failed to set param %d %s", ret, g_paramWorkSpace.buffer);
        paramNum++;
    }
    (void)fclose(fp);

    PARAM_LOGI("Load parameters success %s total %u", fileName, paramNum);
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
    ListInit(&watcher->triggerHead);
    watcher->stream = client;
#ifdef STARTUP_INIT_TEST
    GetParamWorkSpace()->watcherTask = client;
#endif
    return 0;
}

static int GetParamValueFromBuffer(const char *name, const char *buffer, char *value, int length)
{
    size_t bootLen = strlen(OHOS_BOOT);
    const char *tmpName = name + bootLen;
    int ret = GetProcCmdlineValue(tmpName, buffer, value, length);
    return ret;
}

static int LoadParamFromCmdLine(void)
{
    int ret;
    static const char *cmdLines[] = {
        OHOS_BOOT"hardware",
        OHOS_BOOT"bootgroup",
        OHOS_BOOT"reboot_reason",
#ifdef STARTUP_INIT_TEST
        OHOS_BOOT"mem",
        OHOS_BOOT"console",
        OHOS_BOOT"mmz",
        OHOS_BOOT"androidboot.selinux",
        OHOS_BOOT"init",
        OHOS_BOOT"root",
        OHOS_BOOT"uuid",
        OHOS_BOOT"rootfstype",
        OHOS_BOOT"blkdevparts"
#endif
    };
    char *data = ReadFileData(PARAM_CMD_LINE);
    PARAM_CHECK(data != NULL, return -1, "Failed to read file %s", PARAM_CMD_LINE);
    char *value = calloc(1, PARAM_CONST_VALUE_LEN_MAX + 1);
    PARAM_CHECK(value != NULL, free(data);
        return -1, "Failed to read file %s", PARAM_CMD_LINE);

    for (size_t i = 0; i < ARRAY_LENGTH(cmdLines); i++) {
#ifdef BOOT_EXTENDED_CMDLINE
        ret = GetParamValueFromBuffer(cmdLines[i], BOOT_EXTENDED_CMDLINE, value, PARAM_CONST_VALUE_LEN_MAX);
        if (ret != 0) {
            ret = GetParamValueFromBuffer(cmdLines[i], data, value, PARAM_CONST_VALUE_LEN_MAX);
        }
#else
        ret = GetParamValueFromBuffer(cmdLines[i], data, value, PARAM_CONST_VALUE_LEN_MAX);
#endif
        if (ret == 0) {
            PARAM_LOGV("Add param from cmdline %s %s", cmdLines[i], value);
            ret = CheckParamName(cmdLines[i], 0);
            PARAM_CHECK(ret == 0, break, "Invalid name %s", cmdLines[i]);
            PARAM_LOGV("**** cmdLines[%d] %s, value %s", i, cmdLines[i], value);
            ret = WriteParam(&g_paramWorkSpace.paramSpace, cmdLines[i], value, NULL, 0);
            PARAM_CHECK(ret == 0, break, "Failed to write param %s %s", cmdLines[i], value);
        } else {
            PARAM_LOGE("Can not find arrt %s", cmdLines[i]);
        }
    }
    PARAM_LOGV("Parse cmdline finish %s", PARAM_CMD_LINE);
    free(data);
    free(value);
    return 0;
}

int SystemWriteParam(const char *name, const char *value)
{
    PARAM_CHECK(name != NULL && value != NULL, return -1, "The name is null");
    return SystemSetParam(name, value, g_paramWorkSpace.securityLabel);
}

int SystemReadParam(const char *name, char *value, unsigned int *len)
{
    PARAM_CHECK(name != NULL && len != NULL, return -1, "The name is null");
    ParamHandle handle = 0;
    int ret = ReadParamWithCheck(&g_paramWorkSpace, name, DAC_READ, &handle);
    if (ret == 0) {
        ret = ReadParamValue(&g_paramWorkSpace, handle, value, len);
    }
    return ret;
}

int LoadPersistParams(void)
{
    return LoadPersistParam(&g_paramWorkSpace);
}

static int ProcessParamFile(const char *fileName, void *context)
{
    static const char *exclude[] = {"ctl.", "selinux.restorecon_recursive"};
    uint32_t mode = *(int *)context;
    return LoadDefaultParam_(fileName, mode, exclude, ARRAY_LENGTH(exclude));
}

int LoadDefaultParams(const char *fileName, uint32_t mode)
{
    PARAM_CHECK(fileName != NULL, return -1, "Invalid fielname for load");
    if (!PARAM_TEST_FLAG(g_paramWorkSpace.flags, WORKSPACE_FLAGS_INIT)) {
        return PARAM_CODE_NOT_INIT;
    }
    PARAM_LOGI("load default parameters %s.", fileName);
    int ret = 0;
    struct stat st;
    if ((stat(fileName, &st) == 0) && !S_ISDIR(st.st_mode)) {
        ret = ProcessParamFile(fileName, &mode);
    } else {
        ret = ReadFileInDir(fileName, ".para", ProcessParamFile, &mode);
    }

    // load security label
    ParamSecurityOps *ops = &g_paramWorkSpace.paramSecurityOps;
    if (ops->securityGetLabel != NULL) {
        ret = ops->securityGetLabel(AddSecurityLabel, fileName, (void *)&g_paramWorkSpace);
    }
    return ret;
}

void InitParamService(void)
{
    PARAM_LOGI("InitParamService pipe: %s.", PIPE_NAME);
    CheckAndCreateDir(PIPE_NAME);
    int ret = InitParamWorkSpace(&g_paramWorkSpace, 0);
    PARAM_CHECK(ret == 0, return, "Init parameter workspace fail");
    ret = InitPersistParamWorkSpace(&g_paramWorkSpace);
    PARAM_CHECK(ret == 0, return, "Init persist parameter workspace fail");
    if (g_paramWorkSpace.serverTask == NULL) {
        ParamStreamInfo info = {};
        info.server = PIPE_NAME;
        info.close = NULL;
        info.recvMessage = NULL;
        info.incomingConnect = OnIncomingConnect;
        ret = ParamServerCreate(&g_paramWorkSpace.serverTask, &info);
        PARAM_CHECK(ret == 0, return, "Failed to create server");
    }
    ret = InitTriggerWorkSpace();
    PARAM_CHECK(ret == 0, return, "Failed to init trigger");

    RegisterTriggerExec(TRIGGER_PARAM_WAIT, ExecuteWatchTrigger_);
    RegisterTriggerExec(TRIGGER_PARAM_WATCH, ExecuteWatchTrigger_);
    ParamAuditData auditData = {};
    auditData.name = "#";
    auditData.label = NULL;
    auditData.dacData.gid = getegid();
    auditData.dacData.uid = geteuid();
    auditData.dacData.mode = DAC_ALL_PERMISSION;
    ret = AddSecurityLabel(&auditData, (void *)&g_paramWorkSpace);
    PARAM_CHECK(ret == 0, return, "Failed to add default dac label");

    // 读取cmdline的参数
    LoadParamFromCmdLine();
}

int StartParamService(void)
{
    return ParamServiceStart();
}

void StopParamService(void)
{
    PARAM_LOGI("StopParamService.");
    ClosePersistParamWorkSpace();
    CloseParamWorkSpace(&g_paramWorkSpace);
    CloseTriggerWorkSpace();
    ParamTaskClose(g_paramWorkSpace.serverTask);
    g_paramWorkSpace.serverTask = NULL;
    ParamServiceStop();
}

ParamWorkSpace *GetParamWorkSpace(void)
{
    return &g_paramWorkSpace;
}

void DumpParametersAndTriggers(void)
{
    DumpParameters(&g_paramWorkSpace, 1);
    if (GetTriggerWorkSpace() != NULL) {
        DumpTrigger(GetTriggerWorkSpace());
    }
}
