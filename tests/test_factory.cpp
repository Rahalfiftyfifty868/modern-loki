#include "doctest.h"
#include <loki/factory.hpp>
#include <string>

using namespace loki;

struct Animal {
    virtual ~Animal() = default;
    virtual std::string speak() const = 0;
};

struct Dog : Animal {
    std::string speak() const override { return "Woof"; }
};

struct Cat : Animal {
    std::string speak() const override { return "Meow"; }
};

TEST_CASE("factory register and create") {
    factory<Animal> f;
    f.register_type("dog", [] { return std::make_unique<Dog>(); });
    f.register_type("cat", [] { return std::make_unique<Cat>(); });

    auto dog = f.create("dog");
    auto cat = f.create("cat");
    CHECK(dog->speak() == "Woof");
    CHECK(cat->speak() == "Meow");
}

TEST_CASE("factory unknown type throws") {
    factory<Animal> f;
    CHECK_THROWS_AS((void)f.create("fish"), std::runtime_error);
}

TEST_CASE("factory unregister") {
    factory<Animal> f;
    f.register_type("dog", [] { return std::make_unique<Dog>(); });
    CHECK(f.is_registered("dog"));
    CHECK(f.unregister("dog"));
    CHECK_FALSE(f.is_registered("dog"));
}

TEST_CASE("factory duplicate register") {
    factory<Animal> f;
    CHECK(f.register_type("dog", [] { return std::make_unique<Dog>(); }));
    CHECK_FALSE(f.register_type("dog", [] { return std::make_unique<Dog>(); }));
}

TEST_CASE("factory size") {
    factory<Animal> f;
    CHECK(f.size() == 0);
    f.register_type("dog", [] { return std::make_unique<Dog>(); });
    CHECK(f.size() == 1);
    f.register_type("cat", [] { return std::make_unique<Cat>(); });
    CHECK(f.size() == 2);
}

TEST_CASE("clone_factory") {
    clone_factory<Animal> cf;
    cf.register_type<Dog>([](const Animal& a) -> std::unique_ptr<Animal> {
        return std::make_unique<Dog>(static_cast<const Dog&>(a));
    });

    Dog original;
    auto cloned = cf.create(original);
    CHECK(cloned->speak() == "Woof");
}
