#include "doctest.h"
#include <loki/functor.hpp>
#include <string>

using namespace loki;

int add(int a, int b) { return a + b; }

TEST_CASE("functor from function pointer") {
    functor<int, int, int> f(add);
    CHECK(f(3, 4) == 7);
}

TEST_CASE("functor from lambda") {
    functor<int, int, int> f([](int a, int b) { return a * b; });
    CHECK(f(3, 4) == 12);
}

TEST_CASE("functor bool conversion") {
    functor<int, int, int> f;
    CHECK_FALSE(static_cast<bool>(f));
    f = functor<int, int, int>(add);
    CHECK(static_cast<bool>(f));
}

struct Calculator {
    int offset;
    int compute(int x) const { return x + offset; }
    void set_offset(int v) { offset = v; }
};

TEST_CASE("functor from member function") {
    Calculator calc{10};
    functor<int, int> f(calc, &Calculator::compute);
    CHECK(f(5) == 15);
}

TEST_CASE("functor from non-const member function") {
    Calculator calc{0};
    functor<void, int> f(calc, &Calculator::set_offset);
    f(42);
}

TEST_CASE("make_functor from non-const member function") {
    Calculator calc{0};
    auto f = make_functor(calc, &Calculator::set_offset);
    f(99);
}

TEST_CASE("bind_first") {
    functor<int, int, int> f(add);
    auto bound = bind_first(f, 10);
    CHECK(bound(5) == 15);
}

TEST_CASE("make_functor from function pointer") {
    auto f = make_functor(add);
    CHECK(f(2, 3) == 5);
}

TEST_CASE("functor swap") {
    functor<int, int, int> f1(add);
    functor<int, int, int> f2([](int a, int b) { return a - b; });
    f1.swap(f2);
    CHECK(f1(10, 3) == 7);
    CHECK(f2(10, 3) == 13);
}

TEST_CASE("chain") {
    functor<int, int> doubler([](int x) { return x * 2; });
    functor<int, int> add_one([](int x) { return x + 1; });
    auto composed = chain(doubler, add_one);
    CHECK(composed(5) == 12);
}

TEST_CASE("functor_ref from lvalue lambda") {
    auto lambda = [](int a, int b) { return a + b; };
    functor_ref<int, int, int> fr(lambda);
    CHECK(fr(3, 4) == 7);
}

TEST_CASE("functor_ref from function pointer") {
    functor_ref<int, int, int> fr(add);
    CHECK(fr(10, 20) == 30);
}

TEST_CASE("functor_ref bool conversion") {
    functor_ref<int, int, int> fr;
    CHECK_FALSE(static_cast<bool>(fr));

    auto lambda = [](int a, int b) { return a + b; };
    functor_ref<int, int, int> fr2(lambda);
    CHECK(static_cast<bool>(fr2));
}

TEST_CASE("make_functor from lambda returns loki::functor") {
    auto f = make_functor([](int a, int b) { return a + b; });
    static_assert(std::is_same_v<decltype(f), functor<int, int, int>>);
    CHECK(f(3, 7) == 10);
}

TEST_CASE("functor_ref is non-owning (sees mutation)") {
    int capture = 100;
    auto lambda = [&capture](int x) { return x + capture; };
    functor_ref<int, int> fr(lambda);
    CHECK(fr(5) == 105);
    capture = 200;
    CHECK(fr(5) == 205);
}
