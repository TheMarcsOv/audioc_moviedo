#pragma once

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>

/*
* Common constants
*/
#define MAX_PACKET_SIZE (1024 + 12)

/*
* Common types
*/

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef size_t usize;
typedef __ssize_t isize;

/*
 * Utility macros 
 */

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#ifdef AC_ASSERTS
#define ASSERT(X) assert(X)
#else
#define ASSERT(X) 
#endif

/*
 * Common functions
 */

extern bool DEBUG_TRACES_ENABLED;
void verboseInfo(const char* fmt, ...);
void trace(const char* fmt, ...); 
void _printError(const char* file, int line, const char* fmt, ...);
#define printError(fmt, ...) _printError(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define panic(fmt, ...) (printError(fmt, ##__VA_ARGS__), exit(1))