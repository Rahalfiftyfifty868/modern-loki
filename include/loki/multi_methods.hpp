#pragma once

#include "typelist.hpp"
#include "type_traits.hpp"

#include <functional>
#include <map>
#include <stdexcept>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <variant>

namespace loki {

// Modern Multi-Methods (Double Dispatch).
// The original Loki used recursive typelist-based static dispatchers and
// an AssocVector for dynamic dispatch. Modern C++20 uses std::type_index,
// std::map, std::function, and concepts.

// ─── Static Dispatcher ──────────────────────────────────────────────────────

namespace detail {

template <typename Executor, typename BaseLhs, typename BaseRhs, typename R,
          typename LhsTypes, typename RhsTypes, bool symmetric>
struct static_dispatch_impl;

// Base case: empty LHS typelist
template <typename Executor, typename BaseLhs, typename BaseRhs, typename R,
          typename RhsTypes, bool symmetric>
struct static_dispatch_impl<Executor, BaseLhs, BaseRhs, R,
                            typelist<>, RhsTypes, symmetric> {
    static R go(BaseLhs&, BaseRhs&, Executor& exec) {
        return exec.on_error();
    }
};

// Recursive dispatch over RHS for a given LHS type
template <typename SomeLhs, typename Executor, typename BaseRhs, typename R,
          typename RhsTypeList, typename LhsTypes, bool symmetric>
struct dispatch_rhs;

template <typename SomeLhs, typename Executor, typename BaseRhs, typename R,
          typename LhsTypes, bool symmetric>
struct dispatch_rhs<SomeLhs, Executor, BaseRhs, R,
                    typelist<>, LhsTypes, symmetric> {
    static R go(SomeLhs&, BaseRhs&, Executor& exec) {
        return exec.on_error();
    }
};

template <typename SomeLhs, typename Executor, typename BaseRhs, typename R,
          typename Head, typename... Tail, typename LhsTypes, bool symmetric>
struct dispatch_rhs<SomeLhs, Executor, BaseRhs, R,
                    typelist<Head, Tail...>, LhsTypes, symmetric> {
private:
    template <bool Swap>
    static R do_fire_impl(SomeLhs& lhs, Head& rhs, Executor& exec) {
        if constexpr (Swap) {
            return exec.fire(rhs, lhs);
        } else {
            return exec.fire(lhs, rhs);
        }
    }
public:
    static R go(SomeLhs& lhs, BaseRhs& rhs, Executor& exec) {
        if (auto* p = dynamic_cast<Head*>(&rhs)) {
            constexpr bool swap_args = symmetric &&
                (tl::index_of_v<LhsTypes, SomeLhs> >
                 tl::index_of_v<LhsTypes, Head>);
            return do_fire_impl<swap_args>(lhs, *p, exec);
        }
        return dispatch_rhs<SomeLhs, Executor, BaseRhs, R,
                            typelist<Tail...>, LhsTypes, symmetric>::go(lhs, rhs, exec);
    }
};

template <typename Executor, typename BaseLhs, typename BaseRhs, typename R,
          typename Head, typename... LhsTail, typename RhsTypes, bool symmetric>
struct static_dispatch_impl<Executor, BaseLhs, BaseRhs, R,
                            typelist<Head, LhsTail...>, RhsTypes, symmetric> {
    static R go(BaseLhs& lhs, BaseRhs& rhs, Executor& exec) {
        if (auto* p = dynamic_cast<Head*>(&lhs)) {
            return dispatch_rhs<Head, Executor, BaseRhs, R,
                                RhsTypes, typelist<Head, LhsTail...>,
                                symmetric>::go(*p, rhs, exec);
        }
        return static_dispatch_impl<Executor, BaseLhs, BaseRhs, R,
                                     typelist<LhsTail...>, RhsTypes,
                                     symmetric>::go(lhs, rhs, exec);
    }
};

} // namespace detail

template <
    typename Executor,
    typename BaseLhs,
    typename LhsTypes,
    bool symmetric = true,
    typename BaseRhs = BaseLhs,
    typename RhsTypes = LhsTypes,
    typename ResultType = void
>
class static_dispatcher {
public:
    static ResultType go(BaseLhs& lhs, BaseRhs& rhs, Executor& exec) {
        return detail::static_dispatch_impl<
            Executor, BaseLhs, BaseRhs, ResultType,
            LhsTypes, RhsTypes, symmetric>::go(lhs, rhs, exec);
    }
};

// ─── Basic (Dynamic) Dispatcher ──────────────────────────────────────────────

template <
    typename BaseLhs,
    typename BaseRhs = BaseLhs,
    typename ResultType = void,
    typename CallbackType = std::function<ResultType(BaseLhs&, BaseRhs&)>
>
class basic_dispatcher {
    using key_type = std::pair<std::type_index, std::type_index>;
    std::map<key_type, CallbackType> callbacks_;

public:
    template <typename SomeLhs, typename SomeRhs>
    void add(CallbackType fun) {
        callbacks_[{std::type_index(typeid(SomeLhs)),
                    std::type_index(typeid(SomeRhs))}] = std::move(fun);
    }

    template <typename SomeLhs, typename SomeRhs>
    bool remove() {
        return callbacks_.erase({std::type_index(typeid(SomeLhs)),
                                  std::type_index(typeid(SomeRhs))}) == 1;
    }

    ResultType go(BaseLhs& lhs, BaseRhs& rhs) const {
        key_type k{std::type_index(typeid(lhs)), std::type_index(typeid(rhs))};
        auto it = callbacks_.find(k);
        if (it == callbacks_.end()) {
            throw std::runtime_error("multi-method not found for given types");
        }
        return it->second(lhs, rhs);
    }
};

// ─── Fn Dispatcher (with automatic casting) ─────────────────────────────────

template <typename To, typename From>
struct static_caster {
    static To& cast(From& obj) { return static_cast<To&>(obj); }
};

template <typename To, typename From>
struct dynamic_caster {
    static To& cast(From& obj) { return dynamic_cast<To&>(obj); }
};

template <
    typename BaseLhs,
    typename BaseRhs = BaseLhs,
    typename ResultType = void
>
class fn_dispatcher {
    basic_dispatcher<BaseLhs, BaseRhs, ResultType> backend_;

public:
    template <typename SomeLhs, typename SomeRhs,
              ResultType (*callback)(SomeLhs&, SomeRhs&),
              bool symmetric = false>
    void add() {
        backend_.template add<SomeLhs, SomeRhs>(
            [](BaseLhs& lhs, BaseRhs& rhs) -> ResultType {
                return callback(dynamic_cast<SomeLhs&>(lhs),
                                dynamic_cast<SomeRhs&>(rhs));
            });
        if constexpr (symmetric) {
            backend_.template add<SomeRhs, SomeLhs>(
                [](BaseLhs& lhs, BaseRhs& rhs) -> ResultType {
                    return callback(dynamic_cast<SomeLhs&>(rhs),
                                    dynamic_cast<SomeRhs&>(lhs));
                });
        }
    }

    template <typename SomeLhs, typename SomeRhs>
    void add(std::function<ResultType(SomeLhs&, SomeRhs&)> fun, bool symmetric = false) {
        backend_.template add<SomeLhs, SomeRhs>(
            [fun](BaseLhs& lhs, BaseRhs& rhs) -> ResultType {
                return fun(dynamic_cast<SomeLhs&>(lhs),
                           dynamic_cast<SomeRhs&>(rhs));
            });
        if (symmetric) {
            backend_.template add<SomeRhs, SomeLhs>(
                [fun](BaseLhs& lhs, BaseRhs& rhs) -> ResultType {
                    return fun(dynamic_cast<SomeLhs&>(rhs),
                               dynamic_cast<SomeRhs&>(lhs));
                });
        }
    }

    template <typename SomeLhs, typename SomeRhs, bool symmetric = false>
    bool remove() {
        bool ok = backend_.template remove<SomeLhs, SomeRhs>();
        if constexpr (symmetric) {
            ok = backend_.template remove<SomeRhs, SomeLhs>() || ok;
        }
        return ok;
    }

    ResultType go(BaseLhs& lhs, BaseRhs& rhs) const {
        return backend_.go(lhs, rhs);
    }
};

// ─── Variant Dispatcher: O(1) compile-time double dispatch via std::visit ───
// Zero dynamic_cast, zero RTTI. Use when both sides are std::variant.

template <typename VariantLhs, typename VariantRhs, typename Executor>
auto variant_dispatch(VariantLhs&& lhs, VariantRhs&& rhs, Executor&& exec) {
    return std::visit(
        [&exec](auto& l, auto& r) -> decltype(exec.fire(l, r)) {
            return exec.fire(l, r);
        },
        std::forward<VariantLhs>(lhs),
        std::forward<VariantRhs>(rhs));
}

} // namespace loki
