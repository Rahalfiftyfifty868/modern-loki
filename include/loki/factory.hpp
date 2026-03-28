#pragma once

#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeindex>
#include <typeinfo>

namespace loki {

// ─── Error Policies ──────────────────────────────────────────────────────────

template <typename IdentifierType, typename AbstractProduct>
struct default_factory_error {
    struct exception : std::runtime_error {
        exception() : std::runtime_error("Unknown type in factory") {}
    };
    static std::unique_ptr<AbstractProduct> on_unknown_type(const IdentifierType&) {
        throw exception();
    }
};

// ─── Factory: generic object factory ─────────────────────────────────────────

template <
    typename AbstractProduct,
    typename IdentifierType = std::string,
    typename ProductCreator = std::function<std::unique_ptr<AbstractProduct>()>,
    template <typename, typename> class FactoryErrorPolicy = default_factory_error
>
class factory : public FactoryErrorPolicy<IdentifierType, AbstractProduct> {
    static_assert(std::is_invocable_v<ProductCreator&>,
        "factory: ProductCreator must be callable with no arguments");
    static_assert(std::is_convertible_v<
        std::invoke_result_t<ProductCreator&>, std::unique_ptr<AbstractProduct>>,
        "factory: ProductCreator must return a type convertible to std::unique_ptr<AbstractProduct>");

public:
    bool register_type(const IdentifierType& id, ProductCreator creator) {
        return registry_.emplace(id, std::move(creator)).second;
    }

    bool unregister(const IdentifierType& id) {
        return registry_.erase(id) == 1;
    }

    [[nodiscard]] std::unique_ptr<AbstractProduct> create(const IdentifierType& id) const {
        auto it = registry_.find(id);
        if (it != registry_.end()) {
            return it->second();
        }
        return this->on_unknown_type(id);
    }

    [[nodiscard]] bool is_registered(const IdentifierType& id) const {
        return registry_.contains(id);
    }

    [[nodiscard]] std::size_t size() const { return registry_.size(); }

private:
    std::map<IdentifierType, ProductCreator> registry_;
};

// ─── Self-registering factory helper ────────────────────────────────────────

template <typename Base, typename Derived, typename IdentifierType = std::string>
struct auto_register {
    auto_register(factory<Base, IdentifierType>& f, const IdentifierType& id) {
        f.register_type(id, [] { return std::make_unique<Derived>(); });
    }
};

// ─── Clone Factory ───────────────────────────────────────────────────────────

template <
    typename AbstractProduct,
    typename ProductCloner = std::function<std::unique_ptr<AbstractProduct>(const AbstractProduct&)>,
    template <typename, typename> class FactoryErrorPolicy = default_factory_error
>
class clone_factory : public FactoryErrorPolicy<std::type_index, AbstractProduct> {
    static_assert(std::is_polymorphic_v<AbstractProduct>,
        "clone_factory requires a polymorphic AbstractProduct (needs at least one virtual function)");

public:
    bool register_type(std::type_index ti, ProductCloner cloner) {
        return registry_.emplace(ti, std::move(cloner)).second;
    }

    template <typename Derived>
    bool register_type(ProductCloner cloner) {
        return register_type(std::type_index(typeid(Derived)), std::move(cloner));
    }

    bool unregister(std::type_index ti) {
        return registry_.erase(ti) == 1;
    }

    [[nodiscard]] std::unique_ptr<AbstractProduct> create(const AbstractProduct& model) const {
        auto it = registry_.find(std::type_index(typeid(model)));
        if (it != registry_.end()) {
            return it->second(model);
        }
        return this->on_unknown_type(std::type_index(typeid(model)));
    }

private:
    std::map<std::type_index, ProductCloner> registry_;
};

} // namespace loki
