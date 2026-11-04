#include "ecs.h"
#include "rng.h"

static u8 ecs__tag_storage[16];

static inline usize ecs__align_up(usize n, usize align) {
    return (n + (align - 1)) & ~(align - 1);
}

static inline u64 ecs__key(u32 comp_id) {
    return rng_splitmix64((u64)comp_id);
}

static inline u32 archetype__column(const Archetype *arch, 
    u32 comp_id) {
    return comp_id < arch->column_of_len ? 
        arch->column_of[comp_id] : ECS_NO_COLUMN;
}

static inline void *archetype__component(const Archetype *arch, 
    u32 comp_id, u32 row) {
    const CompColumn *col = &arch->columns[arch->column_of[comp_id]];
    return col->data + (usize)row * col->size;
}

static u32 world__find_archetype(const World *world, 
    const CompMask *sig, u64 hash) {
    u32 i = (u32)hash & world->hash_mask;
    for(;;) {
        u32 entry = world->hash_table[i];
        if(!entry) { return ECS_NONE; }
        const Archetype *arch = &world->archetypes[entry - 1];
        if(arch->hash == hash && comp_mask_equal(&arch->sig, sig)) {
            return entry - 1;
        }
        i = (i + 1) & world->hash_mask;
    }
}

static bool world__grow_hash(World *world) {
    u32 cap = world->hash_table ? 
        (world->hash_mask + 1) * 2 : 64;
    u32 *table = arena_array_m(world->arena, u32, cap);
    if(!table) { return false; }
    for(u32 a = 0; a < world->archetype_count; ++a) {
        u32 i = (u32)world->archetypes[a].hash & (cap - 1);
        while(table[i]) {
            i = (i + 1) & (cap - 1);
        }
        table[i] = a + 1;
    }
    world->hash_table = table;
    world->hash_mask = cap - 1;
    return true;
}

static u32 world__create_archetype(World *world, 
    const CompMask *sig, u64 hash) {
    if(world->archetype_count == world->archetype_capacity) {
        u32 cap = world->archetype_capacity ? 
            world->archetype_capacity * 2 : 16;
        Archetype *archs = arena_array_m(world->arena, Archetype, cap);
        if(!archs) { return ECS_NONE; }
        if(world->archetype_count) {
            memcpy(archs, world->archetypes, 
                world->archetype_count * sizeof(Archetype));
        }
        world->archetypes = archs;
        world->archetype_capacity = cap;
    }
    
    if((world->archetype_count + 1) * 2 > world->hash_mask + 1 && 
        !world__grow_hash(world)) {
        return ECS_NONE;
    }
    
    u32 comp_count = 0;
    for(u32 w = 0; w < SIG_WORDS; ++w) {
        comp_count += ecs__popcount64(sig->words[w]);
    }
    CompColumn *cols = comp_count ? 
        arena_array_m(world->arena, CompColumn, comp_count) : NULL;
    if(comp_count && !cols) { return ECS_NONE; }
    
    Archetype *arch = &world->archetypes[world->archetype_count];
    memset(arch, 0, sizeof(*arch));
    arch->sig = *sig;
    arch->hash = hash;
    arch->columns = cols;
    arch->component_count = comp_count;
    u32 k = 0;
    for(u32 w = 0; w < SIG_WORDS; ++w) {
        u64 bits = sig->words[w];
        while(bits) {
            u32 id = (w << 6) + ecs__ctz64(bits);
            bits &= bits - 1;
            u32 size = world->components[id].size;
            cols[k] = (CompColumn){size ? NULL : 
                ecs__tag_storage, id, size };
            ++k;
        }
    }
    
    if(comp_count) {
        u32 len = cols[comp_count - 1].id + 1;
        u16 *column_of = arena_array_m(world->arena, u16, len);
        if(!column_of) { return ECS_NONE; }
        for(u32 id = 0; id < len; ++id) {
            column_of[id] = ECS_NO_COLUMN;
        }
        for(u32 i = 0; i < comp_count; ++i) {
            column_of[cols[i].id] = (u16)i;
        }
        arch->column_of = column_of;
        arch->column_of_len = len;
    }
    
    u32 index = world->archetype_count++;
    u32 i = (u32)hash & world->hash_mask;
    while(world->hash_table[i]) {
        i = (i + 1) & world->hash_mask;
    }
    world->hash_table[i] = index + 1;
    return index;
}
