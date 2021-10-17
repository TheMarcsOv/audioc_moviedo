#pragma once

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

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

#define U32_MAX ((u32)0xFFFFFFFF)
#define U32_MIN ((u32)0)
#define I32_MAX ((i32)0x7FFFFFFF)
#define I32_MIN ((i32)0x80000000)

#define KB(kb) ((kb)*1024)
#define MB(mb) (KB(mb)*1024)
#define GB(gb) (MB(mb)*1024)

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

/*
 * Utility functions 
 */

#define ALIGN_DOWN(x, a) ((x) & ~((a)-1))
#define ALIGN_UP(x, a) ALIGN_DOWN((x) + (a) - 1, (a))
#define ALIGN_DOWN_PTR(x, a) ((void*)ALIGN_DOWN((uintptr_t)(x), (a)))
#define ALIGN_UP_PTR(x, a) ((void*)ALIGN_UP((uintptr_t)(x), (a)))

#define ZERO_SIZE(ptr, size) memset((ptr), 0, (size)) 
#define ZERO_STRUCT(STRUCT) ZERO_SIZE(&(STRUCT), sizeof(STRUCT))
#define ZERO_ARRAY(ARRAY) ZERO_SIZE(ARRAY, sizeof(ARRAY))