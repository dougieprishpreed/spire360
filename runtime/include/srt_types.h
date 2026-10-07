/*
 * srt_types.h - fixed-size integer types for the runtime's public headers.
 *
 * Named srt_* so they never clash with the U8..U64 typedefs in w2c2_base.h,
 * which the runtime's .c files also include. C89; no stdint.h needed.
 */
#ifndef SRT_TYPES_H
#define SRT_TYPES_H

typedef unsigned char srt_u8;
typedef signed char srt_i8;
typedef unsigned short srt_u16;
typedef signed short srt_i16;
typedef unsigned int srt_u32;
typedef signed int srt_i32;
#if defined(_MSC_VER) && _MSC_VER <= 1000
typedef unsigned __int64 srt_u64;
typedef signed __int64 srt_i64;
#else
typedef unsigned long long int srt_u64;
typedef signed long long int srt_i64;
#endif

#endif /* SRT_TYPES_H */
