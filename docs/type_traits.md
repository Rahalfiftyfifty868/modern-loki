# type_traits.hpp

Compile-time type queries and helpers that predate or complement `<type_traits>`.

## API

| Component | Description |
|-----------|-------------|
| `type_traits<T>::is_pointer` | `bool` — true if T is a pointer. |
| `type_traits<T>::is_reference` | `bool` — true if T is a reference. |
| `type_traits<T>::is_const` | `bool` — applied to referred-to type if T is a reference. |
| `type_traits<T>::pointed_to_type` | `remove_pointer_t<T>` if pointer; else `void`. |
| `type_traits<T>::referred_type` | `remove_reference_t<T>` if reference; else `T`. |
| `type_traits<T>::non_const_type` | Removes top-level `const`, preserving reference kind. |
| `type_traits<T>::unqualified_type` | Removes both `const` and `volatile`, preserving reference kind. |
| `type_traits<T>::parameter_type` | Pass-by heuristic: small/scalar by value, large by `const T&`. |
| `type2type<T>` | Empty tag struct. Exposes `using original_type = T`. |
| `conversion<T, U>::exists` | `is_convertible_v<T, U>`. |
| `conversion<T, U>::exists_2way` | Both directions convertible. |
| `conversion<T, U>::same_type` | `is_same_v<T, U>`. |

---

### Optimal parameter passing with parameter_type

Small scalars pass by value; large objects pass by `const T&`. References pass through unchanged.

```cpp
#include <loki/type_traits.hpp>
#include <iostream>
#include <type_traits>

template <typename T>
void store(typename loki::type_traits<T>::parameter_type value) {
    using PT = typename loki::type_traits<T>::parameter_type;
    if constexpr (std::is_reference_v<PT>)
        std::cout << "by-ref\n";
    else
        std::cout << "by-val\n";
    (void)value;
}

struct Big { char data[256]; };

int main() {
    int x = 1;
    std::cout << "int&: ";  store<int&>(x);
    std::cout << "int:  ";  store<int>(42);
    Big b{};
    std::cout << "Big:  ";  store<Big>(b);
    return 0;
}
```

---

### Tag dispatch with type2type for factory overloads

Select an implementation by static type without `if constexpr`: overloads take `type2type<T>`.

```cpp
#include <loki/type_traits.hpp>
#include <iostream>
#include <memory>
#include <string>

struct Widget { virtual ~Widget() = default; };
struct Button : Widget {};
struct Label  : Widget {};

template <typename T>
std::unique_ptr<Widget> make_impl(loki::type2type<T>) {
    return std::make_unique<T>();
}

std::unique_ptr<Widget> make(const std::string& kind) {
    if (kind == "button") return make_impl(loki::type2type<Button>{});
    if (kind == "label")  return make_impl(loki::type2type<Label>{});
    return nullptr;
}

int main() {
    auto w = make("button");
    std::cout << "created: " << (w ? "yes" : "no") << "\n";
    auto n = make("unknown");
    std::cout << "unknown: " << (n ? "yes" : "no") << "\n";
    return 0;
}
```

---

### Conversion checking for safe implicit casts

Gate templates on `conversion<From, To>::exists` so you only instantiate paths the language allows.

```cpp
#include <loki/type_traits.hpp>
#include <iostream>
#include <string>

template <typename From, typename To>
To implicit_cast(From value) {
    static_assert(loki::conversion<From, To>::exists, "no implicit conversion");
    return value;
}

int main() {
    double d = implicit_cast<int, double>(7);
    std::string s = implicit_cast<const char*, std::string>("hello");
    std::cout << "int->double: " << d << "\n";
    std::cout << "cstr->string: " << s << "\n";
    return 0;
}
```

---

### Stripping const from template parameters

Use `non_const_type` when you need a mutable view of the same value category.

```cpp
#include <loki/type_traits.hpp>
#include <iostream>

template <typename T>
void bump(typename loki::type_traits<T>::non_const_type x) {
    ++x;
}

int main() {
    int n = 0;
    bump<const int&>(n);
    std::cout << "n after bump: " << n << "\n";
    return n == 1 ? 0 : 1;
}
```

---

### Detecting pointer vs reference types at compile time

Branch generic algorithms on `is_pointer` and `is_reference` to accept only the shapes you can legally dereference or bind.

```cpp
#include <loki/type_traits.hpp>
#include <iostream>
#include <string_view>

template <typename T>
const char* kind() {
    if constexpr (loki::type_traits<T>::is_pointer) return "pointer";
    else if constexpr (loki::type_traits<T>::is_reference) return "reference";
    else return "other";
}

int main() {
    std::cout << "int*: " << kind<int*>() << "\n";
    std::cout << "int&: " << kind<int&>() << "\n";
    std::cout << "int:  " << kind<int>()  << "\n";
    using std::string_view;
    return string_view(kind<int*>()) == "pointer"
        && string_view(kind<int&>()) == "reference"
        && string_view(kind<int>()) == "other"
        ? 0 : 1;
}
```

---

### Using unqualified_type for template specialization

Specialize behavior on the cv-unqualified type while still accepting `const T&` or `volatile T` in the primary template.

```cpp
#include <loki/type_traits.hpp>
#include <iostream>
#include <type_traits>

template <typename T>
struct Op {
    static constexpr bool is_int =
        std::is_same_v<typename loki::type_traits<T>::unqualified_type, int&> ||
        std::is_same_v<typename loki::type_traits<T>::unqualified_type, int>;
};

int main() {
    std::cout << "const volatile int&: " << Op<const volatile int&>::is_int << "\n";
    std::cout << "int:                 " << Op<int>::is_int << "\n";
    std::cout << "double:              " << Op<double>::is_int << "\n";
    return (Op<const volatile int&>::is_int && Op<int>::is_int && !Op<double>::is_int)
        ? 0 : 1;
}
```

---

### Two-way conversion detection for bidirectional adapters

Require round-trip convertibility with `exists_2way` when two systems must map values in both directions.

```cpp
#include <loki/type_traits.hpp>
#include <iostream>

template <typename A, typename B>
struct Adapter {
    static_assert(loki::conversion<A, B>::exists_2way, "A and B must convert both ways");
    static B to_b(A a) { return a; }
    static A to_a(B b) { return b; }
};

int main() {
    Adapter<int, double> ad;
    std::cout << "3 -> double: " << ad.to_b(3) << "\n";
    std::cout << "3.14 -> int: " << ad.to_a(3.14) << "\n";
    return 0;
}
```

---

### pointed_to_type for generic dereferencing logic

`pointed_to_type` names the element type of a pointer; for non-pointers it is `void`, which you can branch on.

```cpp
#include <loki/type_traits.hpp>
#include <iostream>
#include <type_traits>

template <typename T>
auto deref_or_zero(T x) -> std::conditional_t<
    loki::type_traits<T>::is_pointer,
    typename loki::type_traits<T>::pointed_to_type,
    T> {
    if constexpr (loki::type_traits<T>::is_pointer) {
        return *x;
    } else {
        return x;
    }
}

int main() {
    int n = 7;
    int* p = &n;
    std::cout << "deref ptr: " << deref_or_zero(p) << "\n";
    std::cout << "passthru:  " << deref_or_zero(3) << "\n";
    return deref_or_zero(p) == 7 ? 0 : 1;
}
```

---

### type_traits in a serialization framework

Dispatch encoding to different helpers based on whether a type is a pointer, reference, or plain object.

```cpp
#include <loki/type_traits.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>

template <typename T>
std::string describe_for_wire() {
    using Tr = loki::type_traits<T>;
    if constexpr (Tr::is_pointer) {
        return "pointer";
    } else if constexpr (Tr::is_reference) {
        return "reference_to_" + describe_for_wire<typename Tr::referred_type>();
    } else {
        return "value";
    }
}

int main() {
    std::cout << "int*: " << describe_for_wire<int*>() << "\n";
    std::cout << "int&: " << describe_for_wire<int&>() << "\n";
    std::cout << "int:  " << describe_for_wire<int>()  << "\n";

    std::ostringstream oss;
    oss << describe_for_wire<int*>() << '|' << describe_for_wire<int&>() << '|'
        << describe_for_wire<int>();
    return oss.str() == "pointer|reference_to_value|value" ? 0 : 1;
}
```

---

### Combining conversion with static_assert for safe downcasts

Before `static_cast` from base to derived, assert the implicit upcast exists to document valid public inheritance.

```cpp
#include <loki/type_traits.hpp>
#include <iostream>
#include <type_traits>

struct Base { virtual ~Base() = default; };
struct Derived : Base {};

template <typename DerivedPtr, typename BasePtr>
DerivedPtr downcast(BasePtr base) {
    using D = typename std::remove_pointer<DerivedPtr>::type;
    using B = typename std::remove_pointer<BasePtr>::type;
    static_assert(loki::conversion<D*, B*>::exists, "Derived must be accessible as Base");
    return static_cast<DerivedPtr>(base);
}

int main() {
    Derived d;
    Base* b = &d;
    Derived* p = downcast<Derived*>(b);
    std::cout << "downcast ok: " << (p == &d ? "yes" : "no") << "\n";
    return p == &d ? 0 : 1;
}
```
