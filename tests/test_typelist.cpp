#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <loki/typelist.hpp>

using namespace loki;

TEST_CASE("typelist length") {
    static_assert(tl::length_v<typelist<int, double, char>> == 3);
    static_assert(tl::length_v<typelist<>> == 0);
}

TEST_CASE("typelist type_at") {
    using TL = typelist<int, double, char>;
    static_assert(std::is_same_v<tl::type_at_t<TL, 0>, int>);
    static_assert(std::is_same_v<tl::type_at_t<TL, 1>, double>);
    static_assert(std::is_same_v<tl::type_at_t<TL, 2>, char>);
}

TEST_CASE("typelist index_of") {
    using TL = typelist<int, double, char>;
    static_assert(tl::index_of_v<TL, int> == 0);
    static_assert(tl::index_of_v<TL, double> == 1);
    static_assert(tl::index_of_v<TL, char> == 2);
    static_assert(tl::index_of_v<TL, float> == -1);
}

TEST_CASE("typelist index_of on empty typelist") {
    static_assert(tl::index_of_v<typelist<>, int> == -1);
}

TEST_CASE("typelist push_front") {
    static_assert(std::is_same_v<tl::push_front_t<typelist<double, char>, int>,
                                typelist<int, double, char>>);
}

TEST_CASE("typelist push_back") {
    static_assert(std::is_same_v<tl::push_back_t<typelist<int, double>, char>,
                                typelist<int, double, char>>);
}

TEST_CASE("typelist concat") {
    static_assert(std::is_same_v<tl::concat_t<typelist<int, double>, typelist<char, float>>,
                                typelist<int, double, char, float>>);
}

TEST_CASE("typelist erase") {
    static_assert(std::is_same_v<tl::erase_t<typelist<int, double, char>, double>,
                                typelist<int, char>>);
}

TEST_CASE("typelist erase_all") {
    static_assert(std::is_same_v<tl::erase_all_t<typelist<int, double, int, char>, int>,
                                typelist<double, char>>);
}

TEST_CASE("typelist no_duplicates") {
    using Result = tl::no_duplicates_t<typelist<int, double, int, char, double>>;
    static_assert(tl::length_v<Result> == 3);
    static_assert(tl::contains_v<Result, int>);
    static_assert(tl::contains_v<Result, double>);
    static_assert(tl::contains_v<Result, char>);
}

TEST_CASE("typelist replace") {
    static_assert(std::is_same_v<tl::replace_t<typelist<int, double, char>, double, float>,
                                typelist<int, float, char>>);
}

TEST_CASE("typelist reverse") {
    static_assert(std::is_same_v<tl::reverse_t<typelist<int, double, char>>,
                                typelist<char, double, int>>);
}

TEST_CASE("typelist contains") {
    using TL = typelist<int, double, char>;
    static_assert(tl::contains_v<TL, int>);
    static_assert(!tl::contains_v<TL, float>);
}

TEST_CASE("typelist front and back") {
    using TL = typelist<int, double, char>;
    static_assert(std::is_same_v<tl::front_t<TL>, int>);
    static_assert(std::is_same_v<tl::back_t<TL>, char>);
}

TEST_CASE("typelist pop_front") {
    static_assert(std::is_same_v<tl::pop_front_t<typelist<int, double, char>>,
                                typelist<double, char>>);
}

TEST_CASE("typelist transform") {
    static_assert(std::is_same_v<tl::transform_t<typelist<int, double, char>, std::add_pointer>,
                                typelist<int*, double*, char*>>);
}
