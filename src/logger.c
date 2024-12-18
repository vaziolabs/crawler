#include "logger.h"

LogLevel current_log_level = DEBUG;  // Define the global variable

void logr(LogLevel level, const char* format, ...) {
    if (level < current_log_level) return;
    
    time_t now;
    time(&now);
    char timestamp[26];
    ctime_r(&now, timestamp);
    timestamp[24] = '\0';  // Remove newline
    
    const char* level_str;
    switch (level) {
        case VERBOSE: level_str = "VERBOSE"; break;
        case DEBUG:   level_str = "DEBUG"; break;
        case INFO:    level_str = "INFO"; break;
        case WARN:    level_str = "WARN"; break;
        case ERROR:   level_str = "ERROR"; break;
        default:      level_str = "UNKNOWN";
    }
    
    fprintf(stderr, "[%s] %s: ", timestamp, level_str);
    
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void set_log_level(LogLevel level) {
    current_log_level = level;
}