#pragma once

#include "typelist.hpp"

namespace loki {

// Modern Hierarchy Generators.
// The original Loki used GenScatterHierarchy and GenLinearHierarchy with
// recursive typelist traversal. Modern C++20 replaces Tuple with std::tuple.
// We provide scatter/linear hierarchy generators for advanced use cases
// (e.g. AbstractFactory patterns) that need true inheritance.

// ─── GenScatterHierarchy ─────────────────────────────────────────────────────
// O(1) flat variadic: directly inherits from Unit<T> for each T in the pack.

template <typename TList, template <typename> class Unit>
class gen_scatter_hierarchy;

template <typename... Ts, template <typename> class Unit>
class gen_scatter_hierarchy<typelist<Ts...>, Unit>
    : public Unit<Ts>...
{
    static_assert(std::is_same_v<typelist<Ts...>, tl::no_duplicates_t<typelist<Ts...>>>,
        "gen_scatter_hierarchy: duplicate types in typelist cause ambiguous base classes");

public:
    using type_list = typelist<Ts...>;
};

// Field access by type
template <typename T, typename TList, template <typename> class Unit>
Unit<T>& field(gen_scatter_hierarchy<TList, Unit>& obj) {
    return static_cast<Unit<T>&>(obj);
}

template <typename T, typename TList, template <typename> class Unit>
const Unit<T>& field(const gen_scatter_hierarchy<TList, Unit>& obj) {
    return static_cast<const Unit<T>&>(obj);
}

// ─── TupleUnit ───────────────────────────────────────────────────────────────

template <typename T>
struct tuple_unit {
    T value{};
    operator T&() { return value; }
    operator const T&() const { return value; }
};

// ─── GenLinearHierarchy ──────────────────────────────────────────────────────
// Generates a linear chain of inheritance using a two-arg template Unit.

template <typename TList, template <typename, typename> class Unit, typename Root = empty_type>
class gen_linear_hierarchy;

template <template <typename, typename> class Unit, typename Root>
class gen_linear_hierarchy<typelist<>, Unit, Root> : public Root {};

template <typename Head, typename... Tail,
          template <typename, typename> class Unit, typename Root>
class gen_linear_hierarchy<typelist<Head, Tail...>, Unit, Root>
    : public Unit<Head, gen_linear_hierarchy<typelist<Tail...>, Unit, Root>>
{};

// ─── apply_to_all: invoke a callable for each type in a pack ─────────────────

template <typename F, typename... Ts>
void apply_to_all(F&& f, typelist<Ts...>) {
    (f.template operator()<Ts>(), ...);
}

} // namespace loki
