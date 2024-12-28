#include "list.h"
#include "dalloc.h"

ListNode list_node_create(void *data)
{
    ListNode node = dalloc(sizeof(*node));

    node->data = data;
    node->prev = NULL;
    node->next = NULL;

    return node;
}

void list_node_destroy(ListNode *node)
{
    dfree(*node);
    *node = NULL;
}

List list_create()
{
    List list = dalloc(sizeof(*list));
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;

    return list;
}

void list_insert(ListNode prev, ListNode node)
{
}

void list_push_back(List list, void *data)
{
    ListNode node = list_node_create(data);
    node->prev = list->tail;
    list->tail = node;

    // First element
    if (node->prev == NULL)
    {
        list->head = node;
    }
    else
    {
        node->prev->next = node;
    }
}

void *list_front(List list)
{
    return list->head ? list->head->data : NULL;
}

void *list_pop_front(List list)
{
    ListNode head = list->head;

    if (head)
    {
        if (list->head->next)
        {
                }
        list->head = head->next;
    }

    return front;
}

static void _list_iter_next(Iter *_it)
{
    ListNode node = (ListNode)_it->ref;
    node = node->next;
    _it->ref = node;
}

static void *_list_iter_get(Iter *_it)
{
    ListNode node = (ListNode)_it->ref;
    return node->data;
}

static int _list_iter_done(Iter *_it)
{
    ListNode node = (ListNode)_it->ref;
    return node == NULL;
}

Iter list_begin(List list)
{
    Iter it;
    iter_init(&it, _list_iter_next, _list_iter_get, _list_iter_done);
    it.ref = list->head;

    return it;
}
