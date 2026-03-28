#pragma once

#include "typelist.hpp"

#include <type_traits>
#include <variant>

namespace loki {

// Modern Visitor using C++20 concepts and std::variant support.
// The original Loki Visitor used recursive typelist-based multiple inheritance
// and RTTI (dynamic_cast). Modern C++ offers variant-based visitation and
// concept-constrained designs as alternatives.

// ─── Classic Acyclic Visitor (modernized) ────────────────────────────────────

class base_visitor {
public:
    virtual ~base_visitor() = default;
};

template <typename T, typename R = void>
class visitor {
public:
    using return_type = R;
    virtual R visit(T&) = 0;
    virtual ~visitor() = default;
};

// Variadic visitor: inherit from visitor<T> for each T in the pack
template <typename R, typename... Ts>
class multi_visitor : public visitor<Ts, R>... {
    static_assert(std::is_same_v<typelist<Ts...>, tl::no_duplicates_t<typelist<Ts...>>>,
        "multi_visitor: duplicate types in parameter pack cause ambiguous bases");
public:
    using return_type = R;
    using visitor<Ts, R>::visit...;
};

// BaseVisitable
template <typename R = void>
class base_visitable {
public:
    using return_type = R;
    virtual ~base_visitable() = default;
    virtual R accept(base_visitor&) = 0;

protected:
    template <typename T>
    static R accept_impl(T& visited, base_visitor& guest) {
        if (auto* p = dynamic_cast<visitor<T, R>*>(&guest)) {
            return p->visit(visited);
        }
        if constexpr (std::is_void_v<R>) {
            return;
        } else {
            return R{};
        }
    }
};

// Macro-free visitable: CRTP helper instead of DEFINE_VISITABLE macro
template <typename Derived, typename R = void>
class visitable : public base_visitable<R> {
public:
    R accept(base_visitor& guest) override {
        static_assert(std::is_base_of_v<visitable, Derived>,
            "visitable<Derived>: Derived must inherit from visitable<Derived> (CRTP)");
        return base_visitable<R>::accept_impl(
            static_cast<Derived&>(*this), guest);
    }
};

// ─── Cyclic Visitor ──────────────────────────────────────────────────────────

template <typename R, typename... Ts>
class cyclic_visitor : public visitor<Ts, R>... {
    static_assert(std::is_same_v<typelist<Ts...>, tl::no_duplicates_t<typelist<Ts...>>>,
        "cyclic_visitor: duplicate types in parameter pack cause ambiguous bases");
public:
    using return_type = R;
    using visitor<Ts, R>::visit...;
    ~cyclic_visitor() override = default;

    template <typename T>
        requires (std::is_same_v<T, Ts> || ...)
    R generic_visit(T& host) {
        return static_cast<visitor<T, R>*>(this)->visit(host);
    }
};

// ─── std::variant-based Visitor (modern idiom) ───────────────────────────────

// overloaded: the classic C++17/20 pattern for variant visitation
template <typename... Fs>
struct overloaded : Fs... {
    using Fs::operator()...;
};

// Deduction guide
template <typename... Fs>
overloaded(Fs...) -> overloaded<Fs...>;

// variant_visitor: visit a variant with multiple overloaded lambdas
template <typename Variant, typename... Visitors>
constexpr decltype(auto) variant_visit(Variant&& var, Visitors&&... visitors) {
    return std::visit(
        overloaded{std::forward<Visitors>(visitors)...},
        std::forward<Variant>(var));
}

// Concept for types that can be visited
template <typename T>
concept Visitable = requires(T t, base_visitor& v) {
    { t.accept(v) };
};

} // namespace loki
