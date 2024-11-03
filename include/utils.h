#ifndef __UTILS__
#define __UTILS__

#include <inttypes.h>
#include <stdbool.h>

#define SEC_TO_NS(sec) ((sec)*1000000000)
uint64_t nanos();
bool str_ends_with(const char *str, const char *substr);

#endif
