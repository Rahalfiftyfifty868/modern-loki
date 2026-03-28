#include "doctest.h"
#include <loki/smart_ptr.hpp>

using namespace loki;

TEST_CASE("smart_ptr default is null") {
    smart_ptr<int> sp;
    CHECK_FALSE(static_cast<bool>(sp));
}

TEST_CASE("smart_ptr ref counted") {
    auto* raw = new int(42);
    smart_ptr<int> sp1(raw);
    CHECK(*sp1 == 42);
    {
        smart_ptr<int> sp2 = sp1;
        CHECK(*sp2 == 42);
        CHECK(sp1.get() == sp2.get());
    }
    CHECK(*sp1 == 42);
}

TEST_CASE("smart_ptr arrow operator") {
    struct Foo { int val = 99; };
    smart_ptr<Foo> sp(new Foo);
    CHECK(sp->val == 99);
}

TEST_CASE("smart_ptr comparison") {
    auto* p1 = new int(1);
    auto* p2 = new int(2);
    smart_ptr<int> sp1(p1);
    smart_ptr<int> sp2(p2);
    CHECK(sp1 != sp2);

    smart_ptr<int> sp3 = sp1;
    CHECK(sp1 == sp3);
}

TEST_CASE("smart_ptr swap") {
    smart_ptr<int> sp1(new int(10));
    smart_ptr<int> sp2(new int(20));
    auto* orig1 = sp1.get();
    auto* orig2 = sp2.get();
    sp1.swap(sp2);
    CHECK(sp1.get() == orig2);
    CHECK(sp2.get() == orig1);
}

TEST_CASE("smart_ptr throw_on_null") {
    smart_ptr<int, default_sp_storage, ref_counted, throw_on_null> sp;
    CHECK_THROWS_AS(*sp, std::runtime_error);
}

TEST_CASE("smart_ptr move nulls source") {
    smart_ptr<int> sp1(new int(42));
    auto* raw = sp1.get();
    smart_ptr<int> sp2(std::move(sp1));
    CHECK(sp2.get() == raw);
    CHECK(sp1.get() == nullptr);
}

TEST_CASE("smart_ptr deep_copy non-polymorphic") {
    smart_ptr<int, default_sp_storage, deep_copy, no_check> sp(new int(42));
    CHECK(*sp == 42);
    smart_ptr<int, default_sp_storage, deep_copy, no_check> sp2 = sp;
    CHECK(*sp2 == 42);
    CHECK(sp.get() != sp2.get());
}

TEST_CASE("smart_ptr move assign") {
    smart_ptr<int> sp1(new int(10));
    smart_ptr<int> sp2(new int(20));
    sp2 = std::move(sp1);
    CHECK(sp1.get() == nullptr);
    CHECK(*sp2 == 10);
}

template <typename T>
struct counting_check {
    mutable int check_count = 0;
    static void on_default(const T&) {}
    static void on_init(const T&) {}
    void on_dereference(const T&) const { ++check_count; }
};

TEST_CASE("smart_ptr swap includes checking policy state") {
    smart_ptr<int, default_sp_storage, ref_counted, counting_check> sp1(new int(1));
    smart_ptr<int, default_sp_storage, ref_counted, counting_check> sp2(new int(2));
    *sp1;
    *sp1;
    *sp2;
    using check_t = counting_check<int*>;
    CHECK(static_cast<check_t&>(sp1).check_count == 2);
    CHECK(static_cast<check_t&>(sp2).check_count == 1);
    sp1.swap(sp2);
    CHECK(static_cast<check_t&>(sp1).check_count == 1);
    CHECK(static_cast<check_t&>(sp2).check_count == 2);
}
