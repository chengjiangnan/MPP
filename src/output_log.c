#include "output_log.h"

#include <stdio.h>
#include <stdarg.h>

void log_error(const char *format, ...)
{
#if LOG_LEVEL <= LOG_ERROR
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
#endif
}

void log_warn(const char *format, ...)
{
#if LOG_LEVEL <= LOG_WARNING
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
#endif
}

void log_info(const char *format, ...)
{
#if LOG_LEVEL <= LOG_INFO
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
#endif
}

void log_debug(const char *format, ...)
{
#if LOG_LEVEL <= LOG_DEBUG
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
#endif
}

void log_trace(const char *format, ...)
{
#if LOG_LEVEL <= LOG_TRACE
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
#endif
}
