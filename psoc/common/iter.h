#pragma once

typedef struct Iter Iter;

typedef void (*iter_next_ft)(Iter *);
typedef void *(*iter_get_ft)(Iter *);
typedef int (*iter_done_ft)(Iter *);

struct Iter
{
    iter_next_ft next;
    iter_get_ft get;
    iter_done_ft done;
    void *ref;
};

void iter_init(Iter *it, iter_next_ft next, iter_get_ft get, iter_done_ft done);
Iter iter_create(iter_next_ft next, iter_get_ft get, iter_done_ft done);

static inline void iter_next(Iter *it)
{
    it->next(it);
}

static inline void *iter_get(Iter *it)
{
    return it->get(it);
}

static inline int iter_done(Iter *it)
{
    return it->done(it);
}

/*

Ideal interface.
It would be dynamically allocated.
for (Iter *it = begin(c); it != end(c); it = iter_next(it))
{
}

Iter container is stack allocated, and contains a reference to the underlying iterator.
for (Iter it = begin(c); !iter_end(&it); inext(&it))
{
    SomeData *data = (SomeData*)iter_get(&it);
}

for (Iter it = begin(c); !iter_done(&it); iter_next(&it))
{
    SomeData *data = (SomeData*)iter_get(&it);
}

{
    Iter _begin = begin(c);
    Iter *_end = end(c);
    for (Iter *it = begin(c); it != end(c); it = iter_next(it))
    {
        SomeData *data = (SomeData*)iter_get(it);
    }
}

*/