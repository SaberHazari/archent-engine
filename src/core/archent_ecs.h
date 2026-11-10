#ifndef ARCHENT_ECS_H
#define ARCHENT_ECS_H

#include "archent_arena.h"

#define MAX_COMPONENTS 1024
#define SIG_WORDS ((MAX_COMPONENTS + 63) / 64)

#ifdef __cplusplus
extern "C" {
#endif
typedef struct CompMask {
    u64 words[SIG_WORDS];
} CompMask;

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