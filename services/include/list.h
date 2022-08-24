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

#ifndef BASE_STARTUP_INITLITE_LIST_H
#define BASE_STARTUP_INITLITE_LIST_H
#include <stddef.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief Double linked list structure is show as below:
 * 
 *     |￣￣￣￣￣|-----------------------------------------------|
 *  |->|  head  |                                               |
 *  |  |________|<-------------------------------------------|  |
 *  |   |                                                    |  |
 *  |   └-next->|▔▔▔▔▔▔|-next->|▔▔▔▔▔▔|-next->|▔▔▔▔▔▔|-next--|  |
 *  └------prev-| node |<-prev-| node |<-prev-| node |<-prev----|
 *              |------|       |------|       |------|
 *              | extra|       | extra|       | extra|
 *              |______|       |______|       |______|
 *
 * Usage Example:
 * 1. Define structure for each item in the list
 *   typedef struct tagTEST_LIST_ITEM {
 *       ListNode node;
 *       int value;
 *   } TEST_LIST_ITEM;
 * 
 * 2. Define a list and init list by OH_ListAddTail
 *   ListNode testList;
 *   c(&testList);
 * 
 * 3. Define a list item instance
 *   TEST_LIST_ITEM item;
 *   item.value = 0;
 * 
 * 4. Add list item to list
 *   OH_ListAddTail(&testList, (ListNode *)(&item));
 * 
 * 5. Advanced usage: add with order
 *   // Ordering compare function
 *   static int TestListItemCompareProc(ListNode *node, ListNode *newNode)
 *   {
 *       TEST_LIST_ITEM *left = (TEST_LIST_ITEM *)node;
 *       TEST_LIST_ITEM *right = (TEST_LIST_ITEM *)newNode;
 *       return left->value - right->value;
 *   }
 *   OH_ListAddWithOrder(&testList, (ListNode *)(&item))
 */

/**
 * @brief Double linked list node
 */
typedef struct ListNode {
    struct ListNode *next;
    struct ListNode *prev;
} ListNode, ListHead;

#define ListEmpty(node)   ((node).next == &(node) && (node).prev == &(node))
#define ListEntry(ptr, type, member)   ((type *)((char *)(ptr) - offsetof(type, member)))
#define ForEachListEntry(list, node)   for (node = (list)->next; node != (list); node = node->next)

/**
 * @brief Initialize a double-linked list head
 *
 * All other list API should be initialized by this function.\n
 *
 * @param head list head, make sure head is valid pointer
 * @return None
 */
void OH_ListInit(struct ListNode *list);

/**
 * @brief Add a node to the end of the list
 *
 * @param head list head, make sure head is valid pointer
 * @param item new node to be added
 * @return None
 */
void OH_ListAddTail(struct ListNode *list, struct ListNode *item);

/**
 * @brief Remove a node from the list
 *
 * @param item the node to be removed from the list.
 *             This function does not free any memory within item.
 * @return None
 */
void OH_ListRemove(struct ListNode *item);

/**
 * @brief ListNode comparison function prototype
 *
 * @param node ListNode to be compared.
 * @param newNode new ListNode to be compared.
 * @return
 *     <0 if node < newNode
 *     =0 if node = newNode
 *     >0 if Node > newNode
 */
typedef int (*ListCompareProc)(ListNode *node, ListNode *newNode);

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
void OH_ListAddWithOrder(struct ListNode *head, struct ListNode *item, ListCompareProc compareProc);

/**
 * @brief ListNode traversing and find function prototype
 *
 * @param node ListNode to be compared.
 * @param data value for traversing
 * @return
 *     return 0 if node value equals data for OH_ListFind
 */
typedef int (*ListTraversalProc)(ListNode *node, void *data);

/**
 * @brief Find a node by traversing the list
 *
 * @param head list head, make sure head is valid pointer.
 * @param data comparing data.
 * @param compareProc comparing function, return 0 if matched.
 * @return the found node; return NULL if none is found.
 */
ListNode *OH_ListFind(const ListNode *head, void *data, ListTraversalProc compareProc);

/* Traversing from end to start */
#define TRAVERSE_REVERSE_ORDER   0x1
/* Stop traversing when error returned */
#define TRAVERSE_STOP_WHEN_ERROR 0x2

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
int OH_ListTraversal(ListNode *head, void *data, ListTraversalProc traversalProc, unsigned int flags);

/**
 * @brief ListNode destroy function prototype
 *
 * @param node ListNode to be destroyed.
 * @return None
 */
typedef void (*ListDestroyProc)(ListNode *node);

/**
 * @brief Find a node by traversing the list
 *
 * @param head list head, make sure head is valid pointer.
 * @param destroyProc destroy function; if NULL, it will free each node by default.
 * @return None
 */
void OH_ListRemoveAll(ListNode *head, ListDestroyProc destroyProc);

/**
 * @brief Get list count
 *
 * @param head list head, make sure head is valid pointer.
 * @return the count of nodes in the list; return 0 if error
 */
int OH_ListGetCnt(const ListNode *head);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // BASE_STARTUP_INITLITE_LIST_H
