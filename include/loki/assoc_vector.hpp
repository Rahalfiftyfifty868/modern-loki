#pragma once

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace loki {

namespace detail {
template <typename, typename = void>
struct has_is_transparent : std::false_type {};
template <typename T>
struct has_is_transparent<T, std::void_t<typename T::is_transparent>> : std::true_type {};
template <typename T>
inline constexpr bool has_is_transparent_v = has_is_transparent<T>::value;
} // namespace detail

// Sorted-vector-based associative container. Always compiled, always tested.
// API-compatible with std::flat_map. Faster than std::map for small-to-medium
// datasets due to cache locality. O(log N) lookup, O(N) insert/erase.
// Supports heterogeneous lookup when Compare has an is_transparent tag.

template <
    typename K,
    typename V,
    typename Compare = std::less<K>,
    typename Allocator = std::allocator<std::pair<K, V>>
>
class assoc_vector {
    static constexpr bool is_transparent = detail::has_is_transparent_v<Compare>;

public:
    using key_type = K;
    using mapped_type = V;
    using value_type = std::pair<K, V>;
    using key_compare = Compare;
    using allocator_type = Allocator;
    using reference = value_type&;
    using const_reference = const value_type&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

private:
    using storage_type = std::vector<value_type, Allocator>;
    storage_type data_;
    Compare comp_;

    struct value_compare_impl {
        Compare comp;
        bool operator()(const value_type& a, const value_type& b) const {
            return comp(a.first, b.first);
        }
        bool operator()(const value_type& a, const key_type& b) const {
            return comp(a.first, b);
        }
        bool operator()(const key_type& a, const value_type& b) const {
            return comp(a, b.first);
        }
        template <typename K2>
            requires is_transparent
        bool operator()(const value_type& a, const K2& b) const {
            return comp(a.first, b);
        }
        template <typename K2>
            requires is_transparent
        bool operator()(const K2& a, const value_type& b) const {
            return comp(a, b.first);
        }
    };

    [[nodiscard]] value_compare_impl value_comp_impl() const {
        return value_compare_impl{comp_};
    }

public:
    using iterator = typename storage_type::iterator;
    using const_iterator = typename storage_type::const_iterator;
    using reverse_iterator = typename storage_type::reverse_iterator;
    using const_reverse_iterator = typename storage_type::const_reverse_iterator;
    using pointer = typename storage_type::pointer;
    using const_pointer = typename storage_type::const_pointer;

    assoc_vector() = default;

    explicit assoc_vector(const Compare& comp, const Allocator& alloc = Allocator())
        : data_(alloc), comp_(comp) {}

    template <std::input_iterator InputIt>
    assoc_vector(InputIt first, InputIt last,
                 const Compare& comp = Compare(),
                 const Allocator& alloc = Allocator())
        : data_(first, last, alloc), comp_(comp)
    {
        std::sort(data_.begin(), data_.end(), value_comp_impl());
        auto last_unique = std::unique(data_.begin(), data_.end(),
            [this](const value_type& a, const value_type& b) {
                return !comp_(a.first, b.first) && !comp_(b.first, a.first);
            });
        data_.erase(last_unique, data_.end());
    }

    assoc_vector(std::initializer_list<value_type> init,
                 const Compare& comp = Compare(),
                 const Allocator& alloc = Allocator())
        : assoc_vector(init.begin(), init.end(), comp, alloc) {}

    // Iterators
    [[nodiscard]] iterator begin() noexcept { return data_.begin(); }
    [[nodiscard]] const_iterator begin() const noexcept { return data_.begin(); }
    [[nodiscard]] const_iterator cbegin() const noexcept { return data_.cbegin(); }
    [[nodiscard]] iterator end() noexcept { return data_.end(); }
    [[nodiscard]] const_iterator end() const noexcept { return data_.end(); }
    [[nodiscard]] const_iterator cend() const noexcept { return data_.cend(); }
    [[nodiscard]] reverse_iterator rbegin() noexcept { return data_.rbegin(); }
    [[nodiscard]] const_reverse_iterator rbegin() const noexcept { return data_.rbegin(); }
    [[nodiscard]] reverse_iterator rend() noexcept { return data_.rend(); }
    [[nodiscard]] const_reverse_iterator rend() const noexcept { return data_.rend(); }

    // Capacity
    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }
    [[nodiscard]] size_type size() const noexcept { return data_.size(); }
    [[nodiscard]] size_type max_size() const noexcept { return data_.max_size(); }
    void reserve(size_type n) { data_.reserve(n); }
    [[nodiscard]] size_type capacity() const noexcept { return data_.capacity(); }

    // Element access
    mapped_type& operator[](const key_type& key) {
        auto it = lower_bound(key);
        if (it == end() || comp_(key, it->first)) {
            it = data_.insert(it, value_type(key, mapped_type{}));
        }
        return it->second;
    }

    mapped_type& operator[](key_type&& key) {
        auto it = lower_bound(key);
        if (it == end() || comp_(key, it->first)) {
            it = data_.insert(it, value_type(std::move(key), mapped_type{}));
        }
        return it->second;
    }

    mapped_type& at(const key_type& key) {
        auto it = find(key);
        if (it == end()) throw std::out_of_range("assoc_vector::at");
        return it->second;
    }

    const mapped_type& at(const key_type& key) const {
        auto it = find(key);
        if (it == end()) throw std::out_of_range("assoc_vector::at");
        return it->second;
    }

    // Modifiers
    std::pair<iterator, bool> insert(const value_type& val) {
        auto it = lower_bound(val.first);
        if (it != end() && !comp_(val.first, it->first)) {
            return {it, false};
        }
        return {data_.insert(it, val), true};
    }

    std::pair<iterator, bool> insert(value_type&& val) {
        auto it = lower_bound(val.first);
        if (it != end() && !comp_(val.first, it->first)) {
            return {it, false};
        }
        return {data_.insert(it, std::move(val)), true};
    }

    template <std::input_iterator InputIt>
    void insert(InputIt first, InputIt last) {
        auto old_size = data_.size();
        data_.insert(data_.end(), first, last);
        auto mid = data_.begin() + static_cast<difference_type>(old_size);
        std::sort(mid, data_.end(), value_comp_impl());
        std::inplace_merge(data_.begin(), mid, data_.end(), value_comp_impl());
        auto new_end = std::unique(data_.begin(), data_.end(),
            [this](const value_type& a, const value_type& b) {
                return !comp_(a.first, b.first) && !comp_(b.first, a.first);
            });
        data_.erase(new_end, data_.end());
    }

    void insert(std::initializer_list<value_type> ilist) {
        insert(ilist.begin(), ilist.end());
    }

    template <typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        value_type val(std::forward<Args>(args)...);
        return insert(std::move(val));
    }

    template <typename... Args>
    std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args) {
        auto it = lower_bound(key);
        if (it != end() && !comp_(key, it->first)) {
            return {it, false};
        }
        return {data_.emplace(it, std::piecewise_construct,
                              std::forward_as_tuple(key),
                              std::forward_as_tuple(std::forward<Args>(args)...)),
                true};
    }

    template <typename... Args>
    std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
        auto it = lower_bound(key);
        if (it != end() && !comp_(key, it->first)) {
            return {it, false};
        }
        return {data_.emplace(it, std::piecewise_construct,
                              std::forward_as_tuple(std::move(key)),
                              std::forward_as_tuple(std::forward<Args>(args)...)),
                true};
    }

    iterator erase(iterator pos) { return data_.erase(pos); }
    iterator erase(const_iterator pos) { return data_.erase(pos); }
    iterator erase(iterator first, iterator last) { return data_.erase(first, last); }

    size_type erase(const key_type& key) {
        auto it = find(key);
        if (it == end()) return 0;
        data_.erase(it);
        return 1;
    }

    template <typename K2>
        requires is_transparent
    size_type erase(const K2& key) {
        auto it = find(key);
        if (it == end()) return 0;
        data_.erase(it);
        return 1;
    }

    void clear() noexcept { data_.clear(); }

    void swap(assoc_vector& other)
        noexcept(std::is_nothrow_swappable_v<Compare>) {
        data_.swap(other.data_);
        std::swap(comp_, other.comp_);
    }

    // Lookup
    [[nodiscard]] iterator find(const key_type& key) {
        auto it = lower_bound(key);
        if (it != end() && !comp_(key, it->first)) return it;
        return end();
    }

    [[nodiscard]] const_iterator find(const key_type& key) const {
        auto it = lower_bound(key);
        if (it != end() && !comp_(key, it->first)) return it;
        return end();
    }

    template <typename K2>
        requires is_transparent
    [[nodiscard]] iterator find(const K2& key) {
        auto it = lower_bound(key);
        if (it != end() && !comp_(key, it->first)) return it;
        return end();
    }

    template <typename K2>
        requires is_transparent
    [[nodiscard]] const_iterator find(const K2& key) const {
        auto it = lower_bound(key);
        if (it != end() && !comp_(key, it->first)) return it;
        return end();
    }

    [[nodiscard]] bool contains(const key_type& key) const {
        return find(key) != end();
    }

    template <typename K2>
        requires is_transparent
    [[nodiscard]] bool contains(const K2& key) const {
        return find(key) != end();
    }

    [[nodiscard]] size_type count(const key_type& key) const {
        return contains(key) ? 1 : 0;
    }

    template <typename K2>
        requires is_transparent
    [[nodiscard]] size_type count(const K2& key) const {
        return contains(key) ? 1 : 0;
    }

    [[nodiscard]] iterator lower_bound(const key_type& key) {
        return std::lower_bound(data_.begin(), data_.end(), key, value_comp_impl());
    }

    [[nodiscard]] const_iterator lower_bound(const key_type& key) const {
        return std::lower_bound(data_.begin(), data_.end(), key, value_comp_impl());
    }

    template <typename K2>
        requires is_transparent
    [[nodiscard]] iterator lower_bound(const K2& key) {
        return std::lower_bound(data_.begin(), data_.end(), key, value_comp_impl());
    }

    template <typename K2>
        requires is_transparent
    [[nodiscard]] const_iterator lower_bound(const K2& key) const {
        return std::lower_bound(data_.begin(), data_.end(), key, value_comp_impl());
    }

    [[nodiscard]] iterator upper_bound(const key_type& key) {
        return std::upper_bound(data_.begin(), data_.end(), key, value_comp_impl());
    }

    [[nodiscard]] const_iterator upper_bound(const key_type& key) const {
        return std::upper_bound(data_.begin(), data_.end(), key, value_comp_impl());
    }

    template <typename K2>
        requires is_transparent
    [[nodiscard]] iterator upper_bound(const K2& key) {
        return std::upper_bound(data_.begin(), data_.end(), key, value_comp_impl());
    }

    template <typename K2>
        requires is_transparent
    [[nodiscard]] const_iterator upper_bound(const K2& key) const {
        return std::upper_bound(data_.begin(), data_.end(), key, value_comp_impl());
    }

    [[nodiscard]] std::pair<iterator, iterator> equal_range(const key_type& key) {
        return std::equal_range(data_.begin(), data_.end(), key, value_comp_impl());
    }

    [[nodiscard]] std::pair<const_iterator, const_iterator>
    equal_range(const key_type& key) const {
        return std::equal_range(data_.begin(), data_.end(), key, value_comp_impl());
    }

    template <typename K2>
        requires is_transparent
    [[nodiscard]] std::pair<iterator, iterator> equal_range(const K2& key) {
        return std::equal_range(data_.begin(), data_.end(), key, value_comp_impl());
    }

    template <typename K2>
        requires is_transparent
    [[nodiscard]] std::pair<const_iterator, const_iterator>
    equal_range(const K2& key) const {
        return std::equal_range(data_.begin(), data_.end(), key, value_comp_impl());
    }

    [[nodiscard]] key_compare key_comp() const { return comp_; }

    friend bool operator==(const assoc_vector& lhs, const assoc_vector& rhs) {
        return lhs.data_ == rhs.data_;
    }

    friend auto operator<=>(const assoc_vector& lhs, const assoc_vector& rhs) {
        return lhs.data_ <=> rhs.data_;
    }
};

template <typename K, typename V, typename C, typename A>
void swap(assoc_vector<K, V, C, A>& lhs, assoc_vector<K, V, C, A>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

} // namespace loki
