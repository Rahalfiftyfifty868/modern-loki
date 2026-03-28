# loki.hpp

Umbrella header that includes every Modern Loki component in one `#include`.

```cpp
#include <loki/loki.hpp>  // pulls in all headers below
```

| Header | Component |
|--------|-----------|
| `typelist.hpp` | `typelist`, `tl::` metafunctions, `InTypelist` concept |
| `type_traits.hpp` | `type_traits<T>` facade, `type2type`, `conversion` |
| `threads.hpp` | `single_threaded`, `object_level_lockable`, `class_level_lockable`, `rw_lockable` |
| `singleton.hpp` | `singleton_holder`, creation/lifetime policies |
| `smart_ptr.hpp` | `smart_ptr`, storage/ownership/checking policies |
| `functor.hpp` | `functor`, `functor_ref`, `bind_first`, `chain`, `make_functor` |
| `factory.hpp` | `factory`, `clone_factory` |
| `abstract_factory.hpp` | `abstract_factory`, `concrete_factory` |
| `visitor.hpp` | `base_visitor`, `visitable`, `cyclic_visitor`, `variant_visit`, `overloaded` |
| `multi_methods.hpp` | `static_dispatcher`, `fn_dispatcher` |
| `small_obj.hpp` | `fixed_allocator`, `small_obj_allocator`, `small_object` |
| `assoc_vector.hpp` | `assoc_vector` (sorted flat map) |
| `hierarchy_generators.hpp` | `gen_scatter_hierarchy`, `gen_linear_hierarchy`, `apply_to_all` |

Use individual headers when you want faster compile times or minimal coupling. Use the umbrella header for quick prototyping or when most of the library is needed.

---

### Quick-start: factory + singleton + smart_ptr

One include gives you the full toolkit. Register shapes in a factory, access it through a singleton, and manage objects with policy-based smart pointers.

```cpp
#include <loki/loki.hpp>
#include <iostream>
#include <string>

struct Shape {
    virtual ~Shape() = default;
    virtual std::string name() const = 0;
};

struct Circle : Shape {
    std::string name() const override { return "circle"; }
};

struct Square : Shape {
    std::string name() const override { return "square"; }
};

struct ShapeRegistry {
    loki::factory<Shape> fac;
    ShapeRegistry() {
        fac.register_type("circle", [] { return std::make_unique<Circle>(); });
        fac.register_type("square", [] { return std::make_unique<Square>(); });
    }
};

using Registry = loki::singleton_holder<ShapeRegistry>;

int main() {
    auto& reg = Registry::instance();
    loki::smart_ptr<Shape> s{reg.fac.create("circle").release()};
    std::cout << s->name() << "\n";
    return 0;
}
```

---

### Typelist-driven plugin system

Combine `typelist`, `for_each_type`, `factory`, and `conversion` to auto-register every plugin type at startup.

```cpp
#include <loki/loki.hpp>
#include <iostream>
#include <string>

struct Plugin {
    virtual ~Plugin() = default;
    virtual std::string id() const = 0;
};

struct Audio : Plugin { std::string id() const override { return "audio"; } };
struct Video : Plugin { std::string id() const override { return "video"; } };
struct Net   : Plugin { std::string id() const override { return "net"; } };

using AllPlugins = loki::typelist<Audio, Video, Net>;

struct AutoReg {
    loki::factory<Plugin>& f;
    template <typename T>
    void operator()() {
        static_assert(loki::conversion<T*, Plugin*>::exists,
                      "T must derive from Plugin");
        f.register_type(T{}.id(), [] { return std::make_unique<T>(); });
    }
};

int main() {
    loki::factory<Plugin> fac;
    loki::tl::for_each_type<AllPlugins, AutoReg>::apply(AutoReg{fac});

    for (auto& name : {"audio", "video", "net"}) {
        auto p = fac.create(name);
        std::cout << p->id() << "\n";
    }
    return 0;
}
```

---

### Abstract factory with threading and small_object

Build a themed UI with `abstract_factory`, allocate widgets through `small_object`, and protect the factory singleton with `class_level_lockable`.

```cpp
#include <loki/loki.hpp>
#include <iostream>
#include <string>

struct Button : loki::small_object<> {
    std::string label;
    virtual ~Button() = default;
};
struct Checkbox : loki::small_object<> {
    bool checked = false;
    virtual ~Checkbox() = default;
};

struct DarkButton : Button {
    DarkButton() { label = "dark_btn"; }
};
struct DarkCheckbox : Checkbox {
    DarkCheckbox() { checked = true; }
};

using UIFactory = loki::abstract_factory<Button, Checkbox>;
using DarkFactory = loki::concrete_factory<UIFactory, DarkButton, DarkCheckbox>;

int main() {
    DarkFactory dark;
    UIFactory& ui = dark;
    auto btn = ui.create<Button>();
    auto chk = ui.create<Checkbox>();
    std::cout << btn->label << " checked=" << chk->checked << "\n";
    return 0;
}
```

---

### Scatter hierarchy config with variant_visit

Combine `gen_scatter_hierarchy` for typed field storage with `variant_visit` for runtime inspection.

```cpp
#include <loki/loki.hpp>
#include <iostream>
#include <string>
#include <variant>

struct Host { std::string val; };
struct Port { int val{}; };

template <typename T>
struct field_holder { T data{}; };

using Config = loki::gen_scatter_hierarchy<
    loki::typelist<Host, Port>, field_holder>;

int main() {
    Config cfg;
    loki::field<Host>(cfg).data.val = "localhost";
    loki::field<Port>(cfg).data.val = 8080;

    using Val = std::variant<std::string, int>;
    Val v1 = loki::field<Host>(cfg).data.val;
    Val v2 = loki::field<Port>(cfg).data.val;

    for (auto& v : {v1, v2}) {
        loki::variant_visit(v,
            [](const std::string& s) { std::cout << "str: " << s << "\n"; },
            [](int i)                { std::cout << "int: " << i << "\n"; }
        );
    }
    return 0;
}
```

---

### Functor + bind_first + chain in one pipeline

Show all three functor composition tools working together with a single include.

```cpp
#include <loki/loki.hpp>
#include <iostream>
#include <string>

int main() {
    auto parse = loki::make_functor([](std::string s) {
        return std::stoi(s);
    });

    auto offset = loki::bind_first(
        loki::functor<int, int, int>{[](int base, int x) { return base + x; }},
        100);

    auto pipeline = loki::chain(offset, parse);
    std::cout << "pipeline(\"42\") = " << pipeline(std::string{"42"}) << "\n";
    return 0;
}
```

---

### Multi-method dispatch with type2type tags

Combine `fn_dispatcher` for double dispatch with `type2type` for per-type configuration.

```cpp
#include <loki/loki.hpp>
#include <iostream>

struct Shape   { virtual ~Shape() = default; };
struct Circle  : Shape {};
struct Rect    : Shape {};

int main() {
    loki::fn_dispatcher<Shape, Shape, std::string> disp;
    disp.add<Circle, Rect>([](Circle&, Rect&) { return std::string{"circle-rect"}; });
    disp.add<Rect, Rect>([](Rect&, Rect&) { return std::string{"rect-rect"}; });

    Circle c;
    Rect r;
    Shape& s1 = c;
    Shape& s2 = r;
    std::cout << disp.go(s1, s2) << "\n";
    std::cout << disp.go(s2, s2) << "\n";
    return 0;
}
```

---

### Reader-writer locked assoc_vector

Pair `rw_lockable` with `assoc_vector` for a concurrent sorted map with reader/writer separation.

```cpp
#include <loki/loki.hpp>
#include <iostream>
#include <string>

struct DnsCache : loki::rw_lockable<DnsCache> {
    using Threading = loki::rw_lockable<DnsCache>;
    loki::assoc_vector<std::string, std::string> entries;

    void put(const std::string& host, std::string ip) {
        typename Threading::write_lock w(*this);
        entries[host] = std::move(ip);
    }

    std::string lookup(const std::string& host) const {
        typename Threading::read_lock r(*this);
        auto it = entries.find(host);
        return it != entries.end() ? it->second : "not found";
    }
};

int main() {
    DnsCache cache;
    cache.put("example.com", "93.184.216.34");
    cache.put("localhost", "127.0.0.1");
    std::cout << "example.com -> " << cache.lookup("example.com") << "\n";
    std::cout << "unknown     -> " << cache.lookup("unknown") << "\n";
    return 0;
}
```

---

### Visitor + smart_ptr + small_object

Traverse a tree of pool-allocated visitable nodes. `small_object` handles allocation, `unique_ptr` holds ownership, and the visitor walks the tree.

```cpp
#include <loki/loki.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

struct Leaf : loki::visitable<Leaf>, loki::small_object<> {
    std::string tag;
    explicit Leaf(std::string t) : tag(std::move(t)) {}
};

struct Composite : loki::visitable<Composite>, loki::small_object<> {
    std::vector<std::unique_ptr<loki::base_visitable<>>> kids;
    void add(std::unique_ptr<loki::base_visitable<>> child) {
        kids.push_back(std::move(child));
    }
};

struct TreePrinter : loki::base_visitor,
                     loki::visitor<Leaf>,
                     loki::visitor<Composite> {
    int depth = 0;
    void visit(Leaf& l) override {
        for (int i = 0; i < depth; ++i) std::cout << "  ";
        std::cout << l.tag << "\n";
    }
    void visit(Composite& c) override {
        for (int i = 0; i < depth; ++i) std::cout << "  ";
        std::cout << "group:\n";
        ++depth;
        for (auto& k : c.kids) k->accept(*this);
        --depth;
    }
};

int main() {
    auto root = std::make_unique<Composite>();
    root->add(std::make_unique<Leaf>("A"));
    root->add(std::make_unique<Leaf>("B"));

    TreePrinter tp;
    root->accept(tp);
    return 0;
}
```

---

### Linear hierarchy middleware with singleton config

Compose a middleware chain via `gen_linear_hierarchy` and read settings from a `singleton_holder`-managed config object.

```cpp
#include <loki/loki.hpp>
#include <iostream>
#include <string>

struct AppConfig {
    bool verbose = true;
};

using Cfg = loki::singleton_holder<AppConfig>;

struct Request { std::string path; };

struct Handler {
    void handle(Request& r) {
        std::cout << "handled " << r.path << "\n";
    }
};

template <typename Layer, typename Next>
struct MW : Next {
    void handle(Request& r) {
        if (Cfg::instance().verbose)
            std::cout << "[" << Layer::tag << "] ";
        Next::handle(r);
    }
};

struct Log  { static constexpr const char* tag = "log"; };
struct Auth { static constexpr const char* tag = "auth"; };

using Stack = loki::gen_linear_hierarchy<
    loki::typelist<Log, Auth>, MW, Handler>;

int main() {
    Stack s;
    Request req{"/api"};
    s.handle(req);
    return 0;
}
```

---

### Clone factory with type_traits parameter optimization

Use `clone_factory` for deep-copying polymorphic objects, and `type_traits<T>::parameter_type` to pass small types by value in a generic wrapper.

```cpp
#include <loki/loki.hpp>
#include <iostream>
#include <memory>
#include <string>

struct Document {
    virtual ~Document() = default;
    virtual std::unique_ptr<Document> clone() const = 0;
    virtual std::string title() const = 0;
};

struct Report : Document {
    std::string name;
    explicit Report(std::string n) : name(std::move(n)) {}
    std::unique_ptr<Document> clone() const override {
        return std::make_unique<Report>(*this);
    }
    std::string title() const override { return name; }
};

template <typename T>
void log_param(typename loki::type_traits<T>::parameter_type val) {
    std::cout << "logged (size " << sizeof(T) << ")\n";
    (void)val;
}

int main() {
    loki::clone_factory<Document> cloner;
    cloner.register_type(
        typeid(Report),
        [](const Document& d) { return d.clone(); });

    Report original{"Q1 Sales"};
    auto copy = cloner.create(original);
    std::cout << "clone: " << copy->title() << "\n";

    log_param<int>(42);
    log_param<std::string>(std::string{"big"});
    return 0;
}
```
