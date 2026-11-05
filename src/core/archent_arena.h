#ifndef ARCHENT_ARENA_H
#define ARCHENT_ARENA_H

#include "archent_utils.h"
#include <stdlib.h>
#include <string.h>

#define ARENA_MIN_ALIGN 16u

#define arena_new_m(a, T) ((T *)arena_push_zero((a), sizeof(T), _Alignof(T)))
#define arena_array_m(a, T, n) ((T *)arena_push_array((a), sizeof(T), _Alignof(T), (n)))

typedef void *(*ArenaAllocFn)(usize size, void *user);
typedef void (*ArenaFreeFn)(void *ptr, usize size, void *user);

static inline void *arena_default_alloc(usize size, void *user) {
    (void)user;
    return malloc(size);
}
static inline void arena_default_free(void *ptr, usize size, void *user) {
    (void)size; (void)user;
    free(ptr);
}

typedef struct ArenaBlock ArenaBlock;
struct ArenaBlock {
    ArenaBlock *prev;
    ArenaBlock *next;
    usize size;
    usize used;
};

typedef struct ArchentArena {
    const char *name;
    
    ArenaBlock *first_block;
    ArenaBlock *current_block;
    usize minimum_block_size;
    
    usize total_size;
    usize total_used;
    usize peak;
    
    ArenaAllocFn alloc;
    ArenaFreeFn free;
    void *alloc_user;
} ArchentArena;

typedef struct ArenaMark {
    ArenaBlock *block;
    usize used;
} ArenaMark;

typedef void (*ArenaFailFn)(const ArchentArena *arena, usize size, usize align);
#ifdef ARENA_IMPLEMENTATION
    ArenaFailFn arena_fail_handler = NULL;
#else
    extern ArenaFailFn arena_fail_handler;
#endif

static inline void arena_fail_(const ArchentArena *arena, usize size, usize align) {
    if(arena_fail_handler) { arena_fail_handler(arena, size, align); }
    abort();
}

#undef ARENA_ASAN
#if defined(__SANITIZE_ADDRESS__)
    #define ARENA_ASAN 1
#elif defined(__has_feature)
    #if __has_feature(address_sanitizer)
        #define ARENA_ASAN 1
    #endif
#endif

#if defined(ARENA_ASAN)
    #include <sanitizer/asan_interface.h>
    #define arena_poison_m_(p, n) ASAN_POISON_MEMORY_REGION((p), (n))
    #define arena_unpoison_m_(p, n) ASAN_UNPOISON_MEMORY_REGION((p), (n))
#else
    #define arena_poison_m_(p, n) ((void)(p), (void)(n))
    #define arena_unpoison_m_(p, n) ((void)(p), (void)(n))
#endif

void arena_init(ArchentArena *arena, const char *name, usize minimum_block_size, 
    ArenaAllocFn alloc, ArenaFreeFn free, void *user);

static inline void arena_init_default(ArchentArena *arena, const char * name, usize minimum_block_size) {
    arena_init(arena, name, minimum_block_size, 
        arena_default_alloc, arena_default_free, NULL);
}

void *arena_push(ArchentArena *arena, usize size, usize align);

static inline void *arena_push_zero(ArchentArena *arena, usize size, usize align) {
    void *result = arena_push(arena, size, align);
    if(result) { memset(result, 0, size); }
    return result;
}

static inline void *arena_push_array(ArchentArena *arena, usize element, usize align, usize count) {
    if(element == 0 || count == 0) { return NULL; }
    if(count > (usize)-1 / element) {
        arena__fail(arena, (usize)-1, align);
        return NULL;
    }
    return arena_push_zero(arena, element * count, align);
}

static inline ArenaMark arena_mark(const ArchentArena *arena) {
    ArenaBlock *block = arena->current_block;
    return (ArenaMark){ block, block ? block->used : 0 };
}

void arena_pop_to(ArchentArena *arena, ArenaMark mark);
void arena_clear(ArchentArena *arena);
void arena_destroy(ArchentArena *arena);

#endif