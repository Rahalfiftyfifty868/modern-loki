# typelist.hpp

Compile-time typelist and associated metaprogramming utilities. A modern C++20 replacement for the original Loki `Typelist` using variadic templates, fold expressions, and `consteval`.

## API Overview

| Utility | Description |
|---------|-------------|
| `typelist<Ts...>` | Core variadic type container |
| `null_type` | Sentinel type (final, no instances) |
| `empty_type` | Default base / placeholder |
| `tl::length_v<TL>` | Number of types |
| `tl::front_t<TL>` | First type |
| `tl::back_t<TL>` | Last type |
| `tl::pop_front_t<TL>` | Remove first |
| `tl::push_front_t<TL, T>` | Prepend T |
| `tl::push_back_t<TL, T>` | Append T |
| `tl::concat_t<TL1, TL2>` | Concatenate two typelists |
| `tl::type_at_t<TL, I>` | Access by index |
| `tl::contains_v<TL, T>` | Membership check |
| `tl::index_of_v<TL, T>` | Index of T (-1 if absent) |
| `tl::transform_t<TL, F>` | Apply metafunction to each |
| `tl::erase_t<TL, T>` | Remove first occurrence |
| `tl::erase_all_t<TL, T>` | Remove all occurrences |
| `tl::no_duplicates_t<TL>` | Deduplicate |
| `tl::replace_t<TL, T, U>` | Replace first T with U |
| `tl::replace_all_t<TL, T, U>` | Replace all T with U |
| `tl::reverse_t<TL>` | Reverse order |
| `tl::most_derived_t<TL, B>` | Most derived from B |
| `tl::derived_to_front_t<TL>` | Sort by derivation depth |
| `tl::for_each_type<TL, F>` | Runtime callback per type |
| `InTypelist<T, TL>` | Concept: T is in TL |

---

## Examples

### 1. Event System Type Registry

Define all event types as a typelist, then query it at compile time.

```cpp
#include <loki/typelist.hpp>
#include <iostream>

struct KeyDown {};
struct KeyUp {};
struct MouseMove {};
struct WindowResize {};

using events = loki::typelist<KeyDown, KeyUp, MouseMove, WindowResize>;

int main() {
    std::cout << "Total event types: " << loki::tl::length_v<events> << "\n";
    std::cout << "KeyUp index: " << loki::tl::index_of_v<events, KeyUp> << "\n";

    static_assert(loki::tl::contains_v<events, MouseMove>);
    static_assert(!loki::tl::contains_v<events, int>);

    std::cout << "All compile-time checks passed\n";
}
```

---

### 2. Compile-Time Filtering with erase_all

Remove placeholder types (like `void`) from a typelist before processing.

```cpp
#include <loki/typelist.hpp>
#include <iostream>
#include <type_traits>

struct Sensor {};
struct Actuator {};

using raw_types = loki::typelist<Sensor, void, Actuator, void, void>;
using clean = loki::tl::erase_all_t<raw_types, void>;

static_assert(loki::tl::length_v<clean> == 2);
static_assert(std::is_same_v<clean, loki::typelist<Sensor, Actuator>>);

int main() {
    std::cout << "before: " << loki::tl::length_v<raw_types> << " types\n";
    std::cout << "after:  " << loki::tl::length_v<clean> << " types (void removed)\n";
}
```

---

### 3. Convert Typelist to std::variant

Build a `std::variant` from a typelist for runtime polymorphism without inheritance.

```cpp
#include <loki/typelist.hpp>
#include <iostream>
#include <string>
#include <variant>

struct JsonValue {};
struct XmlValue {};
struct CsvValue {};

using formats = loki::typelist<JsonValue, XmlValue, CsvValue>;

template <typename TL> struct to_variant;
template <typename... Ts>
struct to_variant<loki::typelist<Ts...>> {
    using type = std::variant<Ts...>;
};

using format_variant = typename to_variant<formats>::type;

int main() {
    format_variant v = XmlValue{};

    std::visit([](auto& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, JsonValue>)  std::cout << "JSON\n";
        if constexpr (std::is_same_v<T, XmlValue>)   std::cout << "XML\n";
        if constexpr (std::is_same_v<T, CsvValue>)    std::cout << "CSV\n";
    }, v);
}
```

---

### 4. Serialization Type Mapping with transform

Apply a metafunction to convert each domain type to its serialized form.

```cpp
#include <loki/typelist.hpp>
#include <iostream>
#include <type_traits>
#include <typeinfo>
#include <vector>
#include <string>

template <typename T>
struct to_wire {
    using type = std::vector<unsigned char>;
};

template <>
struct to_wire<int> {
    using type = int32_t;
};

template <>
struct to_wire<std::string> {
    using type = std::vector<char>;
};

using domain_types = loki::typelist<int, std::string, double>;
using wire_types = loki::tl::transform_t<domain_types, to_wire>;

static_assert(std::is_same_v<
    loki::tl::type_at_t<wire_types, 0>, int32_t>);
static_assert(std::is_same_v<
    loki::tl::type_at_t<wire_types, 1>, std::vector<char>>);
static_assert(std::is_same_v<
    loki::tl::type_at_t<wire_types, 2>, std::vector<unsigned char>>);

int main() {
    std::cout << "int       -> " << typeid(loki::tl::type_at_t<wire_types, 0>).name() << "\n";
    std::cout << "string    -> " << typeid(loki::tl::type_at_t<wire_types, 1>).name() << "\n";
    std::cout << "double    -> " << typeid(loki::tl::type_at_t<wire_types, 2>).name() << "\n";
    std::cout << "all transforms verified\n";
}
```

---

### 5. Plugin System: Check Supported Types at Compile Time

Use `contains_v` and the `InTypelist` concept to gate plugin registration.

```cpp
#include <loki/typelist.hpp>
#include <iostream>

struct AudioPlugin {};
struct VideoPlugin {};
struct NetworkPlugin {};

using supported = loki::typelist<AudioPlugin, VideoPlugin, NetworkPlugin>;

template <loki::InTypelist<supported> Plugin>
void load_plugin() {
    std::cout << "Loading plugin (size " << sizeof(Plugin) << ")\n";
}

struct UnsupportedPlugin {};

int main() {
    load_plugin<AudioPlugin>();
    load_plugin<VideoPlugin>();
    // load_plugin<UnsupportedPlugin>();  // won't compile: not in supported
}
```

---

### 6. Type-Safe Enum Alternative Using index_of

Map types to integer codes at compile time without manual numbering.

```cpp
#include <loki/typelist.hpp>
#include <iostream>

struct MsgLogin {};
struct MsgLogout {};
struct MsgChat {};
struct MsgPing {};

using message_types = loki::typelist<MsgLogin, MsgLogout, MsgChat, MsgPing>;

template <typename Msg>
constexpr int msg_code = loki::tl::index_of_v<message_types, Msg>;

int main() {
    static_assert(msg_code<MsgLogin>  == 0);
    static_assert(msg_code<MsgLogout> == 1);
    static_assert(msg_code<MsgChat>   == 2);
    static_assert(msg_code<MsgPing>   == 3);

    std::cout << "MsgChat code: " << msg_code<MsgChat> << "\n";
}
```

---

### 7. Register Types in a Factory with for_each_type

Use `for_each_type` to auto-register every type in a typelist at runtime.

```cpp
#include <loki/typelist.hpp>
#include <loki/factory.hpp>
#include <iostream>
#include <string>

struct Shape {
    virtual ~Shape() = default;
    virtual std::string name() const = 0;
};
struct Circle    : Shape { std::string name() const override { return "Circle"; } };
struct Rectangle : Shape { std::string name() const override { return "Rectangle"; } };
struct Triangle  : Shape { std::string name() const override { return "Triangle"; } };

using shape_types = loki::typelist<Circle, Rectangle, Triangle>;

struct Registrar {
    loki::factory<Shape>& f;

    template <typename T>
    void operator()() {
        f.register_type(T{}.name(), [] { return std::make_unique<T>(); });
    }
};

int main() {
    loki::factory<Shape> factory;
    loki::tl::for_each_type<shape_types, Registrar>::apply(Registrar{factory});

    auto s = factory.create("Triangle");
    std::cout << "Created: " << s->name() << "\n";
}
```

---

### 8. Merging Module Typelists with concat

Combine independently defined typelists from separate modules into one.

```cpp
#include <loki/typelist.hpp>
#include <iostream>
#include <type_traits>

struct UserCreated {};
struct UserDeleted {};
struct OrderPlaced {};
struct OrderShipped {};

using user_events  = loki::typelist<UserCreated, UserDeleted>;
using order_events = loki::typelist<OrderPlaced, OrderShipped>;
using all_events   = loki::tl::concat_t<user_events, order_events>;

static_assert(loki::tl::length_v<all_events> == 4);
static_assert(std::is_same_v<
    loki::tl::type_at_t<all_events, 2>, OrderPlaced>);
static_assert(loki::tl::contains_v<all_events, UserDeleted>);

int main() {
    std::cout << "user_events:  " << loki::tl::length_v<user_events> << " types\n";
    std::cout << "order_events: " << loki::tl::length_v<order_events> << " types\n";
    std::cout << "all_events:   " << loki::tl::length_v<all_events> << " types\n";
}
```

---

### 9. Derived-to-Front for Optimal dynamic_cast Chains

Reorder a typelist so that the most-derived types come first, making linear dynamic_cast probes hit the right type sooner.

```cpp
#include <loki/typelist.hpp>
#include <iostream>
#include <type_traits>

struct Animal {};
struct Mammal : Animal {};
struct Dog    : Mammal {};
struct Cat    : Mammal {};

using unordered = loki::typelist<Animal, Dog, Mammal, Cat>;
using ordered   = loki::tl::derived_to_front_t<unordered>;

// Most-derived types bubble to the front
static_assert(loki::tl::length_v<ordered> == 4);
static_assert(loki::tl::contains_v<ordered, Dog>);
static_assert(loki::tl::contains_v<ordered, Cat>);

int main() {
    constexpr auto dog_idx = loki::tl::index_of_v<ordered, Dog>;
    constexpr auto mammal_idx = loki::tl::index_of_v<ordered, Mammal>;
    constexpr auto animal_idx = loki::tl::index_of_v<ordered, Animal>;

    static_assert(dog_idx < mammal_idx, "Dog appears before Mammal");
    static_assert(mammal_idx < animal_idx, "Mammal appears before Animal");
    std::cout << "Dog at " << dog_idx << ", Mammal at " << mammal_idx
              << ", Animal at " << animal_idx << "\n";
}
```

---

### 10. Reversing Parameter Order

Reverse a typelist for metaprogramming pipelines that need LIFO processing.

```cpp
#include <loki/typelist.hpp>
#include <iostream>
#include <type_traits>

struct Stage1 {};
struct Stage2 {};
struct Stage3 {};
struct Stage4 {};

using pipeline = loki::typelist<Stage1, Stage2, Stage3, Stage4>;
using teardown = loki::tl::reverse_t<pipeline>;

static_assert(std::is_same_v<loki::tl::front_t<teardown>, Stage4>);
static_assert(std::is_same_v<loki::tl::back_t<teardown>, Stage1>);
static_assert(std::is_same_v<loki::tl::type_at_t<teardown, 1>, Stage3>);

int main() {
    std::cout << "pipeline length: " << loki::tl::length_v<pipeline> << "\n";
    std::cout << "teardown length: " << loki::tl::length_v<teardown> << "\n";
    std::cout << "front is Stage4: " << std::is_same_v<loki::tl::front_t<teardown>, Stage4> << "\n";
    std::cout << "back is Stage1:  " << std::is_same_v<loki::tl::back_t<teardown>, Stage1> << "\n";
}
```
