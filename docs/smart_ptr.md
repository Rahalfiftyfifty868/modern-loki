# smart_ptr.hpp

Policy-based smart pointers that compose **storage**, **ownership**, and **checking** policies.

## API

| Component | Description |
|-----------|-------------|
| `smart_ptr<T, Storage, Ownership, Checking>` | Main smart pointer. Defaults: `default_sp_storage`, `ref_counted`, `assert_check`. |
| `default_sp_storage<T>` | Single object; calls `delete`. |
| `array_sp_storage<T>` | Dynamic array; calls `delete[]`. |
| `ref_counted<P>` | Shared ownership with `use_count()`. |
| `deep_copy<P>` | Value semantics: copies the pointee with `new T(*p)`. Not for polymorphic types. |
| `no_copy<P>` | Unique / move-only ownership. |
| `assert_check<T>` | Asserts on null dereference. |
| `no_check<T>` | No validation (fastest, UB if null). |
| `throw_on_null<T>` | Throws `std::runtime_error` on null dereference. |

---

### Shared ownership with ref_counted (scene graph)

Tree nodes share child links; the last owner triggers deletion.

```cpp
#include <loki/smart_ptr.hpp>
#include <iostream>
#include <string>
#include <vector>

struct SceneNode {
    std::string name;
    std::vector<loki::smart_ptr<SceneNode>> children;
};

int main() {
    loki::smart_ptr<SceneNode> root{new SceneNode{"root"}};
    auto child = loki::smart_ptr<SceneNode>{new SceneNode{"mesh"}};
    root->children.push_back(child);
    loki::smart_ptr<SceneNode> alias = child;
    std::cout << "child: " << alias->name
              << " (refs=" << alias.use_count() << ")\n";
    return alias->name == "mesh" ? 0 : 1;
}
```

---

### Unique ownership with no_copy (file handle)

Move-only smart pointer models a unique resource; ownership transfers explicitly.

```cpp
#include <loki/smart_ptr.hpp>
#include <cstdio>
#include <iostream>
#include <utility>

struct FileOwner {
    FILE* raw{};
    explicit FileOwner(FILE* p) : raw(p) {}
    ~FileOwner() {
        if (raw) std::fclose(raw);
    }
};

int main() {
    using UniqueFile = loki::smart_ptr<FileOwner, loki::default_sp_storage, loki::no_copy>;
    UniqueFile log{new FileOwner{std::fopen("app.log", "w")}};
    UniqueFile moved = std::move(log);
    if (moved) {
        std::fputs("ok\n", moved->raw);
        std::cout << "wrote to app.log\n";
    }
    return 0;
}
```

---

### Deep copy for value semantics (config objects)

Non-polymorphic settings structs are duplicated on copy so each component gets an independent snapshot.

```cpp
#include <loki/smart_ptr.hpp>
#include <iostream>
#include <string>

struct AppConfig {
    std::string theme{"dark"};
    int timeout_ms{5000};
};

int main() {
    using ConfigPtr = loki::smart_ptr<AppConfig, loki::default_sp_storage, loki::deep_copy>;
    ConfigPtr a{new AppConfig{}};
    ConfigPtr b = a;
    b->theme = "light";
    std::cout << "a=" << a->theme << " b=" << b->theme << "\n";
    return (a->theme != b->theme) ? 0 : 1;
}
```

---

### Array storage for dynamic arrays

Use `array_sp_storage` when the pointer came from `new[]` so destruction uses `delete[]`.

```cpp
#include <loki/smart_ptr.hpp>
#include <cstring>
#include <iostream>

int main() {
    loki::smart_ptr<char, loki::array_sp_storage, loki::no_copy> buf{new char[64]{}};
    std::strcpy(buf.get(), "payload");
    std::cout << "buffer: " << buf.get() << "\n";
    return buf.get()[0] == 'p' ? 0 : 1;
}
```

---

### throw_on_null for safety-critical code

Fail fast with an exception instead of UB when a required dependency was not initialized.

```cpp
#include <loki/smart_ptr.hpp>
#include <iostream>
#include <stdexcept>

struct Sensor {
    int read() const { return 42; }
};

void use_sensor(const loki::smart_ptr<Sensor, loki::default_sp_storage, loki::ref_counted,
                                      loki::throw_on_null>& s) {
    (void)(*s).read();
}

int main() {
    try {
        loki::smart_ptr<Sensor, loki::default_sp_storage, loki::ref_counted, loki::throw_on_null> empty{};
        use_sensor(empty);
    } catch (const std::runtime_error& e) {
        std::cout << "caught: " << e.what() << "\n";
        return 0;
    }
    return 1;
}
```

---

### Custom checking policy (logging dereferences)

A custom policy traces accesses for debugging while keeping storage and ownership unchanged.

```cpp
#include <loki/smart_ptr.hpp>
#include <iostream>

template <typename P>
struct log_check {
    static void on_default(const P&) {}
    static void on_init(const P&) {}
    static void on_dereference(const P& ptr) {
        std::cerr << "deref " << (ptr ? "non-null" : "null") << "\n";
    }
};

int main() {
    loki::smart_ptr<int, loki::default_sp_storage, loki::ref_counted, log_check> p{
        new int{7}};
    std::cout << "value=" << *p << "\n";
    return *p == 7 ? 0 : 1;
}
```

---

### Swapping two smart pointers

`swap` exchanges pointees and ownership/checking state without extra allocations.

```cpp
#include <loki/smart_ptr.hpp>
#include <iostream>

int main() {
    loki::smart_ptr<int> a{new int{1}};
    loki::smart_ptr<int> b{new int{2}};
    std::cout << "before: a=" << *a << " b=" << *b << "\n";
    a.swap(b);
    std::cout << "after:  a=" << *a << " b=" << *b << "\n";
    return (*a == 2 && *b == 1) ? 0 : 1;
}
```

---

### Passing smart_ptr by const reference

Functions that only observe the pointee should take `const smart_ptr<T>&` to avoid refcount churn.

```cpp
#include <loki/smart_ptr.hpp>
#include <iostream>

struct Widget {
    int id{};
};

int widget_id(const loki::smart_ptr<Widget>& w) { return w ? w->id : -1; }

int main() {
    loki::smart_ptr<Widget> w{new Widget{99}};
    std::cout << "widget id=" << widget_id(w) << "\n";
    return widget_id(w) == 99 ? 0 : 1;
}
```

---

### ref_counted use_count() diagnostics

Reference count helps debug leaks, accidental extra copies, or unexpected sharing.

```cpp
#include <loki/smart_ptr.hpp>
#include <iostream>

int main() {
    loki::smart_ptr<int> p{new int{0}};
    loki::smart_ptr<int> q = p;
    std::cout << "shared: use_count=" << p.use_count() << '\n';
    q = loki::smart_ptr<int>{};
    std::cout << "after drop: use_count=" << p.use_count() << '\n';
    return 0;
}
```

---

### array_sp_storage and ref_counted for shared arrays

Reference counting pairs with array storage when several subsystems share one heap-allocated buffer.

```cpp
#include <loki/smart_ptr.hpp>
#include <iostream>

int main() {
    loki::smart_ptr<int, loki::array_sp_storage, loki::ref_counted> shared{
        new int[3]{1, 2, 3}};
    loki::smart_ptr<int, loki::array_sp_storage, loki::ref_counted> alias = shared;
    std::cout << "refs=" << alias.use_count()
              << " [1]=" << alias.get()[1] << '\n';
    return (alias.use_count() == 2 && alias.get()[1] == 2) ? 0 : 1;
}
```
