#include "doctest.h"
#include <loki/assoc_vector.hpp>
#include <string>
#include <string_view>

using namespace loki;

TEST_CASE("assoc_vector insert and find") {
    assoc_vector<int, std::string> av;
    av.insert({1, "one"});
    av.insert({2, "two"});
    av.insert({3, "three"});

    CHECK(av.size() == 3);
    auto it = av.find(2);
    REQUIRE(it != av.end());
    CHECK(it->second == "two");
}

TEST_CASE("assoc_vector operator[]") {
    assoc_vector<std::string, int> av;
    av["alpha"] = 1;
    av["beta"] = 2;
    av["gamma"] = 3;

    CHECK(av["alpha"] == 1);
    CHECK(av["beta"] == 2);
    CHECK(av.size() == 3);
}

TEST_CASE("assoc_vector erase") {
    assoc_vector<int, int> av;
    av.insert({1, 10});
    av.insert({2, 20});
    av.insert({3, 30});

    CHECK(av.erase(2) == 1);
    CHECK(av.size() == 2);
    CHECK_FALSE(av.contains(2));
}

TEST_CASE("assoc_vector contains") {
    assoc_vector<int, int> av;
    av.insert({42, 100});
    CHECK(av.contains(42));
    CHECK_FALSE(av.contains(99));
}

TEST_CASE("assoc_vector at") {
    assoc_vector<int, std::string> av;
    av.insert({1, "hello"});
    CHECK(av.at(1) == "hello");
    CHECK_THROWS_AS(av.at(999), std::out_of_range);
}

TEST_CASE("assoc_vector initializer_list") {
    assoc_vector<int, std::string> av{
        {1, "one"}, {2, "two"}, {3, "three"}
    };
    CHECK(av.size() == 3);
    CHECK(av[1] == "one");
}

TEST_CASE("assoc_vector iteration is sorted") {
    assoc_vector<int, int> av;
    av.insert({3, 30});
    av.insert({1, 10});
    av.insert({2, 20});

    int prev = -1;
    for (const auto& [k, v] : av) {
        (void)v;
        CHECK(k > prev);
        prev = k;
    }
}

TEST_CASE("assoc_vector clear") {
    assoc_vector<int, int> av{{1, 1}, {2, 2}};
    av.clear();
    CHECK(av.empty());
    CHECK(av.size() == 0);
}

TEST_CASE("assoc_vector swap") {
    assoc_vector<int, int> av1{{1, 10}};
    assoc_vector<int, int> av2{{2, 20}, {3, 30}};
    av1.swap(av2);
    CHECK(av1.size() == 2);
    CHECK(av2.size() == 1);
}

TEST_CASE("assoc_vector lower/upper bound") {
    assoc_vector<int, int> av{{1, 10}, {3, 30}, {5, 50}};
    auto lb = av.lower_bound(3);
    REQUIRE(lb != av.end());
    CHECK(lb->first == 3);

    auto ub = av.upper_bound(3);
    REQUIRE(ub != av.end());
    CHECK(ub->first == 5);
}

TEST_CASE("assoc_vector range insert deduplicates") {
    assoc_vector<int, std::string> av;
    av.insert({1, "one"});
    av.insert({3, "three"});

    std::vector<std::pair<int, std::string>> batch = {
        {2, "two"}, {3, "NEW"}, {4, "four"}, {1, "NEW"}
    };
    av.insert(batch.begin(), batch.end());

    CHECK(av.size() == 4);
    CHECK(av.at(1) == "one");
    CHECK(av.at(2) == "two");
    CHECK(av.at(3) == "three");
    CHECK(av.at(4) == "four");
}

TEST_CASE("assoc_vector try_emplace") {
    assoc_vector<std::string, int> av;
    auto [it1, inserted1] = av.try_emplace("key", 42);
    CHECK(inserted1);
    CHECK(it1->second == 42);

    auto [it2, inserted2] = av.try_emplace("key", 99);
    CHECK_FALSE(inserted2);
    CHECK(it2->second == 42);
}

TEST_CASE("assoc_vector operator[] with rvalue key") {
    assoc_vector<std::string, int> av;
    av[std::string("hello")] = 42;
    CHECK(av.size() == 1);
    CHECK(av.at("hello") == 42);
}

TEST_CASE("assoc_vector try_emplace with rvalue key") {
    assoc_vector<std::string, int> av;
    std::string key = "moved";
    auto [it, inserted] = av.try_emplace(std::move(key), 99);
    CHECK(inserted);
    CHECK(it->second == 99);
}

TEST_CASE("assoc_vector heterogeneous find with transparent comparator") {
    assoc_vector<std::string, int, std::less<>> av;
    av.insert({"alpha", 1});
    av.insert({"beta", 2});
    av.insert({"gamma", 3});

    std::string_view sv = "beta";
    auto it = av.find(sv);
    REQUIRE(it != av.end());
    CHECK(it->second == 2);

    auto it2 = av.find("gamma");
    REQUIRE(it2 != av.end());
    CHECK(it2->second == 3);

    CHECK(av.find(std::string_view("missing")) == av.end());
}

TEST_CASE("assoc_vector heterogeneous contains/count") {
    assoc_vector<std::string, int, std::less<>> av;
    av.insert({"hello", 42});

    CHECK(av.contains(std::string_view("hello")));
    CHECK_FALSE(av.contains(std::string_view("world")));
    CHECK(av.count(std::string_view("hello")) == 1);
    CHECK(av.count(std::string_view("world")) == 0);
}

TEST_CASE("assoc_vector heterogeneous lower/upper bound") {
    assoc_vector<std::string, int, std::less<>> av{
        {"apple", 1}, {"cherry", 3}, {"elderberry", 5}
    };

    auto lb = av.lower_bound(std::string_view("cherry"));
    REQUIRE(lb != av.end());
    CHECK(lb->first == "cherry");

    auto ub = av.upper_bound(std::string_view("cherry"));
    REQUIRE(ub != av.end());
    CHECK(ub->first == "elderberry");
}

TEST_CASE("assoc_vector heterogeneous equal_range") {
    assoc_vector<std::string, int, std::less<>> av{
        {"a", 1}, {"b", 2}, {"c", 3}
    };

    auto [lo, hi] = av.equal_range(std::string_view("b"));
    REQUIRE(lo != av.end());
    CHECK(lo->first == "b");
    CHECK(hi != av.end());
    CHECK(hi->first == "c");
}

TEST_CASE("assoc_vector heterogeneous erase") {
    assoc_vector<std::string, int, std::less<>> av{
        {"x", 1}, {"y", 2}, {"z", 3}
    };
    CHECK(av.erase(std::string_view("y")) == 1);
    CHECK(av.size() == 2);
    CHECK_FALSE(av.contains(std::string_view("y")));
}

TEST_CASE("assoc_vector non-transparent comparator has no heterogeneous overloads") {
    assoc_vector<std::string, int> av;
    av.insert({"key", 42});
    auto it = av.find(std::string("key"));
    REQUIRE(it != av.end());
    CHECK(it->second == 42);
}
