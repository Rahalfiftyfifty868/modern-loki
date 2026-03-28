#include "doctest.h"
#include <loki/type_traits.hpp>
#include <string>

using namespace loki;

TEST_CASE("type_traits is_pointer") {
    static_assert(type_traits<int*>::is_pointer);
    static_assert(!type_traits<int>::is_pointer);
}

TEST_CASE("type_traits is_reference") {
    static_assert(type_traits<int&>::is_reference);
    static_assert(!type_traits<int>::is_reference);
}

TEST_CASE("type_traits is_const") {
    static_assert(type_traits<const int>::is_const);
    static_assert(!type_traits<int>::is_const);
}

TEST_CASE("type_traits pointed_to_type") {
    static_assert(std::is_same_v<type_traits<int*>::pointed_to_type, int>);
}

TEST_CASE("type_traits non_const_type") {
    static_assert(std::is_same_v<type_traits<const int>::non_const_type, int>);
}

TEST_CASE("type_traits parameter_type") {
    static_assert(std::is_same_v<type_traits<int>::parameter_type, int>);
    static_assert(std::is_same_v<type_traits<std::string>::parameter_type, const std::string&>);
    // References pass through as-is
    static_assert(std::is_same_v<type_traits<int&>::parameter_type, int&>);
    static_assert(std::is_same_v<type_traits<const int&>::parameter_type, const int&>);
    // Volatile scalar still by value
    static_assert(std::is_same_v<type_traits<volatile int>::parameter_type, volatile int>);
}

TEST_CASE("type_traits unqualified_type") {
    static_assert(std::is_same_v<type_traits<const volatile int>::unqualified_type, int>);
}

TEST_CASE("type_traits non_const_type through references") {
    static_assert(std::is_same_v<type_traits<const int&>::non_const_type, int&>);
    static_assert(std::is_same_v<type_traits<const int&&>::non_const_type, int&&>);
    static_assert(std::is_same_v<type_traits<const int>::non_const_type, int>);
}

TEST_CASE("type_traits unqualified_type through references") {
    static_assert(std::is_same_v<type_traits<const volatile int&>::unqualified_type, int&>);
}
