#ifndef RNG_H
#define RNG_H

#include "utils.h"
#include <math.h>
#include <assert.h>

typedef enum RngSalt {
    RNG_SALT_NONE = 0x00
} RngSalt;

typedef struct RngStream {
    u64 state;
} RngStream;

typedef struct RngAliasTable {
    u32 n;
    f32 *prob;
    u32 *alias;
} RngAliasTable;

static inline u64 rng_splitmix64(u64 num) {
    num += 0x9E3779B97F4A7C15ull;
    num = (num ^ (num >> 30)) * 0xBF58476D1CE4E5B9ull;
    num = (num ^ (num >> 27)) * 0x94D049BB133111EBull;
    return num ^ (num >> 31);
}

static inline u64 rng_hash(u64 seed, u64 a, u64 b, 
    RngSalt salt) {
    return rng_splitmix64(seed ^ rng_splitmix64(a ^ 
        rng_splitmix64(b ^ rng_splitmix64((u64)salt))));
}

static inline RngStream rng_stream_begin(u64 seed, u64 a, u64 b, 
    RngSalt salt) {
    RngStream stream = { rng_hash(seed, a, b, salt) };
    return stream;
}

static inline u64 rng_stream_next(RngStream *stream) {
    return stream->state = rng_splitmix64(stream->state);
}

static inline u64 rng_stream_at(u64 seed, u64 a, u64 b, 
    RngSalt salt, u64 i) {
    return rng_hash(seed, a, b ^ (i * 0x9E3779B97F4A7C15ull), salt);
}

static inline f32 rng_f32(u64 hash) {
    return (f32)(hash >> 40) * (1.0f / 16777216.0f);
}

static inline u32 rng_range_zero_u32(u64 hash, u32 n) {
    return (u32)(((hash >> 32) * (u64)n) >> 32);
}

static inline bool rng_bool(u64 hash) {
    return (hash >> 63) != 0;
}

static inline bool rng_chance(u64 hash, f32 prob) {
    return rng_f32(hash) < prob;
}

static inline i32 rng_sign(u64 hash) {
    return (i32)(hash & 1) * 2 - 1;
}

static inline f32 rng_range_f32(u64 hash, f32 lo, f32 hi) {
    return lo + rng_f32(hash) * (hi - lo);
}

static inline u32 rng_range_u32(u64 hash, u32 lo, u32 hi) {
    if(hi <= lo) { return lo; }
    return lo + rng_range_zero_u32(hash, (hi - lo));
}

static inline i32 rng_range_i32(u64 hash, i32 lo, i32 hi) {
    if(hi <= lo) { return lo; }
    u32 offset = rng_range_zero_u32(hash, (u32)hi - (u32)lo);
    return (i32)((u32)lo + offset);
}

static inline f32 rng_normal_fast(u64 hash1, u64 hash2, u64 hash3) {
    f32 sum = rng_f32(hash1) + rng_f32(hash2) + rng_f32(hash3);
    return (sum - 1.5f) * 2.0f;
}

static inline f32 rng_normal_evt(u64 seed, u64 a, u64 b, 
    RngSalt salt) {
    RngStream stream = rng_stream_begin(seed, a, b, salt);
    u64 hash1 = rng_stream_next(&stream);
    u64 hash2 = rng_stream_next(&stream);
    u64 hash3 = rng_stream_next(&stream);
    return rng_normal_fast(hash1, hash2, hash3);
}

static inline f32 rng_exponential(u64 hash, f32 lambda) {
    assert(lambda > 0.0f);
    if(lambda <= 0.0f) { lambda = 0.01f; }
    return -logf(1.0f - rng_f32(hash)) / lambda;
}

static inline u32 rng_poisson_small(u64 seed, u64 a, u64 b, 
RngSalt salt, f32 lambda) {
    assert(lambda > 0.0f && lambda < 10.0f);
    if(lambda <= 0.0f) { lambda = 0.01f; }
    if(lambda >= 10.0f) { lambda = 9.99f; }
    f32 limit = expf(-lambda), p = 1.0f;
    u32 k = 0;
    RngStream stream = rng_stream_begin(seed, a, b, salt);
    do { 
        ++k;
        p *= rng_f32(rng_stream_next(&stream));
    } while(p > limit);
    return k - 1;
}

void rng_alias_build(const f32 *weights, u32 n, f32 *prob, 
    u32 *alias, u32 *small_stack, u32 *large_stack, 
    RngAliasTable *out);

static inline u32 rng_alias_pick(const RngAliasTable *table, u64 hash) {
    u32 i = rng_range_zero_u32(hash, table->n);
    f32 u = rng_f32(rng_splitmix64(hash));
    return (u < table->prob[i]) ? i : table->alias[i];
}

void rng_shuffle(u64 seed, u64 a, u64 b, 
    RngSalt salt, u32 *arr, u32 n);

#endif