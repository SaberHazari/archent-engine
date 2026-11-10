# Archent

Archent is a game engine written in C17, built around an entity component system. It's early. What exists so far is a memory system, a random number module and the start of a Windows platform layer. The ECS, input, rendering and a way to hook a game into the engine aren't written yet.

## How it fits together

The engine is split in two. The core, in `src/core/`, is the engine itself. It doesn't know which operating system it's running on, and nothing in it includes a platform header. The platform layer, in `src/platforms/<os>/`, is everything that does know: the entry point, input, performance and the software renderer. Only `windows` exists so far.

The two sides share declarations in `src/platform_communication/`. For now that's `ArchentState`, a struct with one field (`running`), and a global `archent_state` that the platform layer defines. Nothing reads or writes it yet.

```
src/
  archent_utils.h                 types and size macros, used by everything
  core/                           the engine
    archent_rng.h, .c             random numbers
    archent_ecs.h, .c             entity component system (empty)
    archent_memory.h              memory, internal
    archent_arena.h, .c           memory, internal
  platform_communication/         declarations shared by the core and the platform layer
  platforms/
    windows/                      WinMain, input, performance, software renderer
```

Inside the engine, memory comes from two arenas instead of a `malloc` per object. One is permanent and lasts as long as the engine runs. The other is transient and meant to be cleared once a frame. You don't allocate from either, so they aren't documented below.

Not written yet: the ECS (`archent_ecs.h` and `.c` are empty), input, performance code and the software renderer (`windows_input_system`, `windows_performance` and `windows_software_renderer` are empty), and an entry point for game code (`WinMain` returns immediately). There's no Linux platform layer. The Makefile has Linux targets and looks for sources in `src/platforms/linux/`, but that directory doesn't exist.

## Building

The Makefile uses `find`, `mkdir -p` and `rm -rf`, so you need GNU make and a Unix-style shell. Windows builds use MinGW-w64 (`x86_64-w64-mingw32-gcc`), and the `run` targets start the exe with `wine`. Linux builds use `gcc-13`.

```
make                   debug build for Windows, same as make win-debug
make win-release       release build for Windows
make run               debug build, then run it under wine
make win-release-run   release build, then run it under wine
make clean             delete the build output for both platforms
```

The executable ends up in `build/debug/windows/archent-engine.exe` or `build/release/windows/archent-engine.exe`. Everything is compiled with `-Isrc` and the headers rely on it, so include them as `core/archent_rng.h` and `archent_utils.h`.

Debug builds use `-O0 -g` and the undefined behavior sanitizer. On Windows the sanitizer is in trap mode, so undefined behavior crashes the program without printing anything. Release builds use `-O2 -DNDEBUG`, which removes the asserts.

The `linux-*` targets are there, but they can't produce a binary yet. Without a Linux platform layer the link fails for lack of a `main`.

## Conventions

The code is C17 and is compiled with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes -Wmissing-prototypes`.

- Functions start with the name of their module: `rng_hash`, `rng_alias_build`. Types are PascalCase (`RngStream`). Function-like macros end in `_m` (`kilo_bytes_m`). Constants and enum values are upper case (`RNG_SALT_NONE`).
- A name that ends in an underscore is internal. Don't call it, and expect it to change.
- Some requirements on arguments are checked with an `assert`, and the reference below says which. Nothing returns an error code. Release builds define `NDEBUG`, which removes the asserts.
- The headers in `core/` can be included from C++.

## Reference

### Types and macros

`archent_utils.h`

```
i8    i16   i32   i64     signed integers, 8 to 64 bits
u8    u16   u32   u64     unsigned integers, 8 to 64 bits
f32   f64                 float and double
usize uptr                size_t and uintptr_t
```

`bool`, `true` and `false` come from `<stdbool.h>`, which the header includes.

```c
u64 kilo_bytes_m(value);
u64 mega_bytes_m(value);
u64 giga_bytes_m(value);
```

Each one takes a number and gives back that many KiB, MiB or GiB as a byte count in a `u64`. The argument is cast to `u64` before it's multiplied and is evaluated once. `mega_bytes_m(256)` is 268435456.

### Random numbers

`core/archent_rng.h`, plus `core/archent_rng.c` for `rng_alias_build` and `rng_shuffle`

The rng module keeps no state. Give it the same inputs and you get the same number back, every run, so you can reproduce a run. A number drawn for one entity also doesn't change what any other entity gets.

Everything that starts from scratch takes the same four inputs:

- `seed`, a `u64` for the whole run.
- `a` and `b`, two `u64` keys that you pick. An entity id and a tick number would do.
- `salt`, an `RngSalt` that keeps different uses of the same keys apart. Give the damage roll and the loot roll different salts and they won't get the same number for the same entity on the same tick. Only `RNG_SALT_NONE` exists so far. The salt is mixed in as a plain number, so a cast integer works too.

The functions use no global state and don't allocate, so threads can call them as long as they don't share a stream, a table or an array. `rng_exponential` calls `logf` and `rng_poisson_small` calls `expf`, so their results can differ slightly between math libraries. Everything else is integer math and basic float arithmetic.

#### Hashes and streams

```c
u64 rng_hash(u64 seed, u64 a, u64 b, RngSalt salt);
u64 rng_splitmix64(u64 num);
```

`rng_hash` takes the four inputs and returns a 64-bit hash. Most of the module starts from one. `rng_splitmix64` is the mixing step that `rng_hash` chains four times. It takes any `u64` and returns a well mixed one, and you'd call it yourself only to hash an integer of your own.

```c
RngStream rng_stream_begin(u64 seed, u64 a, u64 b, RngSalt salt);
u64       rng_stream_next(RngStream *stream);
u64       rng_stream_at(u64 seed, u64 a, u64 b, RngSalt salt, u64 i);
```

When one hash isn't enough, use a stream. `rng_stream_begin` returns one, and every `rng_stream_next` advances it and returns the next hash. An `RngStream` is a single `u64` that you shouldn't touch. Copying it copies its position.

`rng_stream_at` gives you value number `i` for a set of keys without keeping a stream, so you can jump to any position. `i` can be any `u64`. It isn't the same sequence that `rng_stream_next` gives, and `rng_stream_at(seed, a, b, salt, 0)` is the same as `rng_hash(seed, a, b, salt)`.

#### Turning a hash into a value

```c
f32  rng_f32(u64 hash);
u32  rng_range_zero_u32(u64 hash, u32 n);
u32  rng_range_u32(u64 hash, u32 lo, u32 hi);
i32  rng_range_i32(u64 hash, i32 lo, i32 hi);
f32  rng_range_f32(u64 hash, f32 lo, f32 hi);
bool rng_bool(u64 hash);
i32  rng_sign(u64 hash);
bool rng_chance(u64 hash, f32 prob);
```

All of them take a hash from `rng_hash`, a stream or `rng_stream_at`.

| Function | Returns |
|---|---|
| `rng_f32` | an `f32` from 0 up to, but not including, 1 |
| `rng_range_zero_u32` | a `u32` from 0 to `n - 1` |
| `rng_range_u32` | a `u32` from `lo` to `hi - 1` |
| `rng_range_i32` | an `i32` from `lo` to `hi - 1` |
| `rng_range_f32` | an `f32` from `lo` up to, but not including, `hi` |
| `rng_bool` | `true` or `false`, each half the time |
| `rng_sign` | -1 or 1, each half the time |
| `rng_chance` | `true` with probability `prob` |

If `hi <= lo` the range functions return `lo`, and `rng_range_zero_u32(hash, 0)` returns 0. `rng_chance` is never true for a `prob` of 0 or less and always true for 1 or more.

The integer ranges multiply instead of taking a remainder, with no rejection step. That makes some values come up very slightly more often than others. The bias is at most `n / 2^32` for a range of `n` values, so it doesn't matter for ranges of a few thousand.

#### Using a hash more than once

A hash has 64 bits and each kind of value reads its own part. `rng_f32`, `rng_chance` and `rng_range_f32` read the top 24. The integer ranges read the low 32. `rng_bool` reads bit 32 and `rng_sign` bit 33. So one hash can give you one value of each kind, and `rng_f32(h)` and `rng_range_u32(h, 0, 10)` are independent. Two values of the same kind from the same hash are the same draw. For a second one, take another hash from a stream.

```c
u64 h = rng_hash(world_seed, entity_id, tick, RNG_SALT_NONE);
u32 damage = rng_range_u32(h, 5, 11);    /* 5 to 10 */
bool crit = rng_chance(h, 0.1f);         /* reads other bits of h, so it's independent of damage */
```

#### Distributions

```c
f32 rng_normal_fast(u64 hash1, u64 hash2, u64 hash3);
f32 rng_normal_evt(u64 seed, u64 a, u64 b, RngSalt salt);
f32 rng_exponential(u64 hash, f32 lambda);
u32 rng_poisson_small(u64 seed, u64 a, u64 b, RngSalt salt, f32 lambda);
```

`rng_normal_fast` returns a rough normal with mean 0 and variance 1. It adds three uniform values, so it never leaves the range -3 to 3 and its tails are lighter than a real normal's. Pass it three different hashes, from a stream for example. Pass the same one three times and you get a uniform number between -3 and 3. `rng_normal_evt` does the three draws for you, from a stream started with the keys you give it.

`rng_exponential` returns a value of 0 or more from an exponential distribution with rate `lambda`, so the mean is `1 / lambda`. The largest value it can return is about `16.6 / lambda`. `lambda` has to be above 0. That's asserted, and without asserts it's clamped to 0.01.

`rng_poisson_small` returns a Poisson count with mean `lambda`. It uses Knuth's method, which only suits small `lambda`: it has to be above 0 and below 10. That's asserted too, and without asserts `lambda` is clamped to the range 0.01 to 9.99.

#### Weighted picks

```c
typedef struct RngAliasTable {
    u32 n;
    f32 *prob;
    u32 *alias;
} RngAliasTable;

void rng_alias_build(const f32 *weights, u32 n, f32 *prob, u32 *alias,
                     u32 *small_stack, u32 *large_stack, RngAliasTable *out);
u32  rng_alias_pick(const RngAliasTable *table, u64 hash);
```

`rng_alias_build` takes `n` weights and fills in `out`. After that, `rng_alias_pick` takes the table and a hash and returns an index from 0 to `n - 1`, chosen in proportion to the weights. A pick takes the same time however large `n` is. Building takes time proportional to `n`.

```c
f32 weights[4] = { 1.0f, 3.0f, 0.0f, 6.0f };
f32 prob[4];
u32 alias[4];
u32 small_stack[4], large_stack[4];
RngAliasTable table;

rng_alias_build(weights, 4, prob, alias, small_stack, large_stack, &table);
u32 pick = rng_alias_pick(&table, rng_hash(world_seed, entity_id, tick, RNG_SALT_NONE));
```

The arrays are yours. `prob` and `alias` need room for `n` entries each and have to stay alive as long as you use the table, because the table points at them. `small_stack` and `large_stack` are scratch space for the build, `n` entries each, and you can throw them away afterwards. `weights` isn't changed. Weights must not be negative. An item with weight 0 is never picked, and if every weight is 0 the table is uniform.

`rng_alias_pick` needs a table with at least one item. That's asserted. It uses both the integer and the float part of the hash, so don't use the same hash for another range or float value.

#### Shuffling

```c
void rng_shuffle(u64 seed, u64 a, u64 b, RngSalt salt, u32 *arr, u32 n);
```

Shuffles the `n` elements of `arr` in place (Fisher-Yates). The same keys and `n` always give the same shuffle. It returns nothing, and leaves the array alone when `n` is 0 or 1. It only works on `u32` arrays, so to shuffle something else, shuffle an array of indices and look things up through it.
