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

#ifndef STARTUP_INIT_DATA_STRUCTURES_H
#define STARTUP_INIT_DATA_STRUCTURES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "list.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/* -------------------------------------------------------------------------- */
/* Priority Queue                                                              */
/* -------------------------------------------------------------------------- */

/** Maximum capacity of the priority queue */
#define PRIORITY_QUEUE_MAX_CAPACITY 1024

/** Priority levels */
typedef enum {
    PRIORITY_LOWEST   = 0,
    PRIORITY_LOW      = 1,
    PRIORITY_NORMAL   = 2,
    PRIORITY_HIGH     = 3,
    PRIORITY_HIGHEST  = 4,
    PRIORITY_CRITICAL = 5,
    PRIORITY_COUNT    = 6
} PriorityLevel;

/** Element in a priority queue */
typedef struct PriorityQueueNode {
    ListNode    linkNode;
    void       *userData;
    int64_t     priority;
    uint64_t    sequenceId;
    uint32_t    tag;
    bool        isActive;
} PriorityQueueNode;

/** Priority queue handle */
typedef struct PriorityQueue {
    ListNode    headNodes[PRIORITY_COUNT];
    size_t      totalCount;
    size_t      maxCapacity;
    uint64_t    nextSequenceId;
    int         (*dataCompare)(const void *a, const void *b);
    void        (*dataFree)(void *data);
} PriorityQueue;

/**
 * @brief Initialize a priority queue.
 * @param queue Pointer to PriorityQueue structure.
 * @param maxCapacity Maximum number of elements.
 * @param compareFunc Comparison function for elements at same priority.
 * @param freeFunc Function to free element data, or NULL.
 * @return 0 on success, -1 on failure.
 */
int InitPriorityQueue(PriorityQueue *queue, size_t maxCapacity,
    int (*compareFunc)(const void *, const void *), void (*freeFunc)(void *));

/**
 * @brief Destroy a priority queue and free all elements.
 * @param queue Pointer to PriorityQueue structure.
 */
void DestroyPriorityQueue(PriorityQueue *queue);

/**
 * @brief Insert an element into the priority queue.
 * @param queue Pointer to PriorityQueue structure.
 * @param data User data pointer.
 * @param priority Priority level for the element.
 * @param tag Optional tag for grouping.
 * @return 0 on success, -1 on failure.
 */
int PriorityQueuePush(PriorityQueue *queue, void *data, PriorityLevel priority, uint32_t tag);

/**
 * @brief Remove and return the highest priority element.
 * @param queue Pointer to PriorityQueue structure.
 * @return User data pointer of the popped element, or NULL if empty.
 */
void *PriorityQueuePop(PriorityQueue *queue);

/**
 * @brief Peek at the highest priority element without removing it.
 * @param queue Pointer to PriorityQueue structure.
 * @return User data pointer, or NULL if empty.
 */
void *PriorityQueuePeek(const PriorityQueue *queue);

/**
 * @brief Get the number of elements in the queue.
 * @param queue Pointer to PriorityQueue structure.
 * @return Number of elements.
 */
size_t PriorityQueueSize(const PriorityQueue *queue);

/**
 * @brief Check if the priority queue is empty.
 * @param queue Pointer to PriorityQueue structure.
 * @return true if empty, false otherwise.
 */
bool PriorityQueueIsEmpty(const PriorityQueue *queue);

/**
 * @brief Remove elements with a specific tag.
 * @param queue Pointer to PriorityQueue structure.
 * @param tag Tag value to match.
 * @return Number of elements removed.
 */
size_t PriorityQueueRemoveByTag(PriorityQueue *queue, uint32_t tag);

/* -------------------------------------------------------------------------- */
/* Ring Buffer                                                                 */
/* -------------------------------------------------------------------------- */

/** Default ring buffer capacity */
#define RING_BUFFER_DEFAULT_CAPACITY 256

/** Ring buffer structure */
typedef struct RingBuffer {
    uint8_t *buffer;
    size_t   capacity;
    size_t   readIndex;
    size_t   writeIndex;
    size_t   dataSize;
    bool     isFull;
    bool     isOverwrite;
    uint32_t overflowCount;
    uint32_t underflowCount;
} RingBuffer;

/**
 * @brief Initialize a ring buffer.
 * @param rb Pointer to RingBuffer structure.
 * @param capacity Buffer capacity in bytes.
 * @param allowOverwrite Whether to allow overwriting old data when full.
 * @return 0 on success, -1 on failure.
 */
int InitRingBuffer(RingBuffer *rb, size_t capacity, bool allowOverwrite);

/**
 * @brief Destroy a ring buffer and free resources.
 * @param rb Pointer to RingBuffer structure.
 */
void DestroyRingBuffer(RingBuffer *rb);

/**
 * @brief Write data into the ring buffer.
 * @param rb Pointer to RingBuffer structure.
 * @param data Source data buffer.
 * @param length Number of bytes to write.
 * @return Number of bytes actually written.
 */
size_t RingBufferWrite(RingBuffer *rb, const uint8_t *data, size_t length);

/**
 * @brief Read data from the ring buffer.
 * @param rb Pointer to RingBuffer structure.
 * @param dest Destination buffer.
 * @param maxLength Maximum number of bytes to read.
 * @return Number of bytes actually read.
 */
size_t RingBufferRead(RingBuffer *rb, uint8_t *dest, size_t maxLength);

/**
 * @brief Peek at data in the ring buffer without consuming it.
 * @param rb Pointer to RingBuffer structure.
 * @param dest Destination buffer.
 * @param maxLength Maximum number of bytes to peek.
 * @param offset Byte offset from read position.
 * @return Number of bytes actually peeked.
 */
size_t RingBufferPeek(const RingBuffer *rb, uint8_t *dest, size_t maxLength, size_t offset);

/**
 * @brief Get the amount of data currently in the ring buffer.
 * @param rb Pointer to RingBuffer structure.
 * @return Number of bytes available for reading.
 */
size_t RingBufferDataSize(const RingBuffer *rb);

/**
 * @brief Get the remaining free space in the ring buffer.
 * @param rb Pointer to RingBuffer structure.
 * @return Number of bytes available for writing.
 */
size_t RingBufferFreeSpace(const RingBuffer *rb);

/**
 * @brief Clear all data from the ring buffer.
 * @param rb Pointer to RingBuffer structure.
 */
void RingBufferClear(RingBuffer *rb);

/**
 * @brief Reset overflow/underflow counters.
 * @param rb Pointer to RingBuffer structure.
 */
void RingBufferResetCounters(RingBuffer *rb);

/* -------------------------------------------------------------------------- */
/* Reference-counted Pointer                                                   */
/* -------------------------------------------------------------------------- */

/** Reference-counted buffer */
typedef struct RefCountedBuffer {
    void    *data;
    size_t   size;
    int32_t  refCount;
    void   (*destroyFunc)(void *data);
    uint32_t flags;
    uint64_t allocId;
} RefCountedBuffer;

/**
 * @brief Create a reference-counted buffer.
 * @param data Raw data pointer (ownership transferred).
 * @param size Size of data in bytes.
 * @param destroyFunc Optional destructor for the data.
 * @return New RefCountedBuffer with refCount = 1, or NULL on failure.
 */
RefCountedBuffer *CreateRefCountedBuffer(void *data, size_t size, void (*destroyFunc)(void *));

/**
 * @brief Increment reference count (acquire).
 * @param buf Pointer to RefCountedBuffer.
 * @return New reference count, or -1 on error.
 */
int32_t RefCountedBufferAcquire(RefCountedBuffer *buf);

/**
 * @brief Decrement reference count (release). Frees if count reaches 0.
 * @param buf Pointer to RefCountedBuffer.
 * @return New reference count, or -1 if buffer was destroyed.
 */
int32_t RefCountedBufferRelease(RefCountedBuffer *buf);

/**
 * @brief Get the current reference count.
 * @param buf Pointer to RefCountedBuffer.
 * @return Current reference count, or -1 on error.
 */
int32_t RefCountedBufferGetCount(const RefCountedBuffer *buf);

/**
 * @brief Create a copy of the reference-counted buffer (increments ref count).
 * @param buf Source RefCountedBuffer to copy.
 * @return The same buffer with incremented ref count, or NULL.
 */
RefCountedBuffer *RefCountedBufferCopy(RefCountedBuffer *buf);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* STARTUP_INIT_DATA_STRUCTURES_H */
