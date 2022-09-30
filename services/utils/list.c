/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "list.h"

#include <stddef.h>
#include <stdlib.h>

/**
 * @brief Initialize a double-linked list head
 *
 * All other list API should be initialized by this function.\n
 *
 * @param head list head, make sure head is valid pointer
 * @return None
 */
void OH_ListInit(struct ListNode *node)
{
    if (node == NULL) {
        return;
    }
    node->next = node;
    node->prev = node;
}

/**
 * @brief Add a node to the end of the list
 *
 * @param head list head, make sure head is valid pointer
 * @param item new node to be added
 * @return None
 */
void OH_ListAddTail(struct ListNode *head, struct ListNode *item)
{
    if (head == NULL || item == NULL) {
        return;
    }
    item->next = head;
    item->prev = head->prev;
    head->prev->next = item;
    head->prev = item;
}

/**
 * @brief Remove a node from the list
 *
 * @param item the node to be removed from the list.
 *             This function does not free any memory within item.
 * @return None
 */
void OH_ListRemove(struct ListNode *item)
{
    if (item == NULL) {
        return;
    }
    item->next->prev = item->prev;
    item->prev->next = item->next;
}

/**
 * @brief Add a node to the list with order
 *
 * @param head list head, make sure head is valid pointer
 * @param item new node to be added
 * @param compareProc comparison function for adding node
 *      if it is ascending order, this function should return an integer less than,
 *      equal to, or greater than zero if the first argument is considered to be
 *      respectively less than, equal to, or greater than the second.
 * @return None
 */
void OH_ListAddWithOrder(struct ListNode *head, struct ListNode *item, ListCompareProc compareProc)
{
    ListNode *match;

    if (head == NULL || item == NULL || compareProc == NULL) {
        return;
    }

    match = head->next;
    while ((match != NULL) && (match != head)) {
        if (compareProc(match, item) > 0) {
            break;
        }
        match = match->next;
    }
    if (match == NULL) {
        return;
    }

    // Insert
    item->next = match;
    item->prev = match->prev;
    match->prev->next = item;
    match->prev = item;
}

/**
 * @brief Find a node by traversing the list
 *
 * @param head list head, make sure head is valid pointer.
 * @param data comparing data.
 * @param compareProc comparing function, return 0 if matched.
 * @return the found node; return NULL if none is found.
 */
ListNode *OH_ListFind(const ListNode *head, void *data, ListTraversalProc compareProc)
{
    ListNode *match;
    if ((head == NULL) || (compareProc == NULL)) {
        return NULL;
    }

    match = head->next;
    while ((match != NULL) && (match != head)) {
        if (compareProc(match, data) == 0) {
            return match;
        }
        match = match->next;
    }

    return NULL;
}

#define IS_REVERSE_ORDER(flags)    (((flags) & TRAVERSE_REVERSE_ORDER) != 0)
#define IS_STOP_WHEN_ERROR(flags)  (((flags) & TRAVERSE_STOP_WHEN_ERROR) != 0)

/**
 * @brief Traversal the list with specified function
 *
 * @param head list head, make sure head is valid pointer.
 * @param cookie optional traversing data.
 * @param traversalProc comparing function, return 0 if matched.
 * @param flags optional traversing flags:
 *  TRAVERSE_REVERSE_ORDER: traversing from last node to first node;
 *                          default behaviour is from first node to last node
 *  TRAVERSE_STOP_WHEN_ERROR: stop traversing if traversalProc return non-zero
 *                          default behaviour will ignore traversalProc return values
 * @return return -1 for invalid input arguments.
 *         when TRAVERSE_STOP_WHEN_ERROR is specified, it will return errors from traversalProc
 */
int OH_ListTraversal(ListNode *head, void *data, ListTraversalProc traversalProc, unsigned int flags)
{
    ListNode *match;
    ListNode *next;

    if ((head == NULL) || (traversalProc == NULL)) {
        return -1;
    }

    if (IS_REVERSE_ORDER(flags)) {
        match = head->prev;
    } else {
        match = head->next;
    }
    while ((match != NULL) && (match != head)) {
        if (IS_REVERSE_ORDER(flags)) {
            next = match->prev;
        } else {
            next = match->next;
        }
        int ret = traversalProc(match, data);
        if ((ret != 0) && IS_STOP_WHEN_ERROR(flags)) {
            return ret;
        }
        match = next;
    }

    return 0;
}

static int listDestroyTraversal(ListNode *node, void *data)
{
    ListDestroyProc destroyProc = (ListDestroyProc)data;

    if (destroyProc == NULL) {
        free((void *)node);
    } else {
        destroyProc(node);
    }

    return 0;
}

/**
 * @brief Find a node by traversing the list
 *
 * @param head list head, make sure head is valid pointer.
 * @param destroyProc destroy function; if NULL, it will free each node by default.
 * @return None
 */
void OH_ListRemoveAll(ListNode *head, ListDestroyProc destroyProc)
{
    if (head == NULL) {
        return;
    }

    OH_ListTraversal(head, (void *)destroyProc, listDestroyTraversal, 0);
    OH_ListInit(head);
}

/**
 * @brief Get list count
 *
 * @param head list head, make sure head is valid pointer.
 * @return the count of nodes in the list; return 0 if error
 */
int OH_ListGetCnt(const ListNode *head)
{
    int cnt;
    ListNode *node;

    if (head == NULL) {
        return 0;
    }

    cnt = 0;
    ForEachListEntry(head, node) {
        cnt++;
    }
    return cnt;
}
