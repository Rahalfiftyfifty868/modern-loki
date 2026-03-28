#include "doctest.h"
#include <loki/threads.hpp>
#include <thread>
#include <vector>

using namespace loki;

TEST_CASE("single_threaded atomic ops") {
    single_threaded<int>::int_type val = 0;
    single_threaded<int>::atomic_increment(val);
    CHECK(val == 1);
    single_threaded<int>::atomic_add(val, 5);
    CHECK(val == 6);
    single_threaded<int>::atomic_subtract(val, 2);
    CHECK(val == 4);
    single_threaded<int>::atomic_decrement(val);
    CHECK(val == 3);
    single_threaded<int>::atomic_assign(val, 10);
    CHECK(single_threaded<int>::atomic_load(val) == 10);
}

TEST_CASE("single_threaded lock compiles") {
    single_threaded<int>::lock l;
    (void)l;
}

struct SharedData : object_level_lockable<SharedData> {
    int counter = 0;
};

TEST_CASE("object_level_lockable thread safety") {
    SharedData data;
    constexpr int num_threads = 4;
    constexpr int increments = 1000;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&data]() {
            for (int j = 0; j < increments; ++j) {
                object_level_lockable<SharedData>::lock guard(data);
                ++data.counter;
            }
        });
    }
    for (auto& t : threads) t.join();

    CHECK(data.counter == num_threads * increments);
}

TEST_CASE("object_level_lockable uniform atomic API") {
    object_level_lockable<int>::int_type val{0};
    object_level_lockable<int>::atomic_increment(val);
    CHECK(object_level_lockable<int>::atomic_load(val) == 1);
    object_level_lockable<int>::atomic_add(val, 5);
    CHECK(object_level_lockable<int>::atomic_load(val) == 6);
    object_level_lockable<int>::atomic_subtract(val, 2);
    CHECK(object_level_lockable<int>::atomic_load(val) == 4);
    object_level_lockable<int>::atomic_decrement(val);
    CHECK(object_level_lockable<int>::atomic_load(val) == 3);
    object_level_lockable<int>::atomic_assign(val, 42);
    CHECK(object_level_lockable<int>::atomic_load(val) == 42);
}

struct ClassData : class_level_lockable<ClassData> {
    static inline int counter = 0;
};

TEST_CASE("class_level_lockable thread safety") {
    ClassData::counter = 0;
    constexpr int num_threads = 4;
    constexpr int increments = 1000;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([]() {
            for (int j = 0; j < increments; ++j) {
                class_level_lockable<ClassData>::lock guard;
                ++ClassData::counter;
            }
        });
    }
    for (auto& t : threads) t.join();

    CHECK(ClassData::counter == num_threads * increments);
}

TEST_CASE("rw_lockable basic") {
    struct RWData : rw_lockable<RWData> {
        int value = 0;
    };

    RWData data;
    {
        rw_lockable<RWData>::write_lock wl(data);
        data.value = 42;
    }
    {
        rw_lockable<RWData>::read_lock rl(data);
        CHECK(data.value == 42);
    }
}
