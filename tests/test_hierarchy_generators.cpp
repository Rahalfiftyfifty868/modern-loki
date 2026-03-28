#include "doctest.h"
#include <loki/hierarchy_generators.hpp>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <vector>

using namespace loki;

template <typename T>
struct Holder {
    T value{};
};

TEST_CASE("gen_scatter_hierarchy basic field access") {
    using Types = typelist<int, double>;
    gen_scatter_hierarchy<Types, Holder> h;
    field<int>(h).value = 42;
    field<double>(h).value = 3.14;
    CHECK(field<int>(h).value == 42);
    CHECK(field<double>(h).value == doctest::Approx(3.14));
}

TEST_CASE("gen_scatter_hierarchy const field access") {
    using Types = typelist<int, std::string>;
    gen_scatter_hierarchy<Types, Holder> h;
    field<int>(h).value = 7;
    field<std::string>(h).value = "hello";

    const auto& ch = h;
    CHECK(field<int>(ch).value == 7);
    CHECK(field<std::string>(ch).value == "hello");
}

TEST_CASE("gen_scatter_hierarchy with many types") {
    using Types = typelist<int, double, char, std::string, float>;
    gen_scatter_hierarchy<Types, Holder> h;
    field<int>(h).value = 1;
    field<double>(h).value = 2.0;
    field<char>(h).value = 'x';
    field<std::string>(h).value = "abc";
    field<float>(h).value = 3.5f;

    CHECK(field<int>(h).value == 1);
    CHECK(field<double>(h).value == doctest::Approx(2.0));
    CHECK(field<char>(h).value == 'x');
    CHECK(field<std::string>(h).value == "abc");
    CHECK(field<float>(h).value == doctest::Approx(3.5f));
}

TEST_CASE("gen_scatter_hierarchy single type") {
    using Types = typelist<int>;
    gen_scatter_hierarchy<Types, Holder> h;
    field<int>(h).value = 99;
    CHECK(field<int>(h).value == 99);
}

TEST_CASE("gen_scatter_hierarchy type_list alias") {
    using Types = typelist<int, double, char>;
    using H = gen_scatter_hierarchy<Types, Holder>;
    static_assert(std::is_same_v<H::type_list, Types>);
}

TEST_CASE("gen_scatter_hierarchy inherits from each Unit") {
    using Types = typelist<int, double>;
    using H = gen_scatter_hierarchy<Types, Holder>;
    static_assert(std::is_base_of_v<Holder<int>, H>);
    static_assert(std::is_base_of_v<Holder<double>, H>);
}

TEST_CASE("gen_scatter_hierarchy default-constructs values") {
    using Types = typelist<int, std::string>;
    gen_scatter_hierarchy<Types, Holder> h;
    CHECK(field<int>(h).value == 0);
    CHECK(field<std::string>(h).value.empty());
}

TEST_CASE("tuple_unit basic usage") {
    tuple_unit<int> u;
    u.value = 42;
    CHECK(u.value == 42);
    int& ref = u;
    CHECK(ref == 42);
    ref = 10;
    CHECK(u.value == 10);
}

TEST_CASE("tuple_unit const conversion") {
    tuple_unit<std::string> u;
    u.value = "test";
    const auto& cu = u;
    const std::string& ref = cu;
    CHECK(ref == "test");
}

template <typename T, typename Base>
struct LinearUnit : Base {
    T data{};
};

TEST_CASE("gen_linear_hierarchy basic chain") {
    using Types = typelist<int, double, char>;
    using Chain = gen_linear_hierarchy<Types, LinearUnit>;

    Chain c;
    static_cast<LinearUnit<int, gen_linear_hierarchy<typelist<double, char>, LinearUnit>>&>(c).data = 1;
    CHECK(static_cast<LinearUnit<int, gen_linear_hierarchy<typelist<double, char>, LinearUnit>>&>(c).data == 1);
}

TEST_CASE("gen_linear_hierarchy inherits from Unit chain") {
    using Types = typelist<int, double>;
    using Chain = gen_linear_hierarchy<Types, LinearUnit>;
    using Inner = gen_linear_hierarchy<typelist<double>, LinearUnit>;
    static_assert(std::is_base_of_v<LinearUnit<int, Inner>, Chain>);
}

TEST_CASE("gen_linear_hierarchy empty typelist is Root") {
    using Chain = gen_linear_hierarchy<typelist<>, LinearUnit>;
    static_assert(std::is_base_of_v<empty_type, Chain>);
}

struct CustomRoot {
    int root_val = 42;
};

TEST_CASE("gen_linear_hierarchy custom root") {
    using Chain = gen_linear_hierarchy<typelist<int>, LinearUnit, CustomRoot>;
    Chain c;
    static_assert(std::is_base_of_v<CustomRoot, Chain>);
}

TEST_CASE("apply_to_all invokes for each type") {
    using Types = typelist<int, double, char>;
    int count = 0;
    auto counter = [&count]<typename T>() { ++count; };
    apply_to_all(counter, Types{});
    CHECK(count == 3);
}

TEST_CASE("apply_to_all collects type sizes") {
    using Types = typelist<char, int, double>;
    std::vector<std::size_t> sizes;
    auto collector = [&sizes]<typename T>() { sizes.push_back(sizeof(T)); };
    apply_to_all(collector, Types{});

    REQUIRE(sizes.size() == 3);
    CHECK(sizes[0] == sizeof(char));
    CHECK(sizes[1] == sizeof(int));
    CHECK(sizes[2] == sizeof(double));
}

TEST_CASE("apply_to_all on empty typelist") {
    int count = 0;
    auto counter = [&count]<typename T>() { ++count; };
    apply_to_all(counter, typelist<>{});
    CHECK(count == 0);
}

TEST_CASE("apply_to_all on single type") {
    std::string name;
    auto grab = [&name]<typename T>() { name = typeid(T).name(); };
    apply_to_all(grab, typelist<int>{});
    CHECK_FALSE(name.empty());
}
