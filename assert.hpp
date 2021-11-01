#ifndef ASSERT

#include <cassert>

#ifdef LOCAL
#define ASSERT(...) assert(__VA_ARGS__)
#else
#define ASSERT(...)
#endif

#endif