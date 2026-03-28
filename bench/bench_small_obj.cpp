#include <loki/small_obj.hpp>

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory_resource>
#include <numeric>
#include <random>
#include <string>
#include <vector>

static constexpr int warmup_rounds = 3;
static constexpr int bench_rounds  = 10;

struct result {
    double alloc_ns;
    double dealloc_ns;
};

// ── Benchmark: global new / delete ──────────────────────────────────────────

static result bench_global_new(std::size_t obj_size, std::size_t count,
                               const std::vector<std::size_t>& dealloc_order) {
    std::vector<void*> ptrs(count);

    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < count; ++i)
        ptrs[i] = ::operator new(obj_size);
    auto t1 = std::chrono::steady_clock::now();

    for (auto idx : dealloc_order)
        ::operator delete(ptrs[idx], obj_size);
    auto t2 = std::chrono::steady_clock::now();

    double alloc_ns   = std::chrono::duration<double, std::nano>(t1 - t0).count() / static_cast<double>(count);
    double dealloc_ns = std::chrono::duration<double, std::nano>(t2 - t1).count() / static_cast<double>(count);
    return {alloc_ns, dealloc_ns};
}

// ── Benchmark: loki::small_obj_allocator ────────────────────────────────────

static result bench_loki(loki::small_obj_allocator& alloc, std::size_t obj_size,
                         std::size_t count,
                         const std::vector<std::size_t>& dealloc_order) {
    std::vector<void*> ptrs(count);

    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < count; ++i)
        ptrs[i] = alloc.allocate(obj_size);
    auto t1 = std::chrono::steady_clock::now();

    for (auto idx : dealloc_order)
        alloc.deallocate(ptrs[idx], obj_size);
    auto t2 = std::chrono::steady_clock::now();

    double alloc_ns   = std::chrono::duration<double, std::nano>(t1 - t0).count() / static_cast<double>(count);
    double dealloc_ns = std::chrono::duration<double, std::nano>(t2 - t1).count() / static_cast<double>(count);
    return {alloc_ns, dealloc_ns};
}

// ── Benchmark: std::pmr::monotonic_buffer_resource ──────────────────────────

static result bench_pmr(std::size_t obj_size, std::size_t count) {
    std::size_t buf_size = obj_size * count * 2;
    auto buf = std::make_unique<std::byte[]>(buf_size);
    std::pmr::monotonic_buffer_resource resource(buf.get(), buf_size);
    std::vector<void*> ptrs(count);

    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < count; ++i)
        ptrs[i] = resource.allocate(obj_size, alignof(std::max_align_t));
    auto t1 = std::chrono::steady_clock::now();

    // monotonic_buffer_resource doesn't support individual deallocation;
    // the entire buffer is freed on destruction — measure that.
    auto t2 = t1;

    double alloc_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / static_cast<double>(count);
    double dealloc_ns = std::chrono::duration<double, std::nano>(t2 - t1).count() / static_cast<double>(count);
    return {alloc_ns, dealloc_ns};
}

// ── Helpers ─────────────────────────────────────────────────────────────────

static std::vector<std::size_t> shuffled_indices(std::size_t count, std::mt19937& rng) {
    std::vector<std::size_t> v(count);
    std::iota(v.begin(), v.end(), 0);
    std::shuffle(v.begin(), v.end(), rng);
    return v;
}

static result median_result(std::vector<result>& results) {
    auto by_alloc = [](const result& a, const result& b) { return a.alloc_ns < b.alloc_ns; };
    std::sort(results.begin(), results.end(), by_alloc);
    return results[results.size() / 2];
}

static void print_row(const std::string& label, const result& r) {
    std::cout << "  " << std::left << std::setw(22) << label
              << std::right << std::setw(10) << std::fixed << std::setprecision(1)
              << r.alloc_ns << " ns/alloc"
              << std::setw(12) << r.dealloc_ns << " ns/dealloc\n";
}

int main() {
    std::mt19937 rng(42);

    constexpr std::size_t sizes[]  = {8, 16, 32, 64, 128, 256};
    constexpr std::size_t count    = 100'000;

    std::cout << "Small Object Allocator Benchmark\n"
              << "  " << count << " allocs + deallocs per size, "
              << bench_rounds << " rounds (median), "
              << "random deallocation order\n\n";

    loki::small_obj_allocator loki_alloc;

    for (auto obj_size : sizes) {
        std::cout << "── " << obj_size << " bytes ──\n";

        std::vector<result> res_new, res_loki, res_pmr;

        for (int r = 0; r < warmup_rounds + bench_rounds; ++r) {
            auto order = shuffled_indices(count, rng);

            auto rn = bench_global_new(obj_size, count, order);
            auto rl = bench_loki(loki_alloc, obj_size, count, order);
            auto rp = bench_pmr(obj_size, count);

            if (r >= warmup_rounds) {
                res_new.push_back(rn);
                res_loki.push_back(rl);
                res_pmr.push_back(rp);
            }
        }

        print_row("global new/delete", median_result(res_new));
        print_row("loki::small_obj", median_result(res_loki));
        print_row("pmr::monotonic", median_result(res_pmr));
        std::cout << "\n";
    }

    return 0;
}
