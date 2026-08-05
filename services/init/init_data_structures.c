/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "init_data_structures.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "init_log.h"
#include "securec.h"

#ifndef INIT_STATIC
#define INIT_STATIC static
#endif

INIT_STATIC size_t DataStructMin(size_t a, size_t b)
{
    return (a < b) ? a : b;
}

/* ========================================================================== */
/* Priority Queue Implementation                                               */
/* ========================================================================== */

INIT_STATIC void PriorityQueueInitHeads(PriorityQueue *queue)
{
    for (int i = 0; i < PRIORITY_COUNT; i++) {
        ListInit(&queue->headNodes[i]);
    }
}

INIT_STATIC bool IsPriorityValid(PriorityLevel priority)
{
    return (priority >= PRIORITY_LOWEST && priority < PRIORITY_COUNT);
}

INIT_STATIC PriorityQueueNode *CreateQueueNode(void *data, int64_t priority,
    uint64_t sequenceId, uint32_t tag)
{
    PriorityQueueNode *node = (PriorityQueueNode *)calloc(1, sizeof(PriorityQueueNode));
    if (node == NULL) {
        return NULL;
    }

    ListInit(&node->linkNode);
    node->userData = data;
    node->priority = priority;
    node->sequenceId = sequenceId;
    node->tag = tag;
    node->isActive = true;
    return node;
}

INIT_STATIC void DestroyQueueNode(PriorityQueueNode *node, void (*freeFunc)(void *))
{
    if (node == NULL) {
        return;
    }

    if (freeFunc != NULL && node->userData != NULL) {
        freeFunc(node->userData);
    }

    node->userData = NULL;
    node->isActive = false;
    free(node);
}

INIT_STATIC void PriorityQueueInsertSorted(PriorityQueue *queue, PriorityQueueNode *newNode)
{
    ListNode *head = &queue->headNodes[newNode->priority];
    ListNode *pos = head->next;

    /* Insert maintaining sequence order within the same priority level */
    while (pos != head) {
        PriorityQueueNode *existingNode = ListEntry(pos, PriorityQueueNode, linkNode);
        if (existingNode->sequenceId > newNode->sequenceId) {
            break;
        }
        pos = pos->next;
    }

    /* Insert before pos */
    ListNode *prev = pos->prev;
    newNode->linkNode.next = pos;
    newNode->linkNode.prev = prev;
    prev->next = &newNode->linkNode;
    pos->prev = &newNode->linkNode;
}

int InitPriorityQueue(PriorityQueue *queue, size_t maxCapacity,
    int (*compareFunc)(const void *, const void *), void (*freeFunc)(void *))
{
    if (queue == NULL) {
        INIT_LOG_ERROR("InitPriorityQueue: NULL queue pointer");
        return -1;
    }

    if (maxCapacity == 0 || maxCapacity > PRIORITY_QUEUE_MAX_CAPACITY) {
        INIT_LOG_ERROR("InitPriorityQueue: invalid capacity %zu", maxCapacity);
        return -1;
    }

    PriorityQueueInitHeads(queue);
    queue->totalCount = 0;
    queue->maxCapacity = maxCapacity;
    queue->nextSequenceId = 0;
    queue->dataCompare = compareFunc;
    queue->dataFree = freeFunc;

    return 0;
}

void DestroyPriorityQueue(PriorityQueue *queue)
{
    if (queue == NULL) {
        return;
    }

    for (int i = 0; i < PRIORITY_COUNT; i++) {
        ListNode *head = &queue->headNodes[i];
        ListNode *pos = head->next;

        while (pos != head) {
            PriorityQueueNode *node = ListEntry(pos, PriorityQueueNode, linkNode);
            ListNode *nextPos = pos->next;
            DestroyQueueNode(node, queue->dataFree);
            pos = nextPos;
        }
        ListInit(head);
    }

    queue->totalCount = 0;
    queue->nextSequenceId = 0;
}

int PriorityQueuePush(PriorityQueue *queue, void *data, PriorityLevel priority, uint32_t tag)
{
    if (queue == NULL || data == NULL) {
        INIT_LOG_ERROR("PriorityQueuePush: invalid parameters");
        return -1;
    }

    if (!IsPriorityValid(priority)) {
        INIT_LOG_ERROR("PriorityQueuePush: invalid priority %d", priority);
        return -1;
    }

    if (queue->totalCount >= queue->maxCapacity) {
        INIT_LOG_ERROR("PriorityQueuePush: queue is full (count=%zu, capacity=%zu)",
            queue->totalCount, queue->maxCapacity);
        return -1;
    }

    PriorityQueueNode *node = CreateQueueNode(data, (int64_t)priority,
        queue->nextSequenceId++, tag);
    if (node == NULL) {
        INIT_LOG_ERROR("PriorityQueuePush: failed to allocate node");
        return -1;
    }

    PriorityQueueInsertSorted(queue, node);
    queue->totalCount++;

    return 0;
}

void *PriorityQueuePop(PriorityQueue *queue)
{
    if (queue == NULL) {
        return NULL;
    }

    /* Find the highest priority non-empty level */
    for (int i = PRIORITY_COUNT - 1; i >= 0; i--) {
        ListNode *head = &queue->headNodes[i];
        if (!ListIsEmpty(head)) {
            PriorityQueueNode *node = ListEntry(head->next, PriorityQueueNode, linkNode);
            void *data = node->userData;
            node->userData = NULL;

            ListRemoveNode(&node->linkNode);
            free(node);
            queue->totalCount--;

            return data;
        }
    }

    return NULL;
}

void *PriorityQueuePeek(const PriorityQueue *queue)
{
    if (queue == NULL) {
        return NULL;
    }

    for (int i = PRIORITY_COUNT - 1; i >= 0; i--) {
        const ListNode *head = &queue->headNodes[i];
        if (!ListIsEmpty(head)) {
            PriorityQueueNode *node = ListEntry(head->next, PriorityQueueNode, linkNode);
            return node->userData;
        }
    }

    return NULL;
}

size_t PriorityQueueSize(const PriorityQueue *queue)
{
    if (queue == NULL) {
        return 0;
    }
    return queue->totalCount;
}

bool PriorityQueueIsEmpty(const PriorityQueue *queue)
{
    if (queue == NULL) {
        return true;
    }
    return (queue->totalCount == 0);
}

INIT_STATIC size_t RemoveNodesByTagFromList(PriorityQueue *queue, ListNode *head, uint32_t tag)
{
    size_t removed = 0;
    ListNode *pos = head->next;

    while (pos != head) {
        PriorityQueueNode *node = ListEntry(pos, PriorityQueueNode, linkNode);
        ListNode *nextPos = pos->next;

        if (node->tag == tag) {
            DestroyQueueNode(node, queue->dataFree);
            removed++;
        }

        pos = nextPos;
    }

    return removed;
}

size_t PriorityQueueRemoveByTag(PriorityQueue *queue, uint32_t tag)
{
    if (queue == NULL) {
        return 0;
    }

    size_t removedCount = 0;
    for (int i = 0; i < PRIORITY_COUNT; i++) {
        removedCount += RemoveNodesByTagFromList(queue, &queue->headNodes[i], tag);
    }

    queue->totalCount -= removedCount;
    return removedCount;
}

/* ========================================================================== */
/* Ring Buffer Implementation                                                  */
/* ========================================================================== */

INIT_STATIC size_t RingBufferAdvanceIndex(size_t index, size_t increment, size_t capacity)
{
    return ((index + increment) < capacity) ? (index + increment) : (index + increment - capacity);
}

INIT_STATIC bool RingBufferIsEmptyInternal(const RingBuffer *rb)
{
    return (!rb->isFull && rb->readIndex == rb->writeIndex);
}

int InitRingBuffer(RingBuffer *rb, size_t capacity, bool allowOverwrite)
{
    if (rb == NULL || capacity == 0) {
        INIT_LOG_ERROR("InitRingBuffer: invalid parameters");
        return -1;
    }

    rb->buffer = (uint8_t *)calloc(1, capacity);
    if (rb->buffer == NULL) {
        INIT_LOG_ERROR("InitRingBuffer: failed to allocate buffer");
        return -1;
    }

    rb->capacity = capacity;
    rb->readIndex = 0;
    rb->writeIndex = 0;
    rb->dataSize = 0;
    rb->isFull = false;
    rb->isOverwrite = allowOverwrite;
    rb->overflowCount = 0;
    rb->underflowCount = 0;

    return 0;
}

void DestroyRingBuffer(RingBuffer *rb)
{
    if (rb == NULL) {
        return;
    }

    if (rb->buffer != NULL) {
        free(rb->buffer);
        rb->buffer = NULL;
    }

    rb->capacity = 0;
    rb->readIndex = 0;
    rb->writeIndex = 0;
    rb->dataSize = 0;
    rb->isFull = false;
}

size_t RingBufferWrite(RingBuffer *rb, const uint8_t *data, size_t length)
{
    if (rb == NULL || data == NULL || length == 0) {
        return 0;
    }

    size_t freeSpace = RingBufferFreeSpace(rb);
    size_t toWrite = DataStructMin(length, (rb->isOverwrite ? length : freeSpace));
    size_t written = 0;

    if (rb->isOverwrite && length > freeSpace) {
        /* Advance read index to make room */
        size_t overrun = length - freeSpace;
        rb->readIndex = RingBufferAdvanceIndex(rb->readIndex, overrun, rb->capacity);
        rb->overflowCount++;
        rb->isFull = false;
    }

    while (written < toWrite) {
        size_t spaceToEnd = rb->capacity - rb->writeIndex;
        size_t chunkSize = DataStructMin(toWrite - written, spaceToEnd);

        errno_t ret = memcpy_s(rb->buffer + rb->writeIndex, spaceToEnd, data + written, chunkSize);
        if (ret != 0) {
            break;
        }
        rb->writeIndex = RingBufferAdvanceIndex(rb->writeIndex, chunkSize, rb->capacity);
        written += chunkSize;
    }

    size_t newDataSize = rb->dataSize + written;
    if (newDataSize > rb->capacity) {
        rb->dataSize = rb->capacity;
        rb->isFull = true;
    } else {
        rb->dataSize = newDataSize;
        if (rb->dataSize == rb->capacity) {
            rb->isFull = true;
        }
    }

    return written;
}

size_t RingBufferRead(RingBuffer *rb, uint8_t *dest, size_t maxLength)
{
    if (rb == NULL || dest == NULL || maxLength == 0) {
        return 0;
    }

    if (RingBufferIsEmptyInternal(rb)) {
        rb->underflowCount++;
        return 0;
    }

    size_t available = RingBufferDataSize(rb);
    size_t toRead = DataStructMin(maxLength, available);
    size_t totalRead = 0;

    while (totalRead < toRead) {
        size_t spaceToEnd = rb->capacity - rb->readIndex;
        size_t chunkSize = DataStructMin(toRead - totalRead, spaceToEnd);

        errno_t ret = memcpy_s(dest + totalRead, maxLength - totalRead, rb->buffer + rb->readIndex, chunkSize);
        if (ret != 0) {
            break;
        }
        rb->readIndex = RingBufferAdvanceIndex(rb->readIndex, chunkSize, rb->capacity);
        totalRead += chunkSize;
    }

    rb->dataSize -= totalRead;
    rb->isFull = false;

    return totalRead;
}

size_t RingBufferPeek(const RingBuffer *rb, uint8_t *dest, size_t maxLength, size_t offset)
{
    if (rb == NULL || dest == NULL || maxLength == 0) {
        return 0;
    }

    size_t available = RingBufferDataSize(rb);
    if (offset >= available) {
        return 0;
    }

    size_t remainingAfterOffset = available - offset;
    size_t toPeek = DataStructMin(maxLength, remainingAfterOffset);
    size_t peekIndex = RingBufferAdvanceIndex(rb->readIndex, offset, rb->capacity);
    size_t totalPeeked = 0;

    while (totalPeeked < toPeek) {
        size_t spaceToEnd = rb->capacity - peekIndex;
        size_t chunkSize = DataStructMin(toPeek - totalPeeked, spaceToEnd);

        errno_t ret = memcpy_s(dest + totalPeeked, maxLength - totalPeeked, rb->buffer + peekIndex, chunkSize);
        if (ret != 0) {
            break;
        }
        peekIndex = RingBufferAdvanceIndex(peekIndex, chunkSize, rb->capacity);
        totalPeeked += chunkSize;
    }

    return totalPeeked;
}

size_t RingBufferDataSize(const RingBuffer *rb)
{
    if (rb == NULL) {
        return 0;
    }
    return rb->dataSize;
}

size_t RingBufferFreeSpace(const RingBuffer *rb)
{
    if (rb == NULL) {
        return 0;
    }
    return rb->capacity - rb->dataSize;
}

void RingBufferClear(RingBuffer *rb)
{
    if (rb == NULL) {
        return;
    }

    rb->readIndex = 0;
    rb->writeIndex = 0;
    rb->dataSize = 0;
    rb->isFull = false;
}

void RingBufferResetCounters(RingBuffer *rb)
{
    if (rb == NULL) {
        return;
    }

    rb->overflowCount = 0;
    rb->underflowCount = 0;
}

/* ========================================================================== */
/* Reference-counted Buffer Implementation                                     */
/* ========================================================================== */

RefCountedBuffer *CreateRefCountedBuffer(void *data, size_t size, void (*destroyFunc)(void *))
{
    if (data == NULL || size == 0) {
        INIT_LOG_ERROR("CreateRefCountedBuffer: invalid data or size");
        return NULL;
    }

    RefCountedBuffer *buf = (RefCountedBuffer *)calloc(1, sizeof(RefCountedBuffer));
    if (buf == NULL) {
        INIT_LOG_ERROR("CreateRefCountedBuffer: failed to allocate buffer");
        return NULL;
    }

    buf->data = data;
    buf->size = size;
    buf->refCount = 1;
    buf->destroyFunc = destroyFunc;
    buf->flags = 0;
    buf->allocId = 0;

    return buf;
}

int32_t RefCountedBufferAcquire(RefCountedBuffer *buf)
{
    if (buf == NULL) {
        INIT_LOG_ERROR("RefCountedBufferAcquire: NULL buffer");
        return -1;
    }

    if (buf->refCount <= 0) {
        INIT_LOG_ERROR("RefCountedBufferAcquire: invalid ref count %d", buf->refCount);
        return -1;
    }

    buf->refCount++;
    return buf->refCount;
}

int32_t RefCountedBufferRelease(RefCountedBuffer *buf)
{
    if (buf == NULL) {
        INIT_LOG_ERROR("RefCountedBufferRelease: NULL buffer");
        return -1;
    }

    if (buf->refCount <= 0) {
        INIT_LOG_ERROR("RefCountedBufferRelease: buffer already released");
        return -1;
    }

    buf->refCount--;

    if (buf->refCount == 0) {
        if (buf->destroyFunc != NULL && buf->data != NULL) {
            buf->destroyFunc(buf->data);
        } else if (buf->data != NULL) {
            free(buf->data);
        }

        buf->data = NULL;
        buf->size = 0;
        buf->flags = 0;
        free(buf);
        return 0; /* Buffer was destroyed */
    }

    return buf->refCount;
}

int32_t RefCountedBufferGetCount(const RefCountedBuffer *buf)
{
    if (buf == NULL) {
        return -1;
    }
    return buf->refCount;
}

RefCountedBuffer *RefCountedBufferCopy(RefCountedBuffer *buf)
{
    if (buf == NULL) {
        INIT_LOG_ERROR("RefCountedBufferCopy: NULL buffer");
        return NULL;
    }

    int32_t newCount = RefCountedBufferAcquire(buf);
    if (newCount < 0) {
        return NULL;
    }

    return buf;
}
