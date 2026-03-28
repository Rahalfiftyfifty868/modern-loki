#pragma once

#include "typelist.hpp"

#include <cassert>
#include <concepts>
#include <functional>
#include <memory>
#include <tuple>

namespace loki {

// ─── AbstractFactory ─────────────────────────────────────────────────────────
// Tuple-based dispatch. Zero virtual inheritance, zero diamonds, zero RTTI.
// Creators are frozen after construction: set_creator asserts if called
// after freeze(). Derived classes call freeze() at the end of their ctor.

template <typename... Products>
class abstract_factory {
    std::tuple<std::function<std::unique_ptr<Products>()>...> creators_;
    bool frozen_ = false;

public:
    using product_list = typelist<Products...>;
    virtual ~abstract_factory() = default;

    template <typename T>
        requires (std::same_as<T, Products> || ...)
    [[nodiscard]] std::unique_ptr<T> create() const {
        constexpr std::size_t I = static_cast<std::size_t>(
            tl::index_of_v<product_list, T>);
        auto& creator = std::get<I>(creators_);
        if (creator) return creator();
        return nullptr;
    }

protected:
    abstract_factory() = default;

    template <typename T>
        requires (std::same_as<T, Products> || ...)
    void set_creator(std::function<std::unique_ptr<T>()> creator) {
        assert(!frozen_ && "set_creator called after factory was frozen");
        constexpr std::size_t I = static_cast<std::size_t>(
            tl::index_of_v<product_list, T>);
        std::get<I>(creators_) = std::move(creator);
    }

    void freeze() { frozen_ = true; }
};

// ─── ConcreteFactory ─────────────────────────────────────────────────────────

template <typename AbstractFact, typename... ConcreteProducts>
class concrete_factory;

template <typename... AbstractProducts, typename... ConcreteProducts>
class concrete_factory<abstract_factory<AbstractProducts...>, ConcreteProducts...>
    : public abstract_factory<AbstractProducts...> {
    static_assert(sizeof...(AbstractProducts) == sizeof...(ConcreteProducts),
        "number of concrete products must match abstract products");
    static_assert((std::is_base_of_v<AbstractProducts, ConcreteProducts> && ...),
        "each ConcreteProduct must derive from its corresponding AbstractProduct");

    template <typename Abstract, typename Concrete>
    void wire() {
        this->template set_creator<Abstract>(
            []() -> std::unique_ptr<Abstract> { return std::make_unique<Concrete>(); });
    }

public:
    concrete_factory() {
        (wire<AbstractProducts, ConcreteProducts>(), ...);
        this->freeze();
    }
};

// ─── SimpleConcreteFactory ──────────────────────────────────────────────────

template <typename AbstractFact>
class simple_concrete_factory;

template <typename... AbstractProducts>
class simple_concrete_factory<abstract_factory<AbstractProducts...>>
    : public abstract_factory<AbstractProducts...> {

    template <std::size_t I>
    using abstract_t = tl::type_at_t<typelist<AbstractProducts...>, I>;

public:
    template <std::size_t I>
    void set_creator(std::function<std::unique_ptr<abstract_t<I>>()> creator) {
        this->template abstract_factory<AbstractProducts...>::template set_creator<abstract_t<I>>(
            std::move(creator));
    }

    void done() { this->freeze(); }
};

} // namespace loki
