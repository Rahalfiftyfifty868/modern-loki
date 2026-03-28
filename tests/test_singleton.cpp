#include "doctest.h"
#include <loki/singleton.hpp>
#include <stdexcept>

using namespace loki;

struct MySingleton {
    int value = 42;
};

TEST_CASE("singleton_holder returns same instance") {
    auto& s1 = singleton_holder<MySingleton>::instance();
    auto& s2 = singleton_holder<MySingleton>::instance();
    CHECK(&s1 == &s2);
    CHECK(s1.value == 42);
}

TEST_CASE("meyers_singleton returns same instance") {
    auto& s1 = meyers_singleton<MySingleton>();
    auto& s2 = meyers_singleton<MySingleton>();
    CHECK(&s1 == &s2);
}

TEST_CASE("singleton with create_static") {
    using Holder = singleton_holder<MySingleton, create_static>;
    auto& s = Holder::instance();
    CHECK(s.value == 42);
}

TEST_CASE("singleton with no_destroy") {
    using Holder = singleton_holder<MySingleton, create_using_new, no_destroy>;
    auto& s = Holder::instance();
    CHECK(s.value == 42);
}

TEST_CASE("singleton with create_using_pmr") {
    using Holder = singleton_holder<MySingleton, create_using_pmr>;
    auto& s = Holder::instance();
    CHECK(s.value == 42);
}

struct ThrowingCtor {
    ThrowingCtor() { throw std::runtime_error("ctor fails"); }
};

TEST_CASE("create_using_pmr does not leak on constructor throw") {
    CHECK_THROWS_AS(create_using_pmr<ThrowingCtor>::create(), std::runtime_error);
}

TEST_CASE("singleton_holder lock must be default-constructible (compile-time)") {
    static_assert(
        std::is_default_constructible_v<single_threaded<int>::lock>,
        "single_threaded lock should be default-constructible");
    static_assert(
        std::is_default_constructible_v<class_level_lockable<int>::lock>,
        "class_level_lockable lock should be default-constructible");
    static_assert(
        !std::is_default_constructible_v<object_level_lockable<int>::lock>,
        "object_level_lockable lock should NOT be default-constructible");
}
