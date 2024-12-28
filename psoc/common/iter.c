#include "iter.h"
#include "dalloc.h"

void iter_init(Iter *it, iter_next_ft next, iter_get_ft get, iter_done_ft done)
{
    it->next = next;
    it->get = get;
    it->done = done;
}
Iter iter_create(iter_next_ft next, iter_get_ft get, iter_done_ft done)
{
    Iter it;
    iter_init(&it, next, get, done);
    return it;
}
