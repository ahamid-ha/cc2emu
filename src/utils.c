#include <time.h>
#include <string.h>
#include "utils.h"

#define SEC_TO_NS(sec) ((sec)*1000000000)
uint64_t nanos()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    uint64_t ns = SEC_TO_NS((uint64_t)ts.tv_sec) + (uint64_t)ts.tv_nsec;
    return ns;
}

bool str_ends_with(const char *str, const char *substr) {
    if (!str || !substr) return false;
    if (strlen(str) < strlen(substr)) return false;
    return strncasecmp(str + strlen(str) - strlen(substr), substr, strlen(substr)) == 0;
}
