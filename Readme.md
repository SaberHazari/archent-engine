# Archent

Archent is a game engine written in C17, built around an entity component system. It's early. The repo has an arena allocator, a random number module and the start of a Windows platform layer. The ECS itself isn't written yet.

## Status

The arena allocator, the permanent/transient memory split and the rng module are written. The Windows platform layer is a skeleton: `WinMain` exists with an empty body, and the input, performance and software renderer files have nothing in them. `archent_ecs.h` and `archent_ecs.c` are empty too.

There is no Linux platform layer yet. The Makefile already has Linux targets and looks for sources in `src/platforms/linux/`. There are no tests in the repo.

## Layout

```
Makefile                                   MinGW-w64 for Windows (run with wine), gcc-13 for Linux
src/
  archent_utils.h                          typedefs and size macros
  core/
    archent_arena.h, .c                    arena allocator
    archent_memory.h                       permanent and transient arenas, temp scopes
    archent_rng.h, .c                      random numbers
    archent_ecs.h, .c                      empty
  platform_communication/
    platform_communication.h, .c           state shared with the platform layer
  platforms/
    windows/
      windows_archent.h, .c                WinMain, defines archent_state
      windows_input_system.h, .c           empty
      windows_performance.h, .c            empty
      windows_software_renderer.h, .c      empty
```

Nothing in `core/` includes platform headers.

## Conventions

The code is C17 and is compiled with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes -Wmissing-prototypes`.

- Fixed-width types come from `archent_utils.h`: `i8` to `i64`, `u8` to `u64`, `f32`, `f64` and `b32`, plus `usize` and `uptr`. `kilo_bytes_m`, `mega_bytes_m` and `giga_bytes_m` give byte counts as `u64`.
- Functions start with the name of their module: `arena_push`, `memory_init`, `rng_hash`. Types are PascalCase: `ArchentArena`, `RngStream`.
- Function-like macros end in `_m` (`arena_new_m`, `kilo_bytes_m`). Constants and enum values are upper case (`ARENA_MIN_ALIGN`, `RNG_SALT_NONE`).
- A trailing underscore means internal: `arena_fail_`, `arena_block_data_`, `arena_poison_m_`. Don't call these from outside their module, and expect them to change. A few sit in headers only because an inline function uses them. The marker is a trailing underscore because C++ reserves identifiers that contain a double underscore, and the engine may end up linking with C++ code.
- Bad arguments (an alignment that isn't a power of two, a mark from another arena) trip an `assert`. They don't return an error code. Release builds define `NDEBUG`, so the asserts are compiled out there.

## Memory

### Arena

`archent_arena.h`

An `ArchentArena` hands out memory by moving an offset forward through a chain of blocks. Each block is an `ArenaBlock` header (`prev`, `next`, `size`, `used`, padded to 16 bytes) followed by its data. You don't free single allocations. Memory comes back in bulk, by rewinding to a mark, clearing the arena or destroying it.

The arena gets its blocks from two functions that you pass to `arena_init`:

```c
typedef void *(*ArenaAllocFn)(usize size, void *user);
typedef void (*ArenaFreeFn)(void *ptr, usize size, void *user);

void arena_init(ArchentArena *arena, const char *name, usize minimum_block_size,
                ArenaAllocFn alloc, ArenaFreeFn free, void *user);
```

The allocator must return memory aligned to 16 bytes (`ARENA_MIN_ALIGN`). The free function is called with the same size the allocator was asked for. `arena_init_default` passes `arena_default_alloc` and `arena_default_free`, which call `malloc` and `free`. The plan is for each platform layer to pass its own, backed by the OS (`VirtualAlloc` on Windows).

`arena_init` doesn't allocate. The first push does. `minimum_block_size` has to be a non-zero multiple of 16. When a push doesn't fit in the current block, a new block goes on the end of the chain. It holds at least `minimum_block_size` bytes, more if the request plus its alignment is bigger. Whatever space was left at the end of the previous block isn't used again until that block is rewound.

#### Pushing

- `arena_push(arena, size, align)` returns uninitialized memory. `align` must be a power of two, any power of two. A `size` of 0 is treated as 1, so the result is never `NULL`.
- `arena_push_zero(arena, size, align)` is the same, zeroed.
- `arena_push_array(arena, element, align, count)` returns zeroed memory for `count` elements of `element` bytes each. A multiplication overflow counts as an allocation failure. A count or element size of 0 is handled like a `size` of 0 in `arena_push`.
- `arena_new_m(arena, T)` and `arena_array_m(arena, T, count)` are the typed versions. They're zeroed and use `sizeof` and `_Alignof` of `T`.

#### Rewinding

```c
ArenaMark mark = arena_mark(&arena);
/* ... pushes ... */
arena_pop_to(&arena, mark);
```

`arena_pop_to` frees every block added after the mark and sets the marked block's `used` back to where it was. Marks have to be used in order: popping to a mark invalidates every mark taken after it. A mark taken on an arena with no blocks rewinds to no blocks, so the first block is freed too.

`arena_clear` frees every block except the first and resets the first to empty. If the arena had to use more than one block, the first block is also replaced by one as big as all the blocks were together, so the next frame of the same size fits in a single block.

`arena_destroy` frees all the blocks. The arena keeps its allocator, so it can be pushed to again afterwards.

#### Counters

`total_size` is the bytes of block data (headers not counted), `total_used` is the bytes handed out including alignment padding, and `peak` is the highest `total_used` has reached. Clearing or destroying the arena doesn't reset `peak`. `name` is stored for debugging; nothing reads it yet.

#### Failure

If an allocation fails or a size overflows, the arena calls `arena_fail_handler` if one is set, then `abort()`:

```c
typedef void (*ArenaFailFn)(const ArchentArena *arena, usize size, usize align);
extern ArenaFailFn arena_fail_handler;
```

A push never returns `NULL`.

#### AddressSanitizer

When built with `-fsanitize=address` the arena poisons the unused part of each block. A push unpoisons exactly the bytes it hands out, and rewinding or clearing poisons them again, so touching memory after a rewind gets reported.

An arena isn't thread safe.

### Permanent and transient

`archent_memory.h`

`ArchentMemory` is two arenas, named "Permanent" and "Transient", that share the same allocator functions.

```c
ArchentMemory memory;
memory_init_default(&memory, mega_bytes_m(256), mega_bytes_m(32));
```

The two numbers are the minimum block sizes for the permanent and transient arenas. `memory_init` takes the allocator functions and the user pointer as well.

Permanent is for whatever lives as long as the program does. Nothing in `ArchentMemory` rewinds it, and `memory_destroy` frees it at shutdown. Transient is per-frame scratch. `memory_frame_reset` clears it, so call that once a frame.

```c
/* at startup */
u32 *ids = arena_array_m(&memory.permanent, u32, 1024);

/* every frame */
memory_frame_reset(&memory);
f32 *samples = arena_array_m(&memory.transient, f32, 4096);
```

`TempMemory` is a scoped rewind. It works on any arena, not only the transient one:

```c
TempMemory temp = temp_memory_begin(&memory.transient);
u32 *scratch = arena_array_m(&memory.transient, u32, 512);
/* ... */
temp_memory_end(temp);
```

Scopes can nest as long as they end in reverse order. A `TempMemory` is an arena pointer and an `ArenaMark`. Each arena counts its open scopes. In builds with asserts, `temp_memory_end` checks there is one to close and `arena_clear` (so `memory_frame_reset`) checks that none are left open.

## Random numbers

`archent_rng.h`

The rng module has no global state. The same inputs always give the same result, and drawing for one set of keys never affects another. The inputs are a `seed`, two 64-bit keys `a` and `b` that the caller picks, and an `RngSalt` that keeps different uses of the same keys apart. Only `RNG_SALT_NONE` exists so far.

The two functions that call `logf` or `expf` (`rng_exponential` and `rng_poisson_small`) can give slightly different results between math libraries. The rest is integer math and basic float arithmetic.

A hash is a `u64`:

```c
u64 h = rng_hash(seed, a, b, RNG_SALT_NONE);
```

`rng_splitmix64` is the mixing step underneath, and `rng_hash` chains four of them. To get more than one value for the same keys, use a stream or index into one:

```c
RngStream s = rng_stream_begin(seed, a, b, RNG_SALT_NONE);
u64 first  = rng_stream_next(&s);
u64 second = rng_stream_next(&s);

u64 nth = rng_stream_at(seed, a, b, RNG_SALT_NONE, n);   /* no stream state needed */
```

`rng_stream_at` is a different generator from `rng_stream_next`. Don't expect it to return the same sequence.

### Turning a hash into a value

| Function | Result |
|---|---|
| `rng_f32(h)` | `f32` in [0, 1) |
| `rng_range_zero_u32(h, n)` | `u32` in [0, n) |
| `rng_range_u32(h, lo, hi)` | `u32` in [lo, hi) |
| `rng_range_i32(h, lo, hi)` | `i32` in [lo, hi) |
| `rng_range_f32(h, lo, hi)` | `f32` in [lo, hi) |
| `rng_bool(h)` | `true` or `false` |
| `rng_sign(h)` | -1 or +1 |
| `rng_chance(h, p)` | `true` with probability `p` |

If `hi <= lo` the range functions return `lo`. `rng_range_zero_u32` multiplies instead of taking a remainder and has no rejection step, so it's very slightly biased for large `n` (at most `n / 2^32`).

### One hash, one value

A hash has 64 bits and each kind of value reads its own part. `rng_f32` (and `rng_chance` and `rng_range_f32` on top of it) reads the top 24. `rng_range_zero_u32` (and the integer range functions on top of it) reads the low 32. `rng_bool` reads bit 32 and `rng_sign` bit 33. One hash can feed one value of each kind, so `rng_f32(h)` and `rng_range_u32(h, 0, 10)` are independent. Two values of the same kind from one hash are the same draw, so take a new hash from a stream for the second one.

### Distributions

- `rng_normal_fast(hash1, hash2, hash3)` is a rough normal with mean 0 and variance 1, made from three uniform values. It never leaves the range -3 to 3 and its tails are lighter than a real normal's. `rng_normal_evt(seed, a, b, salt)` takes the three hashes from a stream.
- `rng_exponential(h, lambda)` is an exponential with rate `lambda`, which has to be above 0. That's asserted. Without asserts it's clamped to 0.01.
- `rng_poisson_small(seed, a, b, salt, lambda)` is a Poisson count using Knuth's method, for `lambda` below 10. That's asserted too. Without asserts `lambda` is clamped to the range 0.01 to 9.99.

### Weighted picks

`rng_alias_build` makes an alias table, so picking by weight costs the same however many items there are:

```c
f32 weights[4] = { 1.0f, 3.0f, 0.0f, 6.0f };
f32 prob[4];
u32 alias[4];
u32 small_stack[4], large_stack[4];
RngAliasTable table;

rng_alias_build(weights, 4, prob, alias, small_stack, large_stack, &table);
u32 pick = rng_alias_pick(&table, rng_hash(seed, a, b, RNG_SALT_NONE));
```

All the arrays belong to the caller. `prob` and `alias` need `n` entries each and have to stay alive as long as the table is used. The two stacks are scratch space and need `n` entries each. An item with weight 0 is never picked. If every weight is 0 the table is uniform. `rng_alias_pick` needs `n > 0`, and one hash is enough for it.

### Shuffling

`rng_shuffle(seed, a, b, salt, arr, n)` runs a Fisher-Yates shuffle over an array of `u32`. The same keys give the same order.

## ECS

`archent_ecs.h` and `archent_ecs.c` are empty. The header includes the arena header, since that's where the ECS storage will come from.

## Platform layer

`platform_communication.h` declares what the engine and the platform layer share. For now that's `ArchentState`, which has one field (`running`), and an `extern` instance called `archent_state`. Each platform layer defines that instance. The Windows one does it in `windows_archent.c`, next to the empty `WinMain`.
