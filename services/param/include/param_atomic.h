/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef BASE_STARTUP_PARAM_ATOMIC_H
#define BASE_STARTUP_PARAM_ATOMIC_H
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>

#if (defined(PARAM_SUPPORT_STDATOMIC) || defined(__LITEOS_A__))
#include <pthread.h>
#include <stdatomic.h>
#endif
#if defined FUTEX_WAIT || defined FUTEX_WAKE
#include <linux/futex.h>
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#ifdef __LITEOS_M__
#define ATOMIC_UINT32 uint32_t
#define ATOMIC_LLONG  long long
#define ATOMIC_INIT(commitId, value) *(commitId) = (value)
#define ATOMIC_LOAD_EXPLICIT(commitId, order) *(commitId)
#define ATOMIC_STORE_EXPLICIT(commitId, value, order) *(commitId) = (value)
#define ATOMIC_UINT64_INIT(commitId, value) *(commitId) = (value)
#define ATOMIC_UINT64_LOAD_EXPLICIT(commitId, order) *(commitId)
#define ATOMIC_UINT64_STORE_EXPLICIT(commitId, value, order) *(commitId) = (value)
#define ATOMIC_SYNC_OR_AND_FETCH(commitId, value, order) *(commitId) |= (value)
#define ATOMIC_SYNC_ADD_AND_FETCH(commitId, value, order) *(commitId) += (value)

#define futex_wake(ftx, count) (void)(ftx)
#define futex_wait(ftx, value) (void)(ftx)
#define futex_wake_private(ftx, count) (void)(ftx)
#define futex_wait_private(ftx, value) (void)(ftx)
#else

// support futex
#ifndef __NR_futex
#define PARAM_NR_FUTEX 202 /* syscall number */
#else
#define PARAM_NR_FUTEX __NR_futex
#endif

#if !(defined FUTEX_WAIT || defined FUTEX_WAKE)
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_PRIVATE_FLAG 128
#define FUTEX_WAIT_PRIVATE (FUTEX_WAIT | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAKE_PRIVATE (FUTEX_WAKE | FUTEX_PRIVATE_FLAG)

#define PARAM_FUTEX(ftx, op, value, timeout, bitset)                       \
    do {                                                                   \
        struct timespec d_timeout = { 0, 1000 * 1000 * (timeout) };        \
        syscall(PARAM_NR_FUTEX, ftx, op, value, &d_timeout, NULL, bitset); \
    } while (0)

#define futex_wake(ftx, count) PARAM_FUTEX(ftx, FUTEX_WAKE, count, 0, 0)
#define futex_wait(ftx, value) PARAM_FUTEX(ftx, FUTEX_WAIT, value, 100, 0)
#define futex_wake_private(ftx, count) PARAM_FUTEX(ftx, FUTEX_WAKE_PRIVATE, count, 0, 0)
#define futex_wait_private(ftx, value) PARAM_FUTEX(ftx, FUTEX_WAIT_PRIVATE, value, 100, 0)
#endif

#if (defined(PARAM_SUPPORT_STDATOMIC) || defined(__LITEOS_A__))
#define MEMORY_ORDER_RELAXED memory_order_relaxed
#define MEMORY_ORDER_CONSUME memory_order_consume
#define MEMORY_ORDER_ACQUIRE memory_order_acquire
#define MEMORY_ORDER_RELEASE memory_order_release

#define ATOMIC_UINT32 atomic_uint
#define ATOMIC_LLONG atomic_llong
#define ATOMIC_INIT(commitId, value) atomic_init((commitId), (value))
#define ATOMIC_UINT64_INIT(commitId, value) atomic_init((commitId), (value))
#define ATOMIC_LOAD_EXPLICIT(commitId, order) atomic_load_explicit((commitId), (order))
#define ATOMIC_UINT64_LOAD_EXPLICIT(commitId, order) atomic_load_explicit((commitId), order)
#define ATOMIC_STORE_EXPLICIT(commitId, value, order) atomic_store_explicit((commitId), (value), (order))
#define ATOMIC_UINT64_STORE_EXPLICIT(commitId, value, order) atomic_store_explicit((commitId), (value), (order))
#define ATOMIC_SYNC_OR_AND_FETCH(commitId, value, order) atomic_fetch_or_explicit((commitId), (value), (order))
#define ATOMIC_SYNC_ADD_AND_FETCH(commitId, value, order) atomic_fetch_add_explicit((commitId), (value), (order))

#else

#define MEMORY_ORDER_RELAXED 0
#define MEMORY_ORDER_CONSUME 1
#define MEMORY_ORDER_ACQUIRE 2
#define MEMORY_ORDER_RELEASE 3

#define ATOMIC_UINT32 uint32_t
#define ATOMIC_LLONG int64_t

static inline void param_atomic_store(ATOMIC_UINT32 *ptr, uint32_t value, int order)
{
    __sync_lock_test_and_set(ptr, value);
    if (order == MEMORY_ORDER_RELEASE) {
        __sync_synchronize();
    }
}

static inline void param_atomic_uint64_store(ATOMIC_LLONG *ptr, int64_t value, int order)
{
    __sync_lock_test_and_set(ptr, value);
    if (order == MEMORY_ORDER_RELEASE) {
        __sync_synchronize();
    }
}

static inline void param_atomic_init(ATOMIC_UINT32 *ptr, uint32_t value)
{
    *ptr = 0;
    __sync_fetch_and_add(ptr, value, 0);
}

static inline void param_atomic_uint64_init(ATOMIC_LLONG *ptr, int64_t value)
{
    *ptr = 0;
    __sync_fetch_and_add(ptr, value, 0);
}

static inline ATOMIC_UINT32 param_atomic_load(ATOMIC_UINT32 *ptr, int order)
{
    return *((volatile ATOMIC_UINT32 *)ptr);
}

static inline ATOMIC_LLONG param_atomic_uint64_load(ATOMIC_LLONG *ptr, int order)
{
    return *((volatile ATOMIC_LLONG *)ptr);
}

#define ATOMIC_INIT(commitId, value) param_atomic_init((commitId), (value))
#define ATOMIC_UINT64_INIT(commitId, value) param_atomic_uint64_init((commitId), (value))
#define ATOMIC_LOAD_EXPLICIT(commitId, order) param_atomic_load((commitId), order)
#define ATOMIC_UINT64_LOAD_EXPLICIT(commitId, order) param_atomic_uint64_load((commitId), order)
#define ATOMIC_STORE_EXPLICIT(commitId, value, order) param_atomic_store((commitId), (value), (order))
#define ATOMIC_UINT64_STORE_EXPLICIT(commitId, value, order) param_atomic_uint64_store((commitId), (value), (order))
#define ATOMIC_SYNC_OR_AND_FETCH(commitId, value, order) __sync_or_and_fetch((commitId), (value))
#define ATOMIC_SYNC_ADD_AND_FETCH(commitId, value, order) __sync_add_and_fetch((commitId), (value))
#endif
#endif // __LITEOS_M__
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // BASE_STARTUP_PARAM_ATOMIC_H