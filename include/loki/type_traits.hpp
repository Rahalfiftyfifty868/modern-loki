#pragma once

#include <type_traits>

namespace loki {

// type2type: tag dispatch helper. No std:: equivalent exists.
template <typename T>
struct type2type {
    using original_type = T;
};

// conversion: bundles three related compile-time queries into one struct.
// std:: has these individually but not as a single reusable bundle.
template <typename T, typename U>
struct conversion {
    static constexpr bool exists     = std::is_convertible_v<T, U>;
    static constexpr bool exists_2way = exists && std::is_convertible_v<U, T>;
    static constexpr bool same_type  = std::is_same_v<T, U>;
};

namespace detail {
template <typename T, bool = std::is_reference_v<T>>
struct parameter_type_helper {
    // References pass through as-is
    using type = T;
};

template <typename T>
struct parameter_type_helper<T, false> {
    using stripped = std::remove_cv_t<T>;
    using type = std::conditional_t<
        (std::is_scalar_v<stripped> || sizeof(stripped) <= sizeof(void*)),
        T,
        const T&
    >;
};
} // namespace detail

template <typename T>
struct type_traits {
    static constexpr bool is_pointer = std::is_pointer_v<T>;
    static constexpr bool is_reference = std::is_reference_v<T>;
    static constexpr bool is_member_pointer = std::is_member_pointer_v<T>;
    static constexpr bool is_const = std::is_const_v<std::remove_reference_t<T>>;
    static constexpr bool is_volatile = std::is_volatile_v<std::remove_reference_t<T>>;

    using pointed_to_type = std::conditional_t<is_pointer,
        std::remove_pointer_t<T>, void>;

    using referred_type = std::conditional_t<is_reference,
        std::remove_reference_t<T>, T>;

    using non_const_type = std::conditional_t<std::is_lvalue_reference_v<T>,
        std::remove_const_t<std::remove_reference_t<T>>&,
        std::conditional_t<std::is_rvalue_reference_v<T>,
            std::remove_const_t<std::remove_reference_t<T>>&&,
            std::remove_const_t<T>>>;
    using non_volatile_type = std::conditional_t<std::is_lvalue_reference_v<T>,
        std::remove_volatile_t<std::remove_reference_t<T>>&,
        std::conditional_t<std::is_rvalue_reference_v<T>,
            std::remove_volatile_t<std::remove_reference_t<T>>&&,
            std::remove_volatile_t<T>>>;
    using unqualified_type = std::conditional_t<std::is_lvalue_reference_v<T>,
        std::remove_cv_t<std::remove_reference_t<T>>&,
        std::conditional_t<std::is_rvalue_reference_v<T>,
            std::remove_cv_t<std::remove_reference_t<T>>&&,
            std::remove_cv_t<T>>>;

    // Optimal parameter passing: references pass through, scalars and small
    // types by value, everything else by const ref. cv-qualifiers on the
    // underlying type are stripped for the size check but preserved in output.
    using parameter_type = typename detail::parameter_type_helper<T>::type;
};

} // namespace loki
