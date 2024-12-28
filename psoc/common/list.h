#pragma once
#include "iter.h"

#include <stddef.h>

typedef struct ListNodeDef *ListNode;
struct ListNodeDef
{
    void *data;
    ListNode prev;
    ListNode next;
};

typedef struct ListDef *List;
struct ListDef
{
    ListNode head;
    ListNode tail;
    size_t size;
};

ListNode list_node_create(void *data);
void list_node_destroy(ListNode *node);

List list_create();

void list_push_back(List list, void *data);
void *list_front(List list);
void *list_pop_front(List list);

Iter list_begin(List list);
