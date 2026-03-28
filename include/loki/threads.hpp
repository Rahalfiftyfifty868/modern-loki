#pragma once

#include <atomic>
#include <mutex>
#include <shared_mutex>

namespace loki {

// Modern threading policies using C++20 standard threading primitives.
// All policies expose a uniform static API for atomic operations.

namespace detail {

// Non-atomic specialization for single-threaded policies.
struct plain_atomic_ops {
    using int_type = int;
    static int_type atomic_add(int_type& lval, int_type val) { return lval += val; }
    static int_type atomic_subtract(int_type& lval, int_type val) { return lval -= val; }
    static int_type atomic_increment(int_type& lval) { return ++lval; }
    static int_type atomic_decrement(int_type& lval) { return --lval; }
    static void atomic_assign(int_type& lval, int_type val) { lval = val; }
    static int_type atomic_load(const int_type& lval) { return lval; }
};

// std::atomic specialization for multi-threaded policies.
struct std_atomic_ops {
    using int_type = std::atomic<int>;
    static int atomic_add(int_type& lval, int val) { return lval.fetch_add(val) + val; }
    static int atomic_subtract(int_type& lval, int val) { return lval.fetch_sub(val) - val; }
    static int atomic_increment(int_type& lval) { return ++lval; }
    static int atomic_decrement(int_type& lval) { return --lval; }
    static void atomic_assign(int_type& lval, int val) { lval.store(val); }
    static int atomic_load(const int_type& lval) { return lval.load(); }
};

} // namespace detail

template <typename Host>
class single_threaded : public detail::plain_atomic_ops {
public:
    struct lock {
        lock() = default;
        explicit lock(const single_threaded&) {}
    };

    using volatile_type = Host;
};

template <typename Host>
class object_level_lockable : public detail::std_atomic_ops {
    mutable std::mutex mtx_;

public:
    class lock {
        std::unique_lock<std::mutex> guard_;
    public:
        explicit lock(const object_level_lockable& host)
            : guard_(host.mtx_) {}
        lock(const lock&) = delete;
        lock& operator=(const lock&) = delete;
    };

    using volatile_type = volatile Host;
};

template <typename Host>
class class_level_lockable : public detail::std_atomic_ops {
    static inline std::mutex mtx_;

public:
    class lock {
        std::unique_lock<std::mutex> guard_;
    public:
        lock() : guard_(mtx_) {}
        explicit lock(const class_level_lockable&) : guard_(mtx_) {}
        lock(const lock&) = delete;
        lock& operator=(const lock&) = delete;
    };

    using volatile_type = volatile Host;
};

template <typename Host>
class rw_lockable : public detail::std_atomic_ops {
    mutable std::shared_mutex mtx_;

public:
    class read_lock {
        std::shared_lock<std::shared_mutex> guard_;
    public:
        explicit read_lock(const rw_lockable& host)
            : guard_(host.mtx_) {}
    };

    class write_lock {
        std::unique_lock<std::shared_mutex> guard_;
    public:
        explicit write_lock(const rw_lockable& host)
            : guard_(host.mtx_) {}
    };

    using lock = write_lock;
    using volatile_type = volatile Host;
};

} // namespace loki
