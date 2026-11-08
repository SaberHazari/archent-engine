#ifndef ARCHENT_MEMORY_H
#define ARCHENT_MEMORY_H

#include "archent_utils.h"
#include "archent_arena.h"
#include <assert.h>

typedef struct ArchentMemory {
    ArchentArena permanent;
    ArchentArena transient;
} ArchentMemory;

typedef struct TempMemory {
    ArchentArena *arena;
    ArenaMark mark;
} TempMemory;

static inline void memory_init(ArchentMemory *memory, ArenaAllocFn alloc, ArenaFreeFn free, 
    void *user, usize permanent_min_block_size, usize transient_min_block_size) {
    arena_init(&memory->permanent, "Permanent", permanent_min_block_size, alloc, free, user);
    arena_init(&memory->transient, "Transient", transient_min_block_size, alloc, free, user);
}

static inline void memory_init_default(ArchentMemory *memory, usize permanent_min_block_size, 
    usize transient_min_block_size) {
    memory_init(memory, arena_default_alloc, arena_default_free, NULL, 
        permanent_min_block_size, transient_min_block_size);
}

static inline void memory_frame_reset(ArchentMemory *memory) {
    arena_clear(&memory->transient);
}

static inline void memory_destroy(ArchentMemory *memory) {
    arena_destroy(&memory->permanent);
    arena_destroy(&memory->transient);
}

static inline TempMemory temp_memory_begin(ArchentArena *arena) {
    ++arena->temp_count;
    TempMemory temp = { arena, arena_mark(arena) };
    return temp;
}

static inline void temp_memory_end(TempMemory temp) {
    assert(temp.arena->temp_count > 0 && "temp_memory_end without a temp_memory_begin");
    --temp.arena->temp_count;
    arena_pop_to(temp.arena, temp.mark);
}

#endif