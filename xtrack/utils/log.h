/**
 * @file  utils/log.h
 * @brief Simple tag-based log macros (inspired by ESP-IDF)
 *
 * Usage: define TAG at the top of each .c file, then log:
 *   LOG_E(TAG, "something went wrong: %d", err);
 *   LOG_I(TAG, "started");
 *
 * Compile-time filtering via LOG_LEVEL:
 *   gcc -DLOG_LEVEL=LOG_LEVEL_WARN ...
 */
#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_LEVEL_NONE  0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_DEBUG 4

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#define LOG_E(tag, fmt, ...)                                 \
    do {                                                     \
        if (LOG_LEVEL >= LOG_LEVEL_ERROR)                    \
            printf("[E][" tag "] " fmt "\n", ##__VA_ARGS__); \
    } while (0)

#define LOG_W(tag, fmt, ...)                                 \
    do {                                                     \
        if (LOG_LEVEL >= LOG_LEVEL_WARN)                     \
            printf("[W][" tag "] " fmt "\n", ##__VA_ARGS__); \
    } while (0)

#define LOG_I(tag, fmt, ...)                                 \
    do {                                                     \
        if (LOG_LEVEL >= LOG_LEVEL_INFO)                     \
            printf("[I][" tag "] " fmt "\n", ##__VA_ARGS__); \
    } while (0)

#define LOG_D(tag, fmt, ...)                                 \
    do {                                                     \
        if (LOG_LEVEL >= LOG_LEVEL_DEBUG)                    \
            printf("[D][" tag "] " fmt "\n", ##__VA_ARGS__); \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* LOG_H */
