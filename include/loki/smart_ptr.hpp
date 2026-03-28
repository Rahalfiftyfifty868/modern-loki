#pragma once

#include <atomic>
#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace loki {

// Modern smart pointer policies.
// In C++20 std::unique_ptr/std::shared_ptr cover nearly all use cases.
// modern-loki provides policy-based smart pointers for advanced scenarios
// not covered by the standard (custom ownership, checking, storage, etc.).

// ─── Storage Policies ────────────────────────────────────────────────────────

template <typename T>
class default_sp_storage {
public:
    using stored_type = T*;
    using pointer_type = T*;
    using reference_type = T&;

    default_sp_storage() : ptr_(nullptr) {}
    explicit default_sp_storage(stored_type p) : ptr_(p) {}

    void swap(default_sp_storage& rhs) noexcept { std::swap(ptr_, rhs.ptr_); }

    [[nodiscard]] pointer_type get_impl() const { return ptr_; }
    stored_type& get_impl_ref() { return ptr_; }
    const stored_type& get_impl_ref() const { return ptr_; }
    void destroy() { delete ptr_; }

protected:
    ~default_sp_storage() = default;

private:
    stored_type ptr_;
};

template <typename T>
class array_sp_storage {
public:
    using stored_type = T*;
    using pointer_type = T*;
    using reference_type = T&;

    array_sp_storage() : ptr_(nullptr) {}
    explicit array_sp_storage(stored_type p) : ptr_(p) {}

    void swap(array_sp_storage& rhs) noexcept { std::swap(ptr_, rhs.ptr_); }

    [[nodiscard]] pointer_type get_impl() const { return ptr_; }
    stored_type& get_impl_ref() { return ptr_; }
    const stored_type& get_impl_ref() const { return ptr_; }
    void destroy() { delete[] ptr_; }

protected:
    ~array_sp_storage() = default;

private:
    stored_type ptr_;
};

// ─── Ownership Policies ──────────────────────────────────────────────────────

template <typename P>
class deep_copy {
    static_assert(!std::is_polymorphic_v<std::remove_pointer_t<P>>,
        "deep_copy with polymorphic types will slice; use ref_counted or a clone_factory instead");

public:
    deep_copy() = default;

    template <typename U>
    static P clone(const U& val) {
        if (!val) return P{};
        return P{new std::remove_pointer_t<P>(*val)};
    }

    static bool release(const P&) { return true; }

    void swap(deep_copy&) noexcept {}

protected:
    ~deep_copy() = default;
};

template <typename P>
class ref_counted {
    std::atomic<unsigned int>* count_;

public:
    ref_counted() : count_(new std::atomic<unsigned int>(1)) {}
    ref_counted(const ref_counted& rhs) : count_(rhs.count_) {}
    ref_counted& operator=(const ref_counted&) = delete;

    ref_counted(ref_counted&& rhs) noexcept : count_(rhs.count_) {
        rhs.count_ = nullptr;
    }
    ref_counted& operator=(ref_counted&&) = delete;

    P clone(const P& val) {
        count_->fetch_add(1, std::memory_order_relaxed);
        return val;
    }

    bool release(const P&) {
        if (!count_) return false;
        if (count_->fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete count_;
            count_ = nullptr;
            return true;
        }
        return false;
    }

    void swap(ref_counted& rhs) noexcept { std::swap(count_, rhs.count_); }

    [[nodiscard]] unsigned int use_count() const {
        return count_ ? count_->load(std::memory_order_relaxed) : 0;
    }

protected:
    ~ref_counted() = default;
};

template <typename P>
class no_copy {
public:
    no_copy() = default;
    no_copy(const no_copy&) = delete;
    no_copy& operator=(const no_copy&) = delete;
    no_copy(no_copy&&) noexcept = default;
    no_copy& operator=(no_copy&&) noexcept = default;

    static P clone(const P&) = delete;
    static bool release(const P&) { return true; }

    void swap(no_copy&) noexcept {}

protected:
    ~no_copy() = default;
};

// ─── Checking Policies ───────────────────────────────────────────────────────

template <typename T>
struct assert_check {
    static void on_default(const T&) {}
    static void on_init(const T&) {}
    static void on_dereference([[maybe_unused]] const T& ptr) {
        assert(ptr && "dereferencing null smart pointer");
    }
};

template <typename T>
struct no_check {
    static void on_default(const T&) {}
    static void on_init(const T&) {}
    static void on_dereference(const T&) {}
};

template <typename T>
struct throw_on_null {
    static void on_default(const T&) {}
    static void on_init(const T&) {}
    static void on_dereference(const T& ptr) {
        if (!ptr) throw std::runtime_error("null pointer dereference");
    }
};

// ─── Smart Pointer ───────────────────────────────────────────────────────────

template <
    typename T,
    template <typename> class StoragePolicy = default_sp_storage,
    template <typename> class OwnershipPolicy = ref_counted,
    template <typename> class CheckingPolicy = assert_check
>
class smart_ptr
    : public OwnershipPolicy<typename StoragePolicy<T>::pointer_type>
    , public StoragePolicy<T>
    , public CheckingPolicy<typename StoragePolicy<T>::stored_type>
{
    using storage = StoragePolicy<T>;
    using ownership = OwnershipPolicy<typename storage::pointer_type>;
    using checking = CheckingPolicy<typename storage::stored_type>;

public:
    using pointer_type = typename storage::pointer_type;
    using stored_type = typename storage::stored_type;
    using reference_type = typename storage::reference_type;

    smart_ptr() {
        checking::on_default(storage::get_impl());
    }

    explicit smart_ptr(stored_type p) : storage(p) {
        checking::on_init(storage::get_impl());
    }

    smart_ptr(const smart_ptr& rhs)
        : ownership(rhs)
        , storage(ownership::clone(rhs.get_impl()))
        , checking(rhs) {}

    smart_ptr& operator=(const smart_ptr& rhs) {
        if (this != &rhs) {
            smart_ptr temp(rhs);
            swap(temp);
        }
        return *this;
    }

    smart_ptr(smart_ptr&& rhs) noexcept
        : ownership(static_cast<ownership&&>(rhs))
        , storage(static_cast<storage&&>(rhs))
        , checking(static_cast<checking&&>(rhs)) {
        rhs.get_impl_ref() = stored_type{};
    }

    smart_ptr& operator=(smart_ptr&& rhs) noexcept {
        if (this != &rhs) {
            smart_ptr temp(std::move(rhs));
            swap(temp);
        }
        return *this;
    }

    ~smart_ptr() {
        if (ownership::release(storage::get_impl())) {
            storage::destroy();
        }
    }

    void swap(smart_ptr& rhs) noexcept {
        storage::swap(rhs);
        ownership::swap(rhs);
        std::swap(static_cast<checking&>(*this), static_cast<checking&>(rhs));
    }

    [[nodiscard]] pointer_type get() const { return storage::get_impl(); }

    pointer_type operator->() const {
        checking::on_dereference(storage::get_impl());
        return storage::get_impl();
    }

    reference_type operator*() const {
        checking::on_dereference(storage::get_impl());
        return *storage::get_impl();
    }

    explicit operator bool() const { return storage::get_impl() != nullptr; }

    friend bool operator==(const smart_ptr& lhs, const smart_ptr& rhs) {
        return lhs.get() == rhs.get();
    }

    friend auto operator<=>(const smart_ptr& lhs, const smart_ptr& rhs) {
        return lhs.get() <=> rhs.get();
    }
};

} // namespace loki
