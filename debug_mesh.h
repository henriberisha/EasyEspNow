#ifndef DEBUG_MESH_H
#define DEBUG_MESH_H

/*
 * When defining log levels, the order usually reflects the verbosity of the logs, from least to most detailed.
 * Here’s a commonly used order for log levels:
 *
 *   ERROR: Critical issues that require immediate attention. These are the most severe problems that usually
 *   indicate a failure or an issue that prevents the system from functioning correctly.
 *
 *   WARNING: Issues that are not critical but could potentially lead to problems or require attention in the future.
 *   These are warnings about potential issues that should be noted but don't necessarily stop the system from functioning.
 *
 *   INFO: General information about the system's operations and state.
 *   These messages are useful for understanding the normal operation and state changes within the system.
 *
 *   DEBUG: Detailed information useful for debugging and understanding the internal workings of the system.
 *   These messages help diagnose issues during development and are typically too verbose for production environments.
 *
 *   VERBOSE: Extremely detailed logging information, often including low-level or verbose details about the system's operations.
 *   This is the most detailed level and is usually only used for in-depth troubleshooting or development.
 *
 * Suggested Order:
 *
 *   ERROR
 *   WARNING
 *   INFO
 *   DEBUG
 *   VERBOSE
 *
 **/

// Define the log levels
#define LOG_NONE 0
#define LOG_MONITOR 1
#define LOG_ERROR 2
#define LOG_WARNING 3
#define LOG_INFO 4
#define LOG_DEBUG 5
#define LOG_VERBOSE 6

// Define the log level you want (adjust as needed)
// #define CURRENT_LOG_LEVEL LOG_NONE
extern int CURRENT_LOG_LEVEL;

#define MONITOR(tag, format, ...)                                                              \
    do                                                                                         \
    {                                                                                          \
        if (CURRENT_LOG_LEVEL >= LOG_MONITOR)                                                  \
        {                                                                                      \
            Serial.printf("[%lu] [MONITOR] [%s]: " format "\n", millis(), tag, ##__VA_ARGS__); \
        }                                                                                      \
    } while (0)

#define ERROR(tag, format, ...)                                                              \
    do                                                                                       \
    {                                                                                        \
        if (CURRENT_LOG_LEVEL >= LOG_ERROR)                                                  \
        {                                                                                    \
            Serial.printf("[%lu] [ERROR] [%s]: " format "\n", millis(), tag, ##__VA_ARGS__); \
        }                                                                                    \
    } while (0)

#define WARNING(tag, format, ...)                                                              \
    do                                                                                         \
    {                                                                                          \
        if (CURRENT_LOG_LEVEL >= LOG_WARNING)                                                  \
        {                                                                                      \
            Serial.printf("[%lu] [WARNING] [%s]: " format "\n", millis(), tag, ##__VA_ARGS__); \
        }                                                                                      \
    } while (0)

#define INFO(tag, format, ...)                                                              \
    do                                                                                      \
    {                                                                                       \
        if (CURRENT_LOG_LEVEL >= LOG_INFO)                                                  \
        {                                                                                   \
            Serial.printf("[%lu] [INFO] [%s]: " format "\n", millis(), tag, ##__VA_ARGS__); \
        }                                                                                   \
    } while (0)

#define DEBUG(tag, format, ...)                                                              \
    do                                                                                       \
    {                                                                                        \
        if (CURRENT_LOG_LEVEL >= LOG_DEBUG)                                                  \
        {                                                                                    \
            Serial.printf("[%lu] [DEBUG] [%s]: " format "\n", millis(), tag, ##__VA_ARGS__); \
        }                                                                                    \
    } while (0)

#define VERBOSE(tag, format, ...)                                                              \
    do                                                                                         \
    {                                                                                          \
        if (CURRENT_LOG_LEVEL >= LOG_VERBOSE)                                                  \
        {                                                                                      \
            Serial.printf("[%lu] [VERBOSE] [%s]: " format "\n", millis(), tag, ##__VA_ARGS__); \
        }                                                                                      \
    } while (0)

#endif