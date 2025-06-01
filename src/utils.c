#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <SDL3/SDL_stdinc.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include "utils.h"

#ifdef _WIN32
static LARGE_INTEGER freq;
#endif


static char log_buffer[LOG_BUFFER_SIZE];
static size_t buffer_len = 0;
int log_buffer_error_status = 0;

int log_error_status_clear(){
    if (log_buffer_error_status) {
        log_buffer_error_status = 0;
        return 1;
    }
    return 0;
}

void log_message(LogLevel level, const char *format, ...) {
    va_list args;
    va_start(args, format);

    // Get current timestamp
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", tm_info);

    // Determine the output stream based on log level
    FILE *out_stream;
    char *level_str = "";
    switch (level) {
        case LOG_DEBUG:
            level_str = "DEBUG";
            out_stream = stdout;
            break;
        case LOG_INFO:
            level_str = "INFO";
            out_stream = stdout;
            break;
        case LOG_ERROR:
            level_str = "ERROR";
            out_stream = stderr;
            log_buffer_error_status = 1;
            break;
    }

    // Print message to the appropriate stream
    fprintf(out_stream, "[%s]:%s: ", timestamp, level_str);
    vfprintf(out_stream, format, args);
    fprintf(out_stream, "\n");

    // Calculate the required space for the new log entry
    va_end(args);
    va_start(args, format);
    int required_space = snprintf(NULL, 0, "[%s]:%s: ", timestamp, level_str) + vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (required_space > LOG_BUFFER_SIZE - 2) return;

    if (buffer_len + required_space + 2 >= LOG_BUFFER_SIZE) {
        // Remove complete lines from the beginning until there is enough space
        char *new_buffer = log_buffer;
        while (buffer_len + required_space + 2 >= LOG_BUFFER_SIZE) {
            char *next_line = strchr(new_buffer, '\n');
            if (next_line == NULL)
                break; // No more lines to remove

            buffer_len -= (next_line - new_buffer) + 1;
            new_buffer = next_line + 1;
        }

        // Shift the remaining part of the buffer
        memmove(log_buffer, new_buffer, buffer_len);
    }

    // Format and append the new message to the buffer;
    va_start(args, format);
    int bytes_written = snprintf(log_buffer + buffer_len, LOG_BUFFER_SIZE - buffer_len, "[%s]:%s: ", timestamp, level_str);
    vsnprintf(log_buffer + buffer_len + bytes_written, LOG_BUFFER_SIZE - buffer_len - bytes_written, format, args);
    log_buffer[buffer_len + required_space] = '\n';
    log_buffer[buffer_len + required_space + 1] = '\0';
    buffer_len += required_space + 1;

    va_end(args);
}


char *log_get_buffer() {
    return log_buffer;
}


void init_utils() {
#ifdef _WIN32
    if (!QueryPerformanceFrequency(&freq)) {
        log_message(LOG_ERROR, "QueryPerformanceFrequency failed");
        exit(1);
    }
#endif
}

#define SEC_TO_NS(sec) ((sec)*1000000000)
uint64_t nanos()
{
#ifdef _WIN32
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    uint64_t ns = (uint64_t)((double)counter.QuadPart / freq.QuadPart * SEC_TO_NS(1));
    return ns;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    uint64_t ns = SEC_TO_NS((uint64_t)ts.tv_sec) + (uint64_t)ts.tv_nsec;
    return ns;
#endif
}

bool str_ends_with(const char *str, const char *substr) {
    if (!str || !substr) return false;
    if (strlen(str) < strlen(substr)) return false;
    return SDL_strncasecmp(str + strlen(str) - strlen(substr), substr, strlen(substr)) == 0;
}

bool is_file_writable(const char* path) {
#ifdef _WIN32
    return _access(path, 0x2) == 0;
#else
    return access(path, W_OK) == 0;
#endif
}

bool is_file_readable(const char* path) {
#ifdef _WIN32
    return _access(path, 0x4) == 0;
#else
    return access(path, R_OK) == 0;
#endif
}
