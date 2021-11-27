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

#define INRANGE(x, min, max) ((x) >= (min) && x < (max))
#define ABS(x) ((x) < 0 ? -(x) : (x))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define ARRAY_COUNT(A) (sizeof(A)/sizeof(A[0]))
#define ARRAY_COUNT32(A) ((u32)ARRAY_COUNT(A))

#ifdef AC_ASSERTS
#define ASSERT(X) assert(X)
#else
#define ASSERT(X) 
#endif

//a must be a power of 2
#define ALIGN_DOWN(x, a) ((x) & ~((a)-1))

#define ZERO_SIZE(ptr, size) memset((ptr), 0, (size)) 
#define ZERO_STRUCT(STRUCT) ZERO_SIZE(&(STRUCT), sizeof(STRUCT))
#define ZERO_ARRAY(ARRAY) ZERO_SIZE(ARRAY, sizeof(ARRAY))

/*
 * Common functions
 */

extern bool DEBUG_TRACES_ENABLED;
void trace(const char* fmt, ...); 
void _printError(const char* file, int line, const char* fmt, ...);
#define printError(fmt, ...) _printError(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define panic(fmt, ...) (printError(fmt, ##__VA_ARGS__), exit(1))