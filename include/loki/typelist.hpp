#pragma once

#include <cstddef>
#include <type_traits>

namespace loki {

template <typename... Ts>
struct typelist {};

struct null_type final {};
struct empty_type {};

namespace tl {

// ─── O(1) operations ─────────────────────────────────────────────────────────

// length: sizeof...
template <typename TList> struct length;
template <typename... Ts>
struct length<typelist<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {};
template <typename TList>
inline constexpr std::size_t length_v = length<TList>::value;

// front
template <typename TList> struct front;
template <typename Head, typename... Tail>
struct front<typelist<Head, Tail...>> { using type = Head; };
template <typename TList>
using front_t = typename front<TList>::type;

// pop_front
template <typename TList> struct pop_front;
template <typename Head, typename... Tail>
struct pop_front<typelist<Head, Tail...>> { using type = typelist<Tail...>; };
template <typename TList>
using pop_front_t = typename pop_front<TList>::type;

// push_front
template <typename TList, typename T> struct push_front;
template <typename... Ts, typename T>
struct push_front<typelist<Ts...>, T> { using type = typelist<T, Ts...>; };
template <typename TList, typename T>
using push_front_t = typename push_front<TList, T>::type;

// push_back
template <typename TList, typename T> struct push_back;
template <typename... Ts, typename T>
struct push_back<typelist<Ts...>, T> { using type = typelist<Ts..., T>; };
template <typename TList, typename T>
using push_back_t = typename push_back<TList, T>::type;

// concat
template <typename TL1, typename TL2> struct concat;
template <typename... Ts, typename... Us>
struct concat<typelist<Ts...>, typelist<Us...>> { using type = typelist<Ts..., Us...>; };
template <typename TL1, typename TL2>
using concat_t = typename concat<TL1, TL2>::type;

// ─── type_at: O(N) recursive (no compiler intrinsic available portably) ─────

template <typename TList, std::size_t Index> struct type_at;
template <typename Head, typename... Tail>
struct type_at<typelist<Head, Tail...>, 0> { using type = Head; };
template <typename Head, typename... Tail, std::size_t Index>
struct type_at<typelist<Head, Tail...>, Index> {
    static_assert(Index < sizeof...(Tail) + 1, "typelist index out of range");
    using type = typename type_at<typelist<Tail...>, Index - 1>::type;
};
template <typename TList, std::size_t Index>
using type_at_t = typename type_at<TList, Index>::type;

// ─── contains: O(1) via fold ────────────────────────────────────────────────

template <typename TList, typename T> struct contains;
template <typename... Ts, typename T>
struct contains<typelist<Ts...>, T>
    : std::bool_constant<(std::is_same_v<Ts, T> || ...)> {};
template <typename TList, typename T>
inline constexpr bool contains_v = contains<TList, T>::value;

// ─── index_of: O(N) via fold (flat, no recursion) ──────────────────────────

namespace detail {
template <typename T, typename... Ts>
consteval int index_of_impl() {
    if constexpr (sizeof...(Ts) == 0) {
        return -1;
    } else {
        bool matches[] = { std::is_same_v<T, Ts>... };
        for (int i = 0; i < static_cast<int>(sizeof...(Ts)); ++i) {
            if (matches[i]) return i;
        }
        return -1;
    }
}
} // namespace detail

template <typename TList, typename T> struct index_of;
template <typename... Ts, typename T>
struct index_of<typelist<Ts...>, T>
    : std::integral_constant<int, detail::index_of_impl<T, Ts...>()> {};
template <typename TList, typename T>
inline constexpr int index_of_v = index_of<TList, T>::value;

// ─── transform: O(1) via pack expansion ─────────────────────────────────────

template <typename TList, template <typename> class Func> struct transform;
template <typename... Ts, template <typename> class Func>
struct transform<typelist<Ts...>, Func> {
    using type = typelist<typename Func<Ts>::type...>;
};
template <typename TList, template <typename> class Func>
using transform_t = typename transform<TList, Func>::type;

// ─── erase_all: O(1) via conditional pack expansion ─────────────────────────

namespace detail {
template <typename T, typename U>
using keep_if_not = std::conditional_t<std::is_same_v<T, U>, typelist<>, typelist<T>>;

template <typename... TLists> struct concat_all;
template <> struct concat_all<> { using type = typelist<>; };
template <typename TL> struct concat_all<TL> { using type = TL; };
template <typename TL1, typename TL2, typename... Rest>
struct concat_all<TL1, TL2, Rest...> {
    using type = typename concat_all<concat_t<TL1, TL2>, Rest...>::type;
};
} // namespace detail

template <typename TList, typename T> struct erase_all;
template <typename... Ts, typename T>
struct erase_all<typelist<Ts...>, T> {
    using type = typename detail::concat_all<detail::keep_if_not<Ts, T>...>::type;
};
template <typename TList, typename T>
using erase_all_t = typename erase_all<TList, T>::type;

// ─── erase (first occurrence): still recursive ─────────────────────────────

template <typename TList, typename T> struct erase;
template <typename T>
struct erase<typelist<>, T> { using type = typelist<>; };
template <typename T, typename... Tail>
struct erase<typelist<T, Tail...>, T> { using type = typelist<Tail...>; };
template <typename Head, typename... Tail, typename T>
struct erase<typelist<Head, Tail...>, T> {
    using type = push_front_t<typename erase<typelist<Tail...>, T>::type, Head>;
};
template <typename TList, typename T>
using erase_t = typename erase<TList, T>::type;

// ─── no_duplicates: recursive (inherently order-dependent) ──────────────────

template <typename TList> struct no_duplicates;
template <> struct no_duplicates<typelist<>> { using type = typelist<>; };
template <typename Head, typename... Tail>
struct no_duplicates<typelist<Head, Tail...>> {
private:
    using tail_unique = typename no_duplicates<typelist<Tail...>>::type;
    using tail_erased = typename erase<tail_unique, Head>::type;
public:
    using type = push_front_t<tail_erased, Head>;
};
template <typename TList>
using no_duplicates_t = typename no_duplicates<TList>::type;

// ─── replace / replace_all ──────────────────────────────────────────────────

template <typename TList, typename T, typename U> struct replace;
template <typename T, typename U>
struct replace<typelist<>, T, U> { using type = typelist<>; };
template <typename T, typename... Tail, typename U>
struct replace<typelist<T, Tail...>, T, U> { using type = typelist<U, Tail...>; };
template <typename Head, typename... Tail, typename T, typename U>
struct replace<typelist<Head, Tail...>, T, U> {
    using type = push_front_t<typename replace<typelist<Tail...>, T, U>::type, Head>;
};
template <typename TList, typename T, typename U>
using replace_t = typename replace<TList, T, U>::type;

// replace_all: O(1) via pack expansion
template <typename TList, typename T, typename U> struct replace_all;
template <typename... Ts, typename T, typename U>
struct replace_all<typelist<Ts...>, T, U> {
    using type = typelist<std::conditional_t<std::is_same_v<Ts, T>, U, Ts>...>;
};
template <typename TList, typename T, typename U>
using replace_all_t = typename replace_all<TList, T, U>::type;

// ─── reverse ────────────────────────────────────────────────────────────────

template <typename TList> struct reverse;
template <> struct reverse<typelist<>> { using type = typelist<>; };
template <typename Head, typename... Tail>
struct reverse<typelist<Head, Tail...>> {
    using type = push_back_t<typename reverse<typelist<Tail...>>::type, Head>;
};
template <typename TList>
using reverse_t = typename reverse<TList>::type;

// ─── back: O(1) via index ───────────────────────────────────────────────────

template <typename TList> struct back;
template <typename... Ts>
    requires (sizeof...(Ts) > 0)
struct back<typelist<Ts...>> {
    using type = type_at_t<typelist<Ts...>, sizeof...(Ts) - 1>;
};
template <typename TList>
using back_t = typename back<TList>::type;

// ─── most_derived / derived_to_front ────────────────────────────────────────

template <typename TList, typename Base> struct most_derived;
template <typename Base>
struct most_derived<typelist<>, Base> { using type = Base; };
template <typename Head, typename... Tail, typename Base>
struct most_derived<typelist<Head, Tail...>, Base> {
private:
    using candidate = typename most_derived<typelist<Tail...>, Base>::type;
public:
    using type = std::conditional_t<std::is_base_of_v<candidate, Head>, Head, candidate>;
};
template <typename TList, typename Base>
using most_derived_t = typename most_derived<TList, Base>::type;

template <typename TList> struct derived_to_front;
template <> struct derived_to_front<typelist<>> { using type = typelist<>; };
template <typename Head, typename... Tail>
struct derived_to_front<typelist<Head, Tail...>> {
private:
    using the_most_derived = most_derived_t<typelist<Tail...>, Head>;
    using rest_replaced = replace_t<typelist<Tail...>, the_most_derived, Head>;
    using sorted_tail = typename derived_to_front<rest_replaced>::type;
public:
    using type = push_front_t<sorted_tail, the_most_derived>;
};
template <typename TList>
using derived_to_front_t = typename derived_to_front<TList>::type;

// ─── for_each_type: O(1) via fold ───────────────────────────────────────────

template <typename TList, typename Func> struct for_each_type;
template <typename... Ts, typename Func>
struct for_each_type<typelist<Ts...>, Func> {
    static void apply(Func&& f) {
        (f.template operator()<Ts>(), ...);
    }
};

} // namespace tl

template <typename T, typename TList>
concept InTypelist = tl::contains_v<TList, T>;

} // namespace loki
