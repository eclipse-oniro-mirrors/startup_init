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
#include "param_message.h"

#include <sys/socket.h>
#include <sys/un.h>

#include "param_utils.h"
#include "securec.h"

int ConnectServer(int fd, const char *servername)
{
    PARAM_CHECK(fd >= 0, return -1, "Invalid fd %d", fd);
    PARAM_CHECK(servername != NULL, return -1, "Invalid servername");
    int opt = 1;
    int ret = setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &opt, sizeof(opt));
    PARAM_CHECK(ret == 0, return -1, "Failed to set socket option");
    struct sockaddr_un addr;
    /* fill socket address structure with server's address */
    ret = memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    PARAM_CHECK(ret == 0, return -1, "Failed to memset server address");
    addr.sun_family = AF_UNIX;
    ret = sprintf_s(addr.sun_path, sizeof(addr.sun_path) - 1, "%s", servername);
    PARAM_CHECK(ret > EOK, return -1, "Failed to sprintf_s server address");
    socklen_t len = offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path);
    ret = connect(fd, (struct sockaddr *)&addr, len);
    PARAM_CHECK(ret != -1, return -1, "Failed to connect server %s %d", servername, errno);
    PARAM_LOGV("ConnectServer %s success", servername);
    return 0;
}

int FillParamMsgContent(const ParamMessage *request, uint32_t *start, int type, const char *value, uint32_t length)
{
    PARAM_CHECK(request != NULL && start != NULL, return -1, "Invalid param");
    PARAM_CHECK(value != NULL && length > 0 && length < PARAM_BUFFER_MAX, return -1, "Invalid value");
    uint32_t bufferSize = request->msgSize - sizeof(ParamMessage);
    uint32_t offset = *start;
    PARAM_CHECK((offset + sizeof(ParamMsgContent) + length) <= bufferSize,
        return -1, "Invalid msgSize %u offset %u %d", request->msgSize, offset, type);
    ParamMsgContent *content = (ParamMsgContent *)(request->data + offset);
    content->type = type;
    content->contentSize = length + 1;
    int ret = memcpy_s(content->content, content->contentSize - 1, value, length);
    PARAM_CHECK(ret == EOK, return -1, "Failed to copy value for %d", type);
    content->content[length] = '\0';
    offset += sizeof(ParamMsgContent) + PARAM_ALIGN(content->contentSize);
    *start = offset;
    return 0;
}

ParamMessage *CreateParamMessage(int type, const char *name, uint32_t msgSize)
{
    PARAM_CHECK(name != NULL, return NULL, "Invalid name");
    uint32_t size = msgSize;
    if (msgSize < sizeof(ParamMessage)) {
        size = sizeof(ParamMessage);
    }
    ParamMessage *msg = (ParamMessage *)calloc(1, size);
    PARAM_CHECK(msg != NULL, return NULL, "Failed to malloc message");
    msg->type = type;
    msg->id.msgId = 0;
    msg->msgSize = size;
    int ret = strcpy_s(msg->key, sizeof(msg->key) - 1, name);
    PARAM_CHECK(ret == EOK, free(msg);
        return NULL, "Failed to fill name");
    return msg;
}

ParamMsgContent *GetNextContent(const ParamMessage *reqest, uint32_t *offset)
{
    PARAM_CHECK(reqest != NULL, return NULL, "Invalid reqest");
    PARAM_CHECK(offset != NULL, return NULL, "Invalid offset");
    ParamMessage *msg = (ParamMessage *)reqest;
    if ((msg == NULL) || ((*offset + sizeof(ParamMessage) + sizeof(ParamMsgContent)) >= msg->msgSize)) {
        return NULL;
    }
    ParamMsgContent *content = (ParamMsgContent *)(msg->data + *offset);
    *offset += sizeof(ParamMsgContent) + PARAM_ALIGN(content->contentSize);
    return content;
}
