#pragma once

#include "singleton.hpp"
#include "threads.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <new>
#include <type_traits>
#include <vector>

namespace loki {

// Modern Small Object Allocator.
// The original Loki implementation used a chunk-based fixed-size pool allocator.
// This modernization keeps the same architecture but uses modern C++ idioms:
// alignas, std::byte, sized deallocation, RAII, etc.

inline constexpr std::size_t default_chunk_size = 4096;
inline constexpr std::size_t max_small_object_size = 256;

class fixed_allocator {
    struct chunk {
        std::byte* data = nullptr;
        std::uint16_t first_available = 0;
        std::uint16_t blocks_available = 0;

        void init(std::size_t block_size, std::uint16_t blocks) {
            assert(block_size >= sizeof(std::uint16_t));
            data = static_cast<std::byte*>(
                ::operator new(block_size * blocks));
            first_available = 0;
            blocks_available = blocks;
            auto* p = data;
            for (std::uint16_t i = 0; i < blocks; p += block_size) {
                std::uint16_t next = ++i;
                std::memcpy(p, &next, sizeof(std::uint16_t));
            }
        }

        void* allocate(std::size_t block_size) {
            if (!blocks_available) return nullptr;
            auto* result = data + (first_available * block_size);
            std::memcpy(&first_available, result, sizeof(std::uint16_t));
            --blocks_available;
            return result;
        }

        void deallocate(void* p, std::size_t block_size) {
            auto* released = static_cast<std::byte*>(p);
            assert(released >= data);
            auto offset = static_cast<std::size_t>(released - data);
            assert(offset % block_size == 0);
            std::memcpy(released, &first_available, sizeof(std::uint16_t));
            first_available = static_cast<std::uint16_t>(offset / block_size);
            ++blocks_available;
        }

        void release() {
            ::operator delete(data);
            data = nullptr;
        }
    };

    static constexpr std::size_t no_chunk = static_cast<std::size_t>(-1);

    std::size_t block_size_;
    std::uint16_t num_blocks_;
    std::vector<chunk> chunks_;
    std::size_t dealloc_idx_ = no_chunk;

    // O(1) free-chunk tracking: indices of chunks with available blocks,
    // maintained as an unordered stack so allocate() never scans.
    std::vector<std::size_t> free_list_;

    // Sorted by data pointer for O(log C) chunk lookup on deallocate.
    struct chunk_range {
        const std::byte* start;
        std::size_t index;
    };
    std::vector<chunk_range> chunk_lookup_;

    struct chunk_range_cmp {
        bool operator()(const chunk_range& a, const std::byte* p) const { return a.start < p; }
        bool operator()(const std::byte* p, const chunk_range& a) const { return p < a.start; }
        bool operator()(const chunk_range& a, const chunk_range& b) const { return a.start < b.start; }
    };

    void rebuild_free_list() {
        free_list_.clear();
        for (std::size_t i = 0; i < chunks_.size(); ++i) {
            if (chunks_[i].blocks_available > 0)
                free_list_.push_back(i);
        }
    }

    void rebuild_chunk_lookup() {
        chunk_lookup_.clear();
        chunk_lookup_.reserve(chunks_.size());
        for (std::size_t i = 0; i < chunks_.size(); ++i) {
            chunk_lookup_.push_back({chunks_[i].data, i});
        }
        std::sort(chunk_lookup_.begin(), chunk_lookup_.end(), chunk_range_cmp{});
    }

    // O(log C) binary search to find the chunk owning a pointer.
    std::size_t find_chunk(void* p) const {
        auto* bp = static_cast<const std::byte*>(p);
        auto it = std::upper_bound(chunk_lookup_.begin(), chunk_lookup_.end(),
            bp, chunk_range_cmp{});
        if (it != chunk_lookup_.begin()) {
            --it;
            auto idx = it->index;
            std::less<const std::byte*> lt;
            if (lt(bp, chunks_[idx].data + block_size_ * num_blocks_)) {
                return idx;
            }
        }
        return no_chunk;
    }

public:
    explicit fixed_allocator(std::size_t block_size,
                             std::size_t chunk_size = default_chunk_size)
        : block_size_(std::max(block_size, sizeof(std::uint16_t)))
        , num_blocks_(static_cast<std::uint16_t>(
              std::min<std::size_t>(
                  std::max<std::size_t>(chunk_size / block_size_, 1), 65535)))
    {
        assert(block_size > 0 && "fixed_allocator block_size must be > 0");
        assert(chunk_size >= block_size_ && "chunk_size must be >= block_size");
    }

    fixed_allocator(const fixed_allocator&) = delete;
    fixed_allocator& operator=(const fixed_allocator&) = delete;

    fixed_allocator(fixed_allocator&& rhs) noexcept
        : block_size_(rhs.block_size_)
        , num_blocks_(rhs.num_blocks_)
        , chunks_(std::move(rhs.chunks_))
        , dealloc_idx_(rhs.dealloc_idx_)
        , free_list_(std::move(rhs.free_list_))
        , chunk_lookup_(std::move(rhs.chunk_lookup_))
    {
        rhs.dealloc_idx_ = no_chunk;
    }

    fixed_allocator& operator=(fixed_allocator&& rhs) noexcept {
        if (this != &rhs) {
            for (auto& c : chunks_) c.release();
            block_size_ = rhs.block_size_;
            num_blocks_ = rhs.num_blocks_;
            chunks_ = std::move(rhs.chunks_);
            dealloc_idx_ = rhs.dealloc_idx_;
            free_list_ = std::move(rhs.free_list_);
            chunk_lookup_ = std::move(rhs.chunk_lookup_);
            rhs.dealloc_idx_ = no_chunk;
        }
        return *this;
    }

    ~fixed_allocator() {
        for (auto& c : chunks_) {
            c.release();
        }
    }

    void* allocate() {
        if (free_list_.empty()) {
            chunk c;
            c.init(block_size_, num_blocks_);
            try {
                chunks_.push_back(c);
            } catch (...) {
                c.release();
                throw;
            }
            auto idx = chunks_.size() - 1;
            dealloc_idx_ = idx;
            free_list_.push_back(idx);
            chunk_range cr{chunks_[idx].data, idx};
            auto pos = std::upper_bound(chunk_lookup_.begin(), chunk_lookup_.end(),
                cr.start, chunk_range_cmp{});
            chunk_lookup_.insert(pos, cr);
        }
        auto idx = free_list_.back();
        void* result = chunks_[idx].allocate(block_size_);
        if (chunks_[idx].blocks_available == 0)
            free_list_.pop_back();
        return result;
    }

    void deallocate(void* p) {
        assert(p);
        auto do_dealloc = [this](void* ptr, std::size_t idx) {
            bool was_full = (chunks_[idx].blocks_available == 0);
            chunks_[idx].deallocate(ptr, block_size_);
            if (was_full) free_list_.push_back(idx);
            dealloc_idx_ = idx;
        };
        // Fast path: check last-used chunk
        if (dealloc_idx_ != no_chunk) {
            auto& ch = chunks_[dealloc_idx_];
            auto* bp = static_cast<const std::byte*>(p);
            std::less_equal<const std::byte*> le;
            std::less<const std::byte*> lt;
            if (le(ch.data, bp) && lt(bp, ch.data + block_size_ * num_blocks_)) {
                do_dealloc(p, dealloc_idx_);
                return;
            }
        }
        // O(log C) binary search over sorted chunk ranges
        auto idx = find_chunk(p);
        assert(idx != no_chunk && "pointer not from this allocator");
        do_dealloc(p, idx);
    }

    [[nodiscard]] std::size_t block_size() const { return block_size_; }
};

class small_obj_allocator {
    std::vector<fixed_allocator> pool_;
    std::size_t max_object_size_;

public:
    explicit small_obj_allocator(
        std::size_t chunk_size = default_chunk_size,
        std::size_t max_obj_size = max_small_object_size)
        : max_object_size_(max_obj_size)
    {
        pool_.reserve(max_obj_size);
        for (std::size_t i = 1; i <= max_obj_size; ++i) {
            pool_.emplace_back(i, chunk_size);
        }
    }

    small_obj_allocator(const small_obj_allocator&) = delete;
    small_obj_allocator& operator=(const small_obj_allocator&) = delete;

    void* allocate(std::size_t num_bytes) {
        if (num_bytes == 0) num_bytes = 1;
        if (num_bytes > max_object_size_) {
            return ::operator new(num_bytes);
        }
        return pool_[num_bytes - 1].allocate();
    }

    void deallocate(void* p, std::size_t size) {
        if (!p) return;
        if (size > max_object_size_) {
            ::operator delete(p);
            return;
        }
        if (size == 0) size = 1;
        pool_[size - 1].deallocate(p);
    }
};

// SmallObject: CRTP base that overrides operator new/delete to use the
// small object allocator. Uses singleton for the allocator instance.
template <
    template <typename> class ThreadingModel = single_threaded,
    std::size_t chunk_size = default_chunk_size,
    std::size_t max_obj_size = max_small_object_size
>
class small_object {
    static_assert(std::is_default_constructible_v<typename ThreadingModel<small_object>::lock>,
        "small_object requires a ThreadingModel whose lock is default-constructible "
        "(object_level_lockable is not supported; use class_level_lockable or single_threaded)");

    struct allocator_wrapper : public small_obj_allocator {
        allocator_wrapper() : small_obj_allocator(chunk_size, max_obj_size) {}
    };

public:
    static void* operator new(std::size_t size) {
        typename ThreadingModel<small_object>::lock guard;
        (void)guard;
        return singleton_holder<allocator_wrapper>::instance().allocate(size);
    }

    static void operator delete(void* p, std::size_t size) {
        typename ThreadingModel<small_object>::lock guard;
        (void)guard;
        singleton_holder<allocator_wrapper>::instance().deallocate(p, size);
    }

    virtual ~small_object() = default;
};

} // namespace loki
