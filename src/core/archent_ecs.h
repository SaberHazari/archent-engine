#ifndef ARCHENT_ECS_H
#define ARCHENT_ECS_H

#include "archent_arena.h"
#include <assert.h>

#if defined(_MSC_VER)
    #include <intrin.h>
#endif

#define MAX_COMPONENTS 1024
#define SIG_WORDS ((MAX_COMPONENTS + 63) / 64)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Entity {
    u32 index;
    u32 generation;
} Entity;

static const Entity ENTITY_NULL = { 0, 0 };

typedef struct CompMask {
    u64 words[SIG_WORDS];
} CompMask;

static inline u32 ecs_popcount64_(u64 x) {
    #if defined(__GNUC__) || defined(__clang__)
        return (u32)__builtin_popcountll(x);
    #elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        return (u32)__popcnt64(x);
    #else
        x = x - ((x >> 1) & 0x5555555555555555ull);
        x = (x & 0x3333333333333333ull) + ((x >> 2) & 0x3333333333333333ull);
        x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0Full;
        return (u32)((x * 0x0101010101010101ull) >> 56);
    #endif
}

// count trailing zeroes
static inline u32 ecs_ctz64_(u64 x) {
    assert(x != 0);
    #if defined(__GNUC__) || defined(__clang__)
        return (u32)__builtin_ctzll(x);
    #elif defined(_MSC_VER)
        unsigned long index;
        _BitScanFoward64(&index, x);
        return (u32)index;
    #else
        u32 n = 0;
        while(!(x & 1ull)) {
            x >>= 1;
            ++n;
        }
        return n;
    #endif
}

static inline bool entity_equal(Entity x, Entity y) {
    return x.index == y.index && 
        x.generation == y.generation;
}

static inline bool entity_is_null(Entity ent) {
    return ent.generation == 0;
}

static inline u64 entity_pack(Entity ent) {
    return ((u64)ent.generation << 32) | (u64)ent.index;
}

static inline Entity entity_unpack(u64 pack) {
    Entity ent = { (u32)pack, (u32)(pack >> 32) };
    return ent;
}

static inline void comp_mask_set(CompMask *mask, u32 comp_id) {
    mask->words[comp_id >> 6] |= (1ull << (comp_id & 63));
}

static inline void comp_mask_clear(CompMask *mask, u32 comp_id) {
    mask->words[comp_id >> 6] &= ~(1ull << (comp_id & 63));
}

static inline bool comp_mask_has(const CompMask *mask, u32 comp_id) {
    return (mask->words[comp_id >> 6] >> (comp_id & 63)) & 1ull;
}

static inline bool comp_mask_subset(const CompMask *sub, const CompMask *super) {
    for(u32 i = 0; i < SIG_WORDS; ++i) {
        if((sub->words[i] & super->words[i]) != sub->words[i]) {
            return false;
        }
    }
    return true;
}

static inline comp_mask_equal(CompMask *x, CompMask *y) {
    for(u32 i = 0; i < SIG_WORDS; ++i) {
        if(x->words[i] != y->words[i]) {
            return false;
        }
    }
    return true;
}

#ifdef __cplusplus
}
#endif

#endif