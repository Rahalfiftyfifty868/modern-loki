# hierarchy_generators.hpp

Compile-time hierarchy generators: build flat or linear inheritance trees from a `typelist`.

## API

| Component | Description |
|-----------|-------------|
| `gen_scatter_hierarchy<typelist<Ts...>, Unit>` | Flat variadic inheritance from `Unit<T>` for each T. Access via `field<T>(obj)`. Rejects duplicate types. |
| `tuple_unit<T>` | Simple holder: `T value{}` with implicit conversion to `T&` / `const T&`. |
| `gen_linear_hierarchy<typelist<Ts...>, Unit, Root>` | Linear chain: `Unit<Head, gen_linear_hierarchy<Tail...>>`, ending in `Root` (default: `empty_type`). |
| `apply_to_all(F&& f, typelist<Ts...>)` | Invoke `f.template operator()<T>()` for each type in the pack. |

---

### Scatter hierarchy as a multi-field struct (Name, Age, Email)

Model a person as one object that inherits one storage slot per logical field. A thin `holder<T>` Unit keeps the access pattern clean.

```cpp
#include <iostream>
#include <string>
#include <loki/hierarchy_generators.hpp>

struct Name  { std::string val; };
struct Age   { int val{}; };
struct Email { std::string val; };

template <typename T>
struct holder {
    T data{};
};

using PersonFields = loki::gen_scatter_hierarchy<
    loki::typelist<Name, Age, Email>, holder>;

int main() {
    PersonFields person;
    loki::field<Name>(person).data.val = "Ada Lovelace";
    loki::field<Age>(person).data.val = 36;
    loki::field<Email>(person).data.val = "ada@example.com";

    std::cout << loki::field<Name>(person).data.val << ", age "
              << loki::field<Age>(person).data.val << '\n';
}
```

---

### ECS-style component storage (Position, Velocity, Health)

Each component is a plain aggregate. `field<T>()` retrieves the right slot by type.

```cpp
#include <iostream>
#include <loki/hierarchy_generators.hpp>

struct Position { float x{}, y{}; };
struct Velocity { float dx{}, dy{}; };
struct Health   { int hp{100}; };

template <typename T>
struct slot { T data{}; };

using Entity = loki::gen_scatter_hierarchy<
    loki::typelist<Position, Velocity, Health>, slot>;

int main() {
    Entity e;
    loki::field<Position>(e).data = {10.f, 20.f};
    loki::field<Velocity>(e).data = {1.f, 0.f};
    loki::field<Health>(e).data.hp = 50;
    std::cout << "pos=(" << loki::field<Position>(e).data.x
              << "," << loki::field<Position>(e).data.y
              << ") hp=" << loki::field<Health>(e).data.hp << '\n';
}
```

---

### Linear hierarchy for middleware chain

List middleware layers in order; the last template parameter is the inner Root handler. Each layer forwards to its base.

```cpp
#include <iostream>
#include <string>
#include <loki/hierarchy_generators.hpp>

struct Request { std::string path; };

struct Handler {
    void handle(Request& r) {
        std::cout << "handled: " << r.path << '\n';
    }
};

template <typename Layer, typename Next>
struct Middleware : Next {
    void handle(Request& r) {
        std::cout << '[' << Layer::name << "] ";
        Next::handle(r);
    }
};

struct Logging   { static constexpr const char* name = "log"; };
struct Auth      { static constexpr const char* name = "auth"; };
struct RateLimit { static constexpr const char* name = "ratelimit"; };

using Chain = loki::gen_linear_hierarchy<
    loki::typelist<Logging, Auth, RateLimit>, Middleware, Handler>;

int main() {
    Chain chain;
    Request req{"/api/data"};
    chain.handle(req);
}
```

---

### apply_to_all for type registration

Drive registration from a typelist: `operator()<T>()` runs once per type.

```cpp
#include <iostream>
#include <typeinfo>
#include <vector>
#include <string>
#include <loki/hierarchy_generators.hpp>

struct Registry {
    std::vector<std::string> ids;

    template <typename T>
    void operator()() {
        ids.push_back(typeid(T).name());
    }
};

struct Apple {};
struct Banana {};
struct Cherry {};

int main() {
    Registry reg;
    loki::apply_to_all(reg, loki::typelist<Apple, Banana, Cherry>{});
    for (const auto& id : reg.ids) {
        std::cout << id << '\n';
    }
}
```

---

### Scatter hierarchy for compile-time reflection

`field<T>()` is a compile-time "property map" from tag type to storage.

```cpp
#include <iostream>
#include <loki/hierarchy_generators.hpp>

struct Id   { int val{}; };
struct Mass { double val{}; };

template <typename T>
struct prop { T data{}; };

using Record = loki::gen_scatter_hierarchy<
    loki::typelist<Id, Mass>, prop>;

template <typename R>
void print(const R& r) {
    std::cout << "id=" << loki::field<Id>(r).data.val
              << ", mass=" << loki::field<Mass>(r).data.val << '\n';
}

int main() {
    Record r;
    loki::field<Id>(r).data.val = 7;
    loki::field<Mass>(r).data.val = 3.14;
    print(r);
}
```

---

### Config object with named fields

Tag types for config keys; reordering in the typelist doesn't break call sites.

```cpp
#include <iostream>
#include <string>
#include <loki/hierarchy_generators.hpp>

struct HostName  { std::string val; };
struct Port      { int val{}; };
struct TimeOutMs { int val{}; };

template <typename T>
struct cfg_field { T data{}; };

using AppConfig = loki::gen_scatter_hierarchy<
    loki::typelist<HostName, Port, TimeOutMs>, cfg_field>;

int main() {
    AppConfig cfg;
    loki::field<HostName>(cfg).data.val = "localhost";
    loki::field<Port>(cfg).data.val = 8080;
    loki::field<TimeOutMs>(cfg).data.val = 5000;
    std::cout << loki::field<HostName>(cfg).data.val
              << ":" << loki::field<Port>(cfg).data.val
              << " timeout=" << loki::field<TimeOutMs>(cfg).data.val << "ms\n";
}
```

---

### Linear hierarchy for layered rendering

Each layer calls `Base::render()` to continue the pipeline. Root performs final compositing.

```cpp
#include <iostream>
#include <loki/hierarchy_generators.hpp>

struct Framebuffer {
    void render() { std::cout << "compositing to framebuffer\n"; }
};

template <typename Layer, typename Base>
struct RenderLayer : Base {
    void render() {
        std::cout << "layer " << Layer::name << ": begin\n";
        Base::render();
        std::cout << "layer " << Layer::name << ": end\n";
    }
};

struct Background { static constexpr const char* name = "background"; };
struct Sprites    { static constexpr const char* name = "sprites"; };
struct UI         { static constexpr const char* name = "ui"; };
struct PostFX     { static constexpr const char* name = "postfx"; };

using Pipeline = loki::gen_linear_hierarchy<
    loki::typelist<Background, Sprites, UI, PostFX>, RenderLayer, Framebuffer>;

int main() {
    Pipeline p;
    p.render();
}
```

---

### Visitor-like iteration with apply_to_all

Emulate a type-list visitor that runs a per-type action without a common runtime interface.

```cpp
#include <iostream>
#include <typeinfo>
#include <loki/hierarchy_generators.hpp>

struct PrintEach {
    template <typename T>
    void operator()() const {
        std::cout << typeid(T).name() << '\n';
    }
};

struct Foo {};
struct Bar {};
struct Baz {};

int main() {
    loki::apply_to_all(PrintEach{}, loki::typelist<Foo, Bar, Baz>{});
}
```

---

### Scatter hierarchy with custom Unit that adds serialize()

Derive your Unit from `tuple_unit<T>` and add methods; the scatter hierarchy aggregates one such base per T.

```cpp
#include <iostream>
#include <sstream>
#include <string>
#include <loki/hierarchy_generators.hpp>

struct Width  { int val{}; };
struct Height { int val{}; };

template <typename T>
struct serial_field : loki::tuple_unit<T> {
    std::string serialize() const {
        std::ostringstream os;
        os << this->value.val;
        return os.str();
    }
};

using BoxDims = loki::gen_scatter_hierarchy<
    loki::typelist<Width, Height>, serial_field>;

int main() {
    BoxDims b;
    loki::field<Width>(b).value.val = 800;
    loki::field<Height>(b).value.val = 600;
    std::cout << "w=" << loki::field<Width>(b).serialize()
              << " h=" << loki::field<Height>(b).serialize() << "\n";
}
```

---

### Linear hierarchy for decorator / wrapper chain

Compose behavior by nesting `Unit<Decorator, Base>`. Root is the undecorated core.

```cpp
#include <iostream>
#include <loki/hierarchy_generators.hpp>

struct Core {
    int value() const { return 42; }
};

template <typename D, typename B>
struct Wrap : B {
    int value() const { return D::adjust(B::value()); }
};

struct PlusOne  { static int adjust(int v) { return v + 1; } };
struct TimesTwo { static int adjust(int v) { return v * 2; } };

using Decorated = loki::gen_linear_hierarchy<
    loki::typelist<PlusOne, TimesTwo>, Wrap, Core>;

int main() {
    Decorated d;
    std::cout << d.value() << '\n';
}
```
