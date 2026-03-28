#include "doctest.h"
#include <loki/small_obj.hpp>
#include <vector>

using namespace loki;

TEST_CASE("fixed_allocator basic") {
    fixed_allocator alloc(sizeof(int));
    void* p1 = alloc.allocate();
    REQUIRE(p1 != nullptr);
    void* p2 = alloc.allocate();
    REQUIRE(p2 != nullptr);
    CHECK(p1 != p2);
    alloc.deallocate(p1);
    alloc.deallocate(p2);
}

TEST_CASE("fixed_allocator move leaves source safe") {
    fixed_allocator src(sizeof(int));
    void* p1 = src.allocate();
    REQUIRE(p1 != nullptr);
    src.deallocate(p1);

    fixed_allocator dst(std::move(src));
    void* p2 = dst.allocate();
    REQUIRE(p2 != nullptr);
    dst.deallocate(p2);
}

TEST_CASE("fixed_allocator custom chunk_size") {
    fixed_allocator alloc(4, 64);
    void* p = alloc.allocate();
    REQUIRE(p != nullptr);
    alloc.deallocate(p);
}

TEST_CASE("small_obj_allocator basic") {
    small_obj_allocator alloc;
    void* p1 = alloc.allocate(16);
    REQUIRE(p1 != nullptr);
    void* p2 = alloc.allocate(32);
    REQUIRE(p2 != nullptr);
    alloc.deallocate(p1, 16);
    alloc.deallocate(p2, 32);
}

TEST_CASE("small_obj_allocator with custom chunk_size") {
    small_obj_allocator alloc(512, 64);
    void* p = alloc.allocate(16);
    REQUIRE(p != nullptr);
    alloc.deallocate(p, 16);
}

TEST_CASE("small_obj_allocator large object fallback") {
    small_obj_allocator alloc(4096, 64);
    void* p = alloc.allocate(1024);
    REQUIRE(p != nullptr);
    alloc.deallocate(p, 1024);
}

struct MySmallObj : small_object<> {
    int x;
    int y;
    MySmallObj(int a, int b) : x(a), y(b) {}
};

TEST_CASE("small_object new/delete") {
    auto* obj = new MySmallObj(1, 2);
    CHECK(obj->x == 1);
    CHECK(obj->y == 2);
    delete obj;
}

TEST_CASE("small_object multiple allocations") {
    std::vector<MySmallObj*> objs;
    for (int i = 0; i < 100; ++i) {
        objs.push_back(new MySmallObj(i, i * 2));
    }
    for (int i = 0; i < 100; ++i) {
        CHECK(objs[i]->x == i);
        CHECK(objs[i]->y == i * 2);
    }
    for (auto* p : objs) delete p;
}

TEST_CASE("fixed_allocator survives vector reallocation of chunks") {
    fixed_allocator alloc(4, 16);
    std::vector<void*> ptrs;
    for (int i = 0; i < 200; ++i) {
        void* p = alloc.allocate();
        REQUIRE(p != nullptr);
        ptrs.push_back(p);
    }
    for (int i = static_cast<int>(ptrs.size()) - 1; i >= 0; --i) {
        alloc.deallocate(ptrs[static_cast<std::size_t>(i)]);
    }
    void* p = alloc.allocate();
    REQUIRE(p != nullptr);
    alloc.deallocate(p);
}

TEST_CASE("fixed_allocator interleaved alloc/dealloc across chunks") {
    fixed_allocator alloc(8, 32);
    void* p1 = alloc.allocate();
    void* p2 = alloc.allocate();
    void* p3 = alloc.allocate();
    void* p4 = alloc.allocate();
    void* p5 = alloc.allocate();
    REQUIRE(p1 != nullptr);
    REQUIRE(p5 != nullptr);

    alloc.deallocate(p3);
    alloc.deallocate(p1);

    void* p6 = alloc.allocate();
    void* p7 = alloc.allocate();
    REQUIRE(p6 != nullptr);
    REQUIRE(p7 != nullptr);

    alloc.deallocate(p2);
    alloc.deallocate(p4);
    alloc.deallocate(p5);
    alloc.deallocate(p6);
    alloc.deallocate(p7);
}

TEST_CASE("fixed_allocator move preserves cached indices") {
    fixed_allocator src(sizeof(int));
    void* p1 = src.allocate();
    void* p2 = src.allocate();
    src.deallocate(p1);

    fixed_allocator dst(std::move(src));
    dst.deallocate(p2);
    void* p3 = dst.allocate();
    REQUIRE(p3 != nullptr);
    dst.deallocate(p3);
}
