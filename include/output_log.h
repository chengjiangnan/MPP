#ifndef __OUTPUT_LOG_H__
#define __OUTPUT_LOG_H__ 1

#define LOG_OFF      100
#define LOG_ERROR     50
#define LOG_WARNING   40
#define LOG_INFO      30
#define LOG_DEBUG     20
#define LOG_TRACE     10

#define LOG_LEVEL LOG_TRACE

void log_error(const char *format, ...);
void log_warn(const char *format, ...);
void log_info(const char *format, ...);
void log_debug(const char *format, ...);
void log_trace(const char *format, ...);

#endif /*__OUTPUT_LOG_H__*/
