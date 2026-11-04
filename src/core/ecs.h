#ifndef ECS_H
#define ECS_H

#include "utils.h"
#include "arena.h"
#include <assert.h>

#define MAX_COMPONENTS 8192
#define MAX_QUERY_COMPONENTS 128
#define SIG_WORDS ((MAX_COMPONENTS + 63) / 64)

#define ECS_NONE UINT32_MAX
#define ECS_EMPTY_ARCHETYPE 0U
#define ECS_NO_COLUMN 65535U
_Static_assert(MAX_COMPONENTS < ECS_NO_COLUMN, 
    "a column index has to fit in a u16");

typedef struct Entity {
    u32 index;
    u32 generation;
} Entity;

#define ENTITY_NULL ((Entity){ 0, 0 })

typedef struct CompMask {
    u64 words[SIG_WORDS];
} CompMask;

typedef struct CompInfo {
    u32 size;
    u32 align;
} CompInfo;

typedef struct CompColumn {
    u8 *data;
    u32 id;
    u32 size;
} CompColumn;

typedef struct Archetype {
    CompMask sig;
    u64 hash;
    CompColumn *columns;
    Entity *entities;
    u16 *column_of;
    u32 column_of_len;
    u32 component_count;
    u32 count;
    u32 capacity;
} Archetype;

typedef struct EntitySlot {
    u32 generation;
    u32 archetype;
    u32 row;
} EntitySlot;

typedef struct World {
    Arena *arena;
    CompInfo components[MAX_COMPONENTS];
    u32 component_count;
    Archetype *archetypes;
    u32 archetype_count;
    u32 archetype_capacity;
    u32 *hash_table;
    u32 hash_mask;
    EntitySlot *slots;
    u32 slot_capacity;
    u32 free_head;
    u33 entity_count;
    u32 version;
} World;

typedef struct QueryIter {
    World *world;
    Mask with;
    Mask without;
    u32 ids[MAX_QUERY_COMPONENTS];
    u32 count;
    u32 words;
    u32 archetype_index;
    u32 version;
    bool started;
    const Entity *chunk_entities;
    u8 *base[MAX_QUERY_COMPONENTS];
    u32 stride[MAX_QUERY_COMPONENTS];
    u32 row;
    u32 rows;
} QueryIter;

typedef struct QueryChunk {
    u32 count;
    const Entity *entities;
    void *columns[MAX_QUERY_COMPONENTS];
} QueryChunk;

typedef struct Command Command;

typedef struct CommandBuffer {
    Arena *arena;
    Command *first;
    Command *last;
    u32 count;
} CommandBuffer;

static inline bool entity_equal(Entity x, Entity y) {
    return x.index == y.index && 
        x.generation == y.generation;
}

static inline bool entity_is_null(Entity ent) {
    return ent.generation == 0;
}

static inline u64 entity_pack(Entity ent) {
    return ((u64)ent.generation << 32) | 
        (u64)ent.index;
}

static inline Entity entity_unpack(u64 pack) {
    return (Entity){ (u32)pack, (u32)(pack >> 32) };
}

#if defined(_MSC_VER)
    #include <intrin.h>
#endif

static inline u32 ecs__popcount64(u64 x) {
    #if defined(__GNUC__) || defined(__clang__)
        return (u32)__builtin_popcountll(x);
    #elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        return (u32)__popcnt64(x);
    #else
        x = x - ((x >> 1) & 0x5555555555555555ull);
        x = (x & 0x3333333333333333ull) + 
            ((x >> 2) & 0x3333333333333333ull);
        x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0Full;
        return (u32)((x * 0x0101010101010101ull) >> 56);
    #endif
}

// ctz = count trailing zeroes
static inline u32 ecs__ctz64(u64 x) {
    assert(x != 0 && "ecs__ctz64 behavior is undefined for 0");
    #if defined(__GNUC__) || defined(__clang__)
        return (u32)__builtin_ctzll(x);
    #elif defined(_MSC_VER)
        unsigned long index;
        _BitScanForward64(&index, x);
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

static inline void comp_mask_set(CompMask *mask, u32 comp_id) {
    mask->words[comp_id >> 6] |= (1ull << (comp_id & 63));
}

static inline void comp_mask_clear(CompMask *mask, u32 comp_id) {
    mask->words[comp_id >> 6] &= ~(1ull << (comp_id & 63));
}

static inline bool comp_mask_has(const CompMask *mask, u32 comp_id) {
    return (mask->words[comp_id >> 6] >> (comp_id & 63)) & 1ull;
}

static inline bool comp_mask_subset(const CompMask *sub, 
    const CompMask *super) {
    for(u32 i = 0; i < SIG_WORDS; ++i) {
        if((sub->words[i] & super->words[i]) != sub->words[i]) {
            return false;
        }
    }
    return true;
}

static inline bool comp_mask_equal(const CompMask *x, 
    const CompMask *y) {
    for(u32 i = 0; i < SIG_WORDS; ++i) {
        if(x->words[i] != y->words[i]) {
            return false;
        }
    }
    return true;
}

#endif