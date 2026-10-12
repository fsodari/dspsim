#pragma once

/*
    Control over what the compiler inlines on the run cycle.

    The core is compiled as one translation unit, so the compiler is free to inline any of it into any
    other part, and it decides by size. That makes the result depend on unrelated edits: a function that
    grows by a few instructions is no longer inlined into the scheduler, or the rare, large path of a
    small function is inlined along with it, which makes every caller large and makes it keep registers
    on the stack. Each of these changed the instruction count of some benchmarks by 5 to 12%. The two
    macros pin the decisions that matter.

    DSPSIM_NOINLINE keeps a function out of its callers. It is for the rare path of a function that the
    run cycle calls all the time: the part of a push_back() that makes the storage larger, for example.

    DSPSIM_FLATTEN inlines everything a function calls, as far as the definitions are visible and not
    marked DSPSIM_NOINLINE. It is for the scheduler's loop (Context::eval()). Compilers without it use
    their own judgment, and so does a build that compiles the core file by file.
*/
#if defined(_MSC_VER)
#define DSPSIM_NOINLINE __declspec(noinline)
#define DSPSIM_FLATTEN
#elif defined(__GNUC__)
#define DSPSIM_NOINLINE [[gnu::noinline]]
#define DSPSIM_FLATTEN [[gnu::flatten]]
#else
#define DSPSIM_NOINLINE
#define DSPSIM_FLATTEN
#endif
