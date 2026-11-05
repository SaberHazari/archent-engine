#define ARENA_IMPLEMENTATION
#include "archent_arena.h"
#include <assert.h>

static inline usize arena_align_up_(usize n, usize align) {
    return (n + (align - 1)) & ~(align - 1);
}

static inline uptr arena_align_up_ptr_(uptr addr, usize align) {
    return (addr + (align - 1)) & ~(uptr)(align - 1);
}

static inline usize arena_block_header_size_(void) {
    return arena__align_up(sizeof(ArenaBlock), ARENA_MIN_ALIGN);
}

static inline u8 *arena_block_data_(ArenaBlock *block) {
    return (u8 *)block + arena_block_header_size_();
}

static inline ArenaBlock *arena_new_block_(ArchentArena *arena, usize min_data_size) {
    usize data_size = arena->minimum_block_size;
    if(data_size < min_data_size) { data_size = min_data_size; }
    if(data_size > SIZE_MAX - arena_block_header_size_() - (ARENA_MIN_ALIGN - 1)) {
        arena_fail_(arena, data_size, ARENA_MIN_ALIGN);
        return NULL;
    }
    data_size = arena_align_up_(data_size, ARENA_MIN_ALIGN);
    
    usize total = arena_block_header_size_() + data_size;
    u8 *raw = (u8 *)arena->alloc(total, arena->alloc_user);
    if(!raw) {
        arena_fail_(arena, data_size, ARENA_MIN_ALIGN);
        return NULL;
    }
    assert(((uptr)raw & (ARENA_MIN_ALIGN - 1)) == 0 && 
    "allocator must return ARENA_MIN_ALIGN-aligned memory");
    
    ArenaBlock *block = (ArenaBlock *)raw;
    block->prev = arena->current_block;
    block->next = NULL;
    block->size = data_size;
    block->used = 0;
    
    if(arena->current_block) { arena->current_block->next = block; }
    else { arena->first_block = block; }
    
    arena->current_block = block;
    arena->total_size += data_size;
    
    arena_poison_m_(arena_block_data_(block), data_size);
    return block;
}

void arena_init(ArchentArena *arena, const char *name, usize minimum_block_size, 
    ArenaAllocFn alloc, ArenaFreeFn free, void *user) {
    assert(arena && alloc && free && minimum_block_size > 0u);
    assert((minimum_block_size & (ARENA_MIN_ALIGN - 1u)) == 0u && 
        "minimum block size must be ARENA_MIN_ALIGN-aligned");
    
    arena->name = name;
    arena->first_block = NULL;
    arena->current_block = NULL;
    arena->minimum_block_size = minimum_block_size;
    arena->total_size = 0;
    arena->total_used = 0;
    arena->peak = 0;
    arena->alloc = alloc;
    arena->free = free;
    arena->alloc_user = user;
}

void *arena_push(ArchentArena *arena, usize size, usize align) {
    assert(arena);
    assert(align && (align & (align - 1)) == 0 && 
        "align must be a power of two");
    if(size == 0) { return NULL; }
    
    ArenaBlock *block = arena->current_block;
    if(block) {
        uptr base = (uptr)arena_block_data_(block);
        uptr addr = base + block->used;
        uptr aligned = arena_align_up_ptr_(addr, align);
        usize off = (usize)(aligned - base);
        
        if(off <= block->size && size <= block->size - off) {
            void *result = (u8 *)aligned;
            usize new_used = off + size;
            arena->total_used += new_used - block->used;
            if(arena->total_used > arena->peak) { arena->peak = arena->total_used; }
            block->used = new_used;
            arena_unpoison_m_(result, size);
            return result;
        }
    }
    
    if(size > SIZE_MAX - align) {
        arena_fail_(arena, size, align);
        return NULL;
    }
    ArenaBlock *fresh = arena_new_block_(arena, size + align);
    if(!fresh) { return NULL; }
    
    uptr base = (uptr)arena_block_data_(fresh);
    uptr aligned = arena_align_up_ptr_(base, align);
    usize off = (usize)(aligned - base);
    
    void *result = (u8 *)aligned;
    usize new_used = off + size;
    arena->total_used += new_used;
    if(arena->total_used > arena->peak) { arena->peak = arena->total_used; }
    fresh->used = new_used;
    arena_unpoison_m_(result, size);
    return result;
}

void arena_pop_to(ArchentArena *arena, ArenaMark mark) {
    assert(arena);
    
    ArenaBlock *block = arena->current_block;
    while(block != mark.block) {
        assert(block && "mark doesn't belong to this arena");
        ArenaBlock *prev = block->prev;
        usize total = arena_block_header_size_() + block->size;
        arena_unpoison_m_(arena_block_data_(block), block->size);
        arena->total_size -= block->size;
        arena->total_used -= block->used;
        arena->free(block, total, arena->alloc_user);
        block = prev;
    }
    
    arena->current_block = mark.block;
    
    if(arena->current_block) {
        arena->current_block->next = NULL;
        usize old_used = arena->current_block->used;
        assert(mark.used <= old_used && "mark is newer than current usage");
        if(mark.used < old_used) {
            arena_poison_m_(arena_block_data_(arena->current_block) + mark.used, old_used - mark.used);
            arena->total_used -= (old_used - mark.used);
        }
        arena->current_block->used = mark.used;
    } else {
        arena->first_block = NULL;
    }
}

void arena_clear(ArchentArena *arena) {
    assert(arena);
    if(!arena->first_block) { return; }
    
    ArenaBlock *first = arena->first_block;
    ArenaBlock *block = arena->current_block;
    while(block != first) {
        ArenaBlock *prev = block->prev;
        usize total = arena_block_header_size_() + block->size;
        arena_unpoison_m_(arena_block_data_(block), block->size);
        arena->total_size -= block->size;
        arena->free(block, total, arena->alloc_user);
        block = prev;
    }
    
    first->next = NULL;
    if(first->used) {
        arena_poison_m_(arena_block_data_(first), first->used);
    }
    first->used = 0;
    
    arena->current_block = first;
    arena->total_used = 0;
}

void arena_destroy(ArchentArena *arena) {
    assert(arena);
    ArenaBlock *block = arena->first_block;
    while(block) {
        ArenaBlock *next = block->next;
        usize total = arena_block_header_size_() + block->size;
        arena_unpoison_m_(arena_block_data_(block), block->size);
        arena->free(block, total, arena->alloc_user);
        block = next;
    }
    
    arena->first_block = NULL;
    arena->current_block = NULL;
    arena->total_size = 0;
    arena->total_used = 0;
}