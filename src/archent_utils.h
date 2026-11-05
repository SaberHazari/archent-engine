#ifndef ARCHENT_UTILS_H
#define ARCHENT_UTILS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef size_t usize;
typedef uintptr_t uptr;

#define kilo_bytes_m(value) ((u64)(value) * (u64)1024)
#define mega_bytes_m(value) (kilo_bytes_m(value) * (u64)1024)
#define giga_bytes_m(value) (mega_bytes_m(value) * (u64)1024)

#endif