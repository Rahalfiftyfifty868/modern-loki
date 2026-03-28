#include "doctest.h"
#include <loki/abstract_factory.hpp>
#include <string>

using namespace loki;

struct Widget {
    virtual ~Widget() = default;
    virtual std::string name() const = 0;
};

struct Gadget {
    virtual ~Gadget() = default;
    virtual std::string name() const = 0;
};

struct ConcreteWidget : Widget {
    std::string name() const override { return "ConcreteWidget"; }
};

struct ConcreteGadget : Gadget {
    std::string name() const override { return "ConcreteGadget"; }
};

TEST_CASE("concrete_factory creates products") {
    using AF = abstract_factory<Widget, Gadget>;
    using CF = concrete_factory<AF, ConcreteWidget, ConcreteGadget>;

    CF factory;
    auto w = factory.create<Widget>();
    auto g = factory.create<Gadget>();
    REQUIRE(w != nullptr);
    REQUIRE(g != nullptr);
    CHECK(w->name() == "ConcreteWidget");
    CHECK(g->name() == "ConcreteGadget");
}

TEST_CASE("concrete_factory through base pointer") {
    using AF = abstract_factory<Widget, Gadget>;
    using CF = concrete_factory<AF, ConcreteWidget, ConcreteGadget>;

    std::unique_ptr<AF> factory = std::make_unique<CF>();
    auto w = factory->create<Widget>();
    auto g = factory->create<Gadget>();
    REQUIRE(w != nullptr);
    REQUIRE(g != nullptr);
    CHECK(w->name() == "ConcreteWidget");
    CHECK(g->name() == "ConcreteGadget");
}

TEST_CASE("simple_concrete_factory creates products") {
    using AF = abstract_factory<Widget, Gadget>;
    simple_concrete_factory<AF> factory;
    factory.set_creator<0>([] { return std::make_unique<ConcreteWidget>(); });
    factory.set_creator<1>([] { return std::make_unique<ConcreteGadget>(); });
    factory.done();

    auto w = factory.create<Widget>();
    auto g = factory.create<Gadget>();
    REQUIRE(w != nullptr);
    REQUIRE(g != nullptr);
    CHECK(w->name() == "ConcreteWidget");
    CHECK(g->name() == "ConcreteGadget");
}

TEST_CASE("abstract_factory create is const-callable") {
    using AF = abstract_factory<Widget, Gadget>;
    using CF = concrete_factory<AF, ConcreteWidget, ConcreteGadget>;

    const CF factory;
    auto w = factory.create<Widget>();
    auto g = factory.create<Gadget>();
    REQUIRE(w != nullptr);
    REQUIRE(g != nullptr);
    CHECK(w->name() == "ConcreteWidget");
    CHECK(g->name() == "ConcreteGadget");
}

TEST_CASE("simple_concrete_factory returns null when no creator set") {
    using AF = abstract_factory<Widget>;
    simple_concrete_factory<AF> factory;
    auto w = factory.create<Widget>();
    CHECK(w == nullptr);
}
