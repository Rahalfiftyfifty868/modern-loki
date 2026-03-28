# Modern Loki

[![CI](https://github.com/skillman1337/modern-loki/actions/workflows/ci.yml/badge.svg)](https://github.com/skillman1337/modern-loki/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)

C++20 port of Andrei Alexandrescu's Loki library from *Modern C++ Design*.

Header-only. Zero dependencies. Optional C++20 named module. Targets MSVC 2022 (v143), GCC 12+, Clang 15+.

This is primarily a **teaching and reference library**. It demonstrates how the policy-based design patterns from the book translate into modern C++20 idioms (concepts, variadic templates, `std::variant`, standard threading). For production code, prefer `std::unique_ptr`/`std::shared_ptr` over `smart_ptr`, `std::flat_map` (C++23) over `assoc_vector`, and mature concurrency libraries over the threading policies. Use Modern Loki when you want to study the patterns, teach generic programming, or need a policy-based component that the standard library doesn't provide (e.g. abstract factories, multi-methods, small object allocators).

## What's Modernized

| Original Loki | Modern Loki | C++20 Feature Used |
|---|---|---|
| Recursive `Typelist<H, T>` | Variadic `typelist<Ts...>` | Parameter packs, fold expressions, `consteval` |
| `SingletonHolder` with OS locks | `singleton_holder` with DCLP | `std::atomic`, policy-based `ThreadingModel` |
| Manual `SmartPtr` policies | `smart_ptr` with move semantics | Move constructors, `operator<=>` |
| `Functor` with virtual dispatch | `functor` wrapping `std::function` | `std::function`, `std::invoke`, concepts |
| — | `functor_ref` (non-owning) | C++20 polyfill for `std::function_ref` (P0792) |
| `AbstractFactory` with virtual MI | Tuple-based `abstract_factory` | `std::tuple`, fold expressions, concepts |
| `Factory` with raw pointers | `factory` with `std::unique_ptr` | `std::unique_ptr`, `std::function` |
| Acyclic Visitor with macros | CRTP `visitable<Derived>` | `std::variant`, `overloaded` pattern |
| `AssocVector` | `assoc_vector` flat-map polyfill | `std::input_iterator` concept, `operator<=>`, heterogeneous lookup |
| `SmallObject` with C casts + O(N) alloc | `small_object` with O(log C) dealloc | `std::byte`, `std::memcpy`, RAII chunks, binary-searched chunk index |
| Windows-only threading (copy-pasted) | Portable threading (CRTP DRY) | `std::mutex`, `std::shared_mutex`, `std::atomic` |
| `GenScatterHierarchy` (recursive) | Flat variadic inheritance | `public Unit<Ts>...` pack expansion |
| `Tuple` (custom) | Deleted — use `std::tuple` | — |

## Requirements

- **Compiler**: MSVC 2022 (17.x, `/std:c++20`), GCC 12+, or Clang 15+
- **CMake**: 3.20+
- **No external dependencies** (tests use bundled [doctest](https://github.com/doctest/doctest))

## Build

**MSVC** (Developer Command Prompt or any terminal with `cl.exe` on PATH):

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

**GCC**:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-13
cmake --build build
```

**Clang**:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++-17
cmake --build build
```

### C++20 Named Module (optional)

Modern Loki ships a named module interface (`loki.cppm`) so consumers can `import loki;` instead of `#include <loki/loki.hpp>`. Requires CMake 3.28+. Append `-DLOKI_BUILD_MODULE=ON` to any of the configure commands above:

```
cmake -B build -G "Visual Studio 17 2022" -A x64 -DLOKI_BUILD_MODULE=ON
cmake --build build --config Release
```

Link against the `modern_loki_module` target and use `import loki;` in your source files.

## Run Tests

```
cd build
ctest -C Release --output-on-failure
```

Or run the test binary directly:

```
build/tests/Release/loki_tests.exe          # MSVC
build/tests/loki_tests                      # GCC / Clang
```

## Benchmarks

The small object allocator includes a benchmark comparing `loki::small_obj_allocator` against global `new`/`delete` and `std::pmr::monotonic_buffer_resource` across allocation sizes from 8 to 256 bytes:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DLOKI_BUILD_BENCH=ON
cmake --build build --config Release
build/bench/Release/bench_small_obj.exe     # MSVC
build/bench/bench_small_obj                 # GCC / Clang
```

The benchmark performs 100,000 allocations and deallocations per size in random order over 10 rounds and reports median nanoseconds per operation.

## Documentation

Each header has a companion doc with API overview and 10 compiled, tested examples.

| Header | Doc | Description |
|--------|-----|-------------|
| `loki/loki.hpp` | [loki.md](docs/loki.md) | Umbrella header — cross-component examples |
| `loki/typelist.hpp` | [typelist.md](docs/typelist.md) | Variadic typelist and `tl::` metafunctions |
| `loki/type_traits.hpp` | [type_traits.md](docs/type_traits.md) | `type_traits<T>`, `type2type`, `conversion` |
| `loki/threads.hpp` | [threads.md](docs/threads.md) | Threading policies |
| `loki/singleton.hpp` | [singleton.md](docs/singleton.md) | Policy-based singletons |
| `loki/smart_ptr.hpp` | [smart_ptr.md](docs/smart_ptr.md) | Policy-based smart pointers |
| `loki/functor.hpp` | [functor.md](docs/functor.md) | Type-erased callables, composition |
| `loki/factory.hpp` | [factory.md](docs/factory.md) | Generic factory, clone factory |
| `loki/abstract_factory.hpp` | [abstract_factory.md](docs/abstract_factory.md) | Tuple-based abstract factory |
| `loki/visitor.hpp` | [visitor.md](docs/visitor.md) | Acyclic, cyclic, and variant visitors |
| `loki/multi_methods.hpp` | [multi_methods.md](docs/multi_methods.md) | Static, dynamic, and variant double dispatch |
| `loki/small_obj.hpp` | [small_obj.md](docs/small_obj.md) | Small object allocator |
| `loki/assoc_vector.hpp` | [assoc_vector.md](docs/assoc_vector.md) | Sorted flat map |
| `loki/hierarchy_generators.hpp` | [hierarchy_generators.md](docs/hierarchy_generators.md) | Scatter/linear hierarchy generators |
| `loki/loki.cppm` | — | C++20 named module interface (`import loki;`) |

## Design Decisions

**Kept**: Policy-based `smart_ptr`, `singleton_holder`, `abstract_factory`, `small_object`, `assoc_vector`, hierarchy generators — these demonstrate design patterns that `std::` doesn't cover. The policy architecture is Loki's core contribution and remains relevant.

**Merged**: `type_manip.hpp` (`type2type`, `conversion`) folded into `type_traits.hpp`.

**Deleted**: `loki::tuple` (replaced by `std::tuple`), `type_manip.hpp` (merged into `type_traits.hpp`), all custom type-trait aliases that were 1:1 wrappers for `std::` equivalents, OS-specific threading primitives.

**Threading model**: All four policies (`single_threaded`, `object_level_lockable`, `class_level_lockable`, `rw_lockable`) expose a uniform API via CRTP bases (`detail::plain_atomic_ops`, `detail::std_atomic_ops`) — zero duplication, fully substitutable.

**Multi-methods**: `variant_dispatch` provides O(1) compile-time double dispatch via `std::visit` for `std::variant` types, complementing the existing `static_dispatcher` (RTTI-based) and `basic_dispatcher`/`fn_dispatcher` (dynamic registration).

**Smart pointers**: `ref_counted` uses `std::atomic<unsigned int>` for thread-safe reference counting out of the box. Base class order places ownership before storage, enabling single-phase copy construction (clone result initializes storage directly, no default-construct-then-overwrite).

**Abstract factory**: Uses `std::tuple<std::function<...>...>` for compile-time product dispatch. Zero virtual inheritance, zero `dynamic_cast`, zero `void*`. Creators are frozen after construction.

**`assoc_vector` heterogeneous lookup**: When the comparator defines an `is_transparent` tag (e.g. `std::less<>`), `find`, `contains`, `count`, `lower_bound`, `upper_bound`, `equal_range`, and `erase` accept any key type comparable via the comparator, avoiding temporary key construction.

**Small object allocator**: `fixed_allocator::deallocate` uses a sorted vector of chunk start pointers for O(log C) binary-searched chunk ownership lookup, with a last-used chunk cache as a fast path. The previous O(N) linear scan is eliminated.

**C++20 modules**: A named module interface (`loki.cppm`) wraps all headers via `export extern "C++"`, with standard library headers pre-included in the global module fragment. The header-only `#pragma once` path remains the default; the module is opt-in via `-DLOKI_BUILD_MODULE=ON`.

## License

[MIT](LICENSE). The original Loki library was released under the MIT License by Andrei Alexandrescu.
