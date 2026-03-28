#pragma once

#include "typelist.hpp"

#include <cassert>
#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace loki {

// Modern Functor: wraps any callable with type-erasure.
// The original Loki::Functor used complex recursive typelists and manual
// virtual dispatch. C++20 gives us std::function, std::invoke, concepts,
// and variadic templates, so the implementation is vastly simpler.

// ─── Functor concept ────────────────────────────────────────────────────────

template <typename F, typename R, typename... Args>
concept Callable = std::invocable<F, Args...> &&
    (std::same_as<R, void> || std::convertible_to<std::invoke_result_t<F, Args...>, R>);

// ─── Core Functor class ─────────────────────────────────────────────────────
// Args follow std::function semantics: value types are copied per call.
// Use const T& in the signature to avoid copies for large types.

template <typename R, typename... Args>
class functor {
public:
    using result_type = R;
    using args_typelist = typelist<Args...>;

    functor() = default;

    template <Callable<R, Args...> F>
    functor(F f) : impl_(std::move(f)) {}

    functor(R (*fp)(Args...)) : impl_(fp) {}

    // Member function pointer + object
    template <typename Obj, typename MemFn>
        requires std::is_member_function_pointer_v<MemFn>
    functor(Obj&& obj, MemFn mfp)
        : impl_([o = std::forward<Obj>(obj), mfp](Args... args) mutable -> R {
            return std::invoke(mfp, o, std::forward<Args>(args)...);
        }) {}

    R operator()(Args... args) const {
        assert(impl_ && "calling empty functor");
        return impl_(std::forward<Args>(args)...);
    }

    explicit operator bool() const { return static_cast<bool>(impl_); }

    void swap(functor& rhs) noexcept { impl_.swap(rhs.impl_); }

private:
    std::function<R(Args...)> impl_;
};

// ─── functor_ref: non-owning, non-allocating callable reference ──────────────
// std::function owns and heap-allocates. This doesn't.
// C++26 adds std::function_ref; this is the C++20 polyfill.

template <typename R, typename... Args>
class functor_ref {
    void* obj_ = nullptr;
    R (*invoke_)(void*, Args...) = nullptr;

public:
    constexpr functor_ref() noexcept = default;

    // Per P0792: block rvalue references for non-trivially-copyable types
    // (they dangle). Function pointers and trivial callables are safe.
    template <typename F>
        requires std::invocable<F&, Args...> &&
                 (!std::same_as<std::remove_cvref_t<F>, functor_ref>) &&
                 (!std::is_trivially_copyable_v<std::remove_cvref_t<F>>)
    constexpr functor_ref(F&&) noexcept
        requires std::is_rvalue_reference_v<F&&> = delete;

    template <typename F>
        requires std::invocable<F&, Args...> &&
                 (!std::same_as<std::remove_cvref_t<F>, functor_ref>)
    constexpr functor_ref(F& f) noexcept
        : obj_(reinterpret_cast<void*>(std::addressof(f)))
        , invoke_([](void* p, Args... args) -> R {
            return (*reinterpret_cast<std::add_pointer_t<F>>(p))(
                std::forward<Args>(args)...);
        }) {}

    R operator()(Args... args) const {
        assert(invoke_ && "calling empty functor_ref");
        return invoke_(obj_, std::forward<Args>(args)...);
    }

    constexpr explicit operator bool() const noexcept { return invoke_ != nullptr; }

    constexpr void swap(functor_ref& rhs) noexcept {
        std::swap(obj_, rhs.obj_);
        std::swap(invoke_, rhs.invoke_);
    }
};

// ─── Bind helpers ────────────────────────────────────────────────────────────

// bind_first: bind the first argument of a functor
template <typename R, typename A1, typename... Rest>
auto bind_first(const functor<R, A1, Rest...>& f, A1 bound) {
    return functor<R, Rest...>(
        [f, bound = std::move(bound)](Rest... args) -> R {
            return f(bound, std::forward<Rest>(args)...);
        });
}

// chain: compose two functors f(g(x))
// R2 (return of g) must be convertible to the parameter of f.
template <typename R1, typename R2, typename... Args>
auto chain(const functor<R1, R2>& f, const functor<R2, Args...>& g) {
    return functor<R1, Args...>(
        [f, g](Args... args) -> R1 {
            return f(g(std::forward<Args>(args)...));
        });
}

// ─── make_functor helpers ────────────────────────────────────────────────────

template <typename R, typename... Args>
auto make_functor(R (*fp)(Args...)) {
    return functor<R, Args...>(fp);
}

template <typename R, typename... Args>
auto make_functor(std::function<R(Args...)> f) {
    return functor<R, Args...>(std::move(f));
}

template <typename F>
    requires (!std::is_pointer_v<std::remove_cvref_t<F>>)
auto make_functor(F&& f) {
    return make_functor(std::function(std::forward<F>(f)));
}

template <typename Obj, typename R, typename... Args>
auto make_functor(Obj&& obj, R (std::remove_reference_t<Obj>::*mfp)(Args...)) {
    return functor<R, Args...>(std::forward<Obj>(obj), mfp);
}

template <typename Obj, typename R, typename... Args>
auto make_functor(Obj&& obj, R (std::remove_reference_t<Obj>::*mfp)(Args...) const) {
    return functor<R, Args...>(std::forward<Obj>(obj), mfp);
}

} // namespace loki
