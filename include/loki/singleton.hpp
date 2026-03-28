#pragma once

#include "threads.hpp"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <memory_resource>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace loki {

using atexit_pfn_t = void (*)();

namespace detail {

class lifetime_tracker {
public:
    explicit lifetime_tracker(unsigned int x) : longevity_(x) {}
    virtual ~lifetime_tracker() = default;

    [[nodiscard]] unsigned int longevity() const { return longevity_; }

    static bool compare(const lifetime_tracker* lhs, const lifetime_tracker* rhs) {
        return lhs->longevity_ < rhs->longevity_;
    }

private:
    unsigned int longevity_;
};

inline std::vector<lifetime_tracker*>& get_tracker_array() {
    static std::vector<lifetime_tracker*> array;
    return array;
}

template <typename T, typename Destroyer>
class concrete_lifetime_tracker : public lifetime_tracker {
public:
    concrete_lifetime_tracker(T* p, unsigned int longevity, Destroyer d)
        : lifetime_tracker(longevity), tracked_(p), destroyer_(d) {}

    ~concrete_lifetime_tracker() override { destroyer_(tracked_); }

private:
    T* tracked_;
    Destroyer destroyer_;
};

inline void at_exit_fn() {
    auto& arr = get_tracker_array();
    if (!arr.empty()) {
        auto* top = arr.back();
        arr.pop_back();
        delete top;
    }
}

} // namespace detail

template <typename T, typename Destroyer>
void set_longevity(T* obj, unsigned int longevity, Destroyer d) {
    auto& arr = detail::get_tracker_array();
    std::unique_ptr<detail::lifetime_tracker> tracker(
        new detail::concrete_lifetime_tracker<T, Destroyer>(
            obj, longevity, d));

    auto pos = std::upper_bound(
        arr.begin(), arr.end(), tracker.get(),
        detail::lifetime_tracker::compare);
    arr.insert(pos, tracker.get());
    if (std::atexit(detail::at_exit_fn) != 0) {
        arr.erase(std::find(arr.begin(), arr.end(), tracker.get()));
        throw std::runtime_error("std::atexit failed in set_longevity");
    }
    tracker.release();
}

template <typename T>
void set_longevity(T* obj, unsigned int longevity) {
    set_longevity(obj, longevity, [](T* p) { delete p; });
}

// ─── Creation policies ──────────────────────────────────────────────────────

template <typename T>
struct create_using_new {
    static T* create() { return new T; }
    static void destroy(T* p) { delete p; }
};

// PMR creation policy. 1-arg template compatible with singleton_holder.
// Call set_resource() before first use to plug in arenas, pools, etc.
// Defaults to new_delete_resource.
template <typename T>
struct create_using_pmr {
    static void set_resource(std::pmr::memory_resource* mr) {
        resource_.store(mr, std::memory_order_release);
    }
    static std::pmr::memory_resource* get_resource() {
        return resource_.load(std::memory_order_acquire);
    }

    static T* create() {
        auto* mr = get_resource();
        used_resource_.store(mr, std::memory_order_release);
        std::pmr::polymorphic_allocator<T> alloc{mr};
        T* p = alloc.allocate(1);
        try {
            alloc.construct(p);
        } catch (...) {
            alloc.deallocate(p, 1);
            throw;
        }
        return p;
    }
    static void destroy(T* p) {
        auto* mr = used_resource_.load(std::memory_order_acquire);
        std::pmr::polymorphic_allocator<T> alloc{mr};
        std::destroy_at(p);
        alloc.deallocate(p, 1);
    }

private:
    static inline std::atomic<std::pmr::memory_resource*> resource_{
        std::pmr::new_delete_resource()};
    static inline std::atomic<std::pmr::memory_resource*> used_resource_{nullptr};
};

template <typename T>
struct create_static {
    static T* create() {
        alignas(alignof(T)) static unsigned char buf[sizeof(T)];
        return new (buf) T;
    }
    static void destroy(T* p) {
        p->~T();
    }
};

// ─── Lifetime policies ──────────────────────────────────────────────────────

template <typename T>
struct default_lifetime {
    static void schedule_destruction(T*, atexit_pfn_t fun) {
        if (std::atexit(fun) != 0) {
            throw std::runtime_error("std::atexit failed in default_lifetime");
        }
    }
    static void on_dead_reference() {
        throw std::logic_error("dead reference detected");
    }
};

template <typename T>
struct phoenix_singleton {
    static void schedule_destruction(T*, atexit_pfn_t fun) {
        if (std::atexit(fun) != 0) {
            throw std::runtime_error("std::atexit failed in phoenix_singleton");
        }
    }
    static void on_dead_reference() {
    }
};

template <typename T>
struct singleton_with_longevity {
    static void schedule_destruction(T* obj, atexit_pfn_t fun) {
        set_longevity(obj, get_longevity(obj),
            [fun](T*) { fun(); });
    }
    static void on_dead_reference() {
        throw std::logic_error("dead reference detected");
    }
};

template <typename T>
struct no_destroy {
    static void schedule_destruction(T*, atexit_pfn_t) {}
    static void on_dead_reference() {}
};

// ─── SingletonHolder ────────────────────────────────────────────────────────
// Thread-safe via DCLP: std::atomic<T*> for the fast path, ThreadingModel's
// lock for the slow path. single_threaded uses a no-op lock,
// class_level_lockable uses a static std::mutex.

template <
    typename T,
    template <typename> class CreationPolicy = create_using_new,
    template <typename> class LifetimePolicy = default_lifetime,
    template <typename> class ThreadingModel = single_threaded
>
class singleton_holder {
    static_assert(std::is_default_constructible_v<typename ThreadingModel<singleton_holder>::lock>,
        "singleton_holder requires a ThreadingModel whose lock is default-constructible "
        "(object_level_lockable and rw_lockable are not supported; use class_level_lockable or single_threaded)");

public:
    singleton_holder() = delete;
    singleton_holder(const singleton_holder&) = delete;
    singleton_holder& operator=(const singleton_holder&) = delete;

    static T& instance() {
        auto* p = instance_.load(std::memory_order_acquire);
        if (!p) {
            p = make_instance();
        }
        return *p;
    }

private:
    static T* make_instance() {
        typename ThreadingModel<singleton_holder>::lock guard;
        (void)guard;

        auto* p = instance_.load(std::memory_order_relaxed);
        if (!p) {
            if (destroyed_.load(std::memory_order_relaxed)) {
                LifetimePolicy<T>::on_dead_reference();
            }
            p = CreationPolicy<T>::create();
            try {
                LifetimePolicy<T>::schedule_destruction(p, &destroy_singleton);
            } catch (...) {
                CreationPolicy<T>::destroy(p);
                throw;
            }
            destroyed_.store(false, std::memory_order_relaxed);
            instance_.store(p, std::memory_order_release);
        }
        return p;
    }

    static void destroy_singleton() {
        assert(!destroyed_.load(std::memory_order_relaxed));
        CreationPolicy<T>::destroy(instance_.load(std::memory_order_relaxed));
        instance_.store(nullptr, std::memory_order_release);
        destroyed_.store(true, std::memory_order_release);
    }

    static inline std::atomic<T*> instance_{nullptr};
    static inline std::atomic<bool> destroyed_{false};
};

// Convenient Meyers singleton (C++11 guarantees thread safety of static locals)
template <typename T>
T& meyers_singleton() {
    static T instance;
    return instance;
}

} // namespace loki
