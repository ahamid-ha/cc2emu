#ifndef __UTILS__
#define __UTILS__

#include <inttypes.h>
#include <stdbool.h>

#define LOG_BUFFER_SIZE 1024 * 4
#define SEC_TO_NS(sec) ((sec)*1000000000)


typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_ERROR
} LogLevel;


void init_utils();
void log_message(LogLevel level, const char *format, ...);
char *log_get_buffer();
int log_error_status_clear();
uint64_t nanos();
bool str_ends_with(const char *str, const char *substr);
bool is_file_writable(const char* path);
bool is_file_readable(const char* path);

#endif
