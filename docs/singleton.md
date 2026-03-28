# Singleton system (`singleton.hpp`)

Loki provides a **policy-based singleton** built around `singleton_holder`: creation strategy (heap, PMR, static storage), lifetime and shutdown behavior (atexit ordering, phoenix resurrection, or no teardown), and a threading model for **thread-safe double-checked locking** (`std::atomic` fast path plus the policy lock on the slow path).

## API summary

- **`singleton_holder<T, CreationPolicy, LifetimePolicy, ThreadingModel>`** — Policy-based singleton with thread-safe DCLP. **`instance()`** returns **`T&`**. Default policies: `create_using_new`, `default_lifetime`, `single_threaded`.
- **Creation policies**
  - **`create_using_new<T>`** — `new` / `delete`.
  - **`create_using_pmr<T>`** — Allocates with `std::pmr::polymorphic_allocator<T>`; call **`set_resource(std::pmr::memory_resource*)`** before first use (optional; defaults to `std::pmr::new_delete_resource()`). **`get_resource()`** returns the configured resource.
  - **`create_static<T>`** — Placement-new into a function-local static buffer; **`destroy`** runs the destructor only (no heap free).
- **Lifetime policies**
  - **`default_lifetime<T>`** — Registers destruction via `std::atexit`; **`on_dead_reference`** throws `std::logic_error` if the instance is used after teardown.
  - **`phoenix_singleton<T>`** — Same scheduling as default; **`on_dead_reference`** is a no-op so the instance can be **recreated** after destruction (e.g. during shutdown).
  - **`singleton_with_longevity<T>`** — Uses **`set_longevity`** with a user-supplied longevity (see **`get_longevity`** below) for **ordered destruction** relative to other longevity-tracked objects.
  - **`no_destroy<T>`** — Never schedules destruction; **`on_dead_reference`** does not throw (instance is not torn down by the holder).
- **`set_longevity(T* obj, unsigned int longevity, Destroyer d)`** — Inserts the object into a global ordering used at process exit (higher longevity values are destroyed **before** lower ones in this implementation’s tracker). Overload **`set_longevity(T* obj, unsigned int longevity)`** uses `delete` as the destroyer.
- **`meyers_singleton<T>()`** — Returns a reference to a function-local `static T`; relies on the compiler’s thread-safe static initialization (C++11 onward).
- **Threading models** (see `threads.hpp`): **`single_threaded`** (no mutex on the slow path) or **`class_level_lockable`** (static `std::mutex` for multi-threaded `instance()`). **`object_level_lockable`** is **not** supported by `singleton_holder` (its lock is not default-constructible).

For **`singleton_with_longevity<T>`**, you must supply **`unsigned int get_longevity(T*)`** (typically in the same namespace as `T` for ADL), following the library’s convention for ordering.

---

### Application-wide configuration manager

Use `singleton_holder` with defaults or `class_level_lockable` so any thread can read settings during startup and runtime. Keep `T` default-constructible or initialize from environment inside the constructor.

```cpp
#include <loki/singleton.hpp>
#include <iostream>
#include <string>

struct AppConfig {
    std::string app_name = "MyApp";
    int port = 8080;
};

int main() {
    using Config = loki::singleton_holder<AppConfig, loki::create_using_new,
        loki::default_lifetime, loki::class_level_lockable>;

    auto& cfg = Config::instance();
    std::cout << cfg.app_name << ':' << cfg.port << '\n';
    return 0;
}
```

---

### Logger singleton with `class_level_lockable` for thread safety

Multiple threads may call `instance()` and log concurrently; **`class_level_lockable`** serializes the slow path (first creation) while the published pointer is read via the atomic fast path.

```cpp
#include <loki/singleton.hpp>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

class Logger {
    std::mutex m_;
public:
    void log(std::string_view msg) {
        std::lock_guard<std::mutex> lock(m_);
        std::cout << "[log] " << msg << '\n';
    }
};

int main() {
    using Log = loki::singleton_holder<Logger, loki::create_using_new,
        loki::default_lifetime, loki::class_level_lockable>;

    std::thread a([] { Log::instance().log("worker A"); });
    std::thread b([] { Log::instance().log("worker B"); });
    Log::instance().log("main");
    a.join();
    b.join();
    return 0;
}
```

---

### Database connection pool singleton

A single process-wide pool is a classic singleton: expensive OS or library handles are created once and shared. **`default_lifetime`** registers normal teardown at exit.

```cpp
#include <loki/singleton.hpp>
#include <iostream>
#include <vector>

struct ConnectionPool {
    std::vector<int> fake_handles{1, 2, 3};
    int borrow() {
        int h = fake_handles.back();
        fake_handles.pop_back();
        return h;
    }
    void release(int h) { fake_handles.push_back(h); }
};

int main() {
    using Pool = loki::singleton_holder<ConnectionPool, loki::create_using_new,
        loki::default_lifetime, loki::class_level_lockable>;

    int c = Pool::instance().borrow();
    Pool::instance().release(c);
    std::cout << "done\n";
    return 0;
}
```

---

### PMR-backed singleton (arena allocator for high performance)

Route the singleton’s storage through a **monotonic** or pool resource so allocations for `T` and its members sit in a contiguous arena, improving locality and reducing global allocator traffic. Call **`create_using_pmr<T>::set_resource`** before **`instance()`**.

```cpp
#include <loki/singleton.hpp>
#include <array>
#include <cstddef>
#include <iostream>
#include <memory_resource>
#include <vector>

struct HeavyService {
    std::pmr::vector<int> data{std::pmr::get_default_resource()};
    HeavyService() {
        data.assign(1000u, 42);
    }
};

int main() {
    alignas(std::max_align_t) std::array<std::byte, 64 * 1024> buffer{};
    std::pmr::monotonic_buffer_resource arena(buffer.data(), buffer.size());

    loki::create_using_pmr<HeavyService>::set_resource(&arena);

    using Svc = loki::singleton_holder<HeavyService, loki::create_using_pmr,
        loki::default_lifetime, loki::class_level_lockable>;

    std::cout << Svc::instance().data.size() << '\n';
    return 0;
}
```

---

### Phoenix singleton for resurrection during shutdown

If something may touch the singleton **after** the normal `atexit` destructor has run (ordering bugs, late callbacks), **`phoenix_singleton`** avoids throwing on that “dead reference” and allows a **new** instance to be created instead of `default_lifetime`’s `std::logic_error`.

```cpp
#include <loki/singleton.hpp>
#include <iostream>

struct LateConsumer {
    void ping() { std::cout << "late ping\n"; }
};

int main() {
    using Phoenix = loki::singleton_holder<LateConsumer, loki::create_using_new,
        loki::phoenix_singleton, loki::single_threaded>;

    Phoenix::instance().ping();
    // After process-wide teardown runs, another call could recreate T
    // instead of throwing (illustrative; normal apps fix ordering instead).
    return 0;
}
```

---

### Ordered destruction with `set_longevity` (DB before Logger)

With **`singleton_with_longevity`**, implement **`get_longevity(T*)`** for each singleton type. **Larger** longevity values are torn down **earlier** at exit; **smaller** values **later**. To close the database pool first and flush the logger second, give the pool a **higher** longevity number than the logger.

```cpp
#include <loki/singleton.hpp>
#include <iostream>
#include <string>

namespace app {

struct Database {
    Database() = default;
    ~Database() { std::cout << "DB closed\n"; }
};

struct Logger {
    Logger() = default;
    ~Logger() { std::cout << "Logger flushed\n"; }
};

inline unsigned int get_longevity(Database*) { return 100; }
inline unsigned int get_longevity(Logger*) { return 50; }

} // namespace app

int main() {
    using DB = loki::singleton_holder<app::Database, loki::create_using_new,
        loki::singleton_with_longevity, loki::single_threaded>;
    using Log = loki::singleton_holder<app::Logger, loki::create_using_new,
        loki::singleton_with_longevity, loki::single_threaded>;

    DB::instance();
    Log::instance();
    return 0;
}
```

---

### Meyers singleton for simple cases

When you only need a single static instance and no policies, **`meyers_singleton<T>()`** is minimal and relies on standard thread-safe static initialization.

```cpp
#include <loki/singleton.hpp>
#include <iostream>
#include <string>

struct Counter {
    int n = 0;
    void bump() { ++n; }
};

int main() {
    loki::meyers_singleton<Counter>().bump();
    loki::meyers_singleton<Counter>().bump();
    std::cout << loki::meyers_singleton<Counter>().n << '\n';
    return 0;
}
```

---

### Resource manager singleton (textures, meshes)

Centralize GPU or disk-backed resources so loaders deduplicate by name. **`class_level_lockable`** protects the initial creation path when assets load from worker threads.

```cpp
#include <loki/singleton.hpp>
#include <iostream>
#include <string>
#include <unordered_map>

class ResourceManager {
    std::unordered_map<std::string, int> textures_;
public:
    int load_texture(const std::string& name) {
        auto it = textures_.find(name);
        if (it != textures_.end()) {
            return it->second;
        }
        int id = static_cast<int>(textures_.size() + 1);
        textures_[name] = id;
        return id;
    }
};

int main() {
    using RM = loki::singleton_holder<ResourceManager, loki::create_using_new,
        loki::default_lifetime, loki::class_level_lockable>;

    int a = RM::instance().load_texture("hero.png");
    int b = RM::instance().load_texture("hero.png");
    std::cout << (a == b) << '\n';
    return 0;
}
```

---

### `no_destroy` singleton for process-lifetime objects

Use **`no_destroy`** when the OS reclaims everything on exit and you want to **avoid** static destructor ordering issues entirely (or when leak-sanitizer exceptions are registered). The holder never schedules `atexit` destruction.

```cpp
#include <loki/singleton.hpp>
#include <iostream>

struct ProcessWideCache {
    int hits = 0;
};

int main() {
    using Cache = loki::singleton_holder<ProcessWideCache, loki::create_using_new,
        loki::no_destroy, loki::single_threaded>;

    ++Cache::instance().hits;
    std::cout << Cache::instance().hits << '\n';
    return 0;
}
```

---

### `create_static` for embedded / no-heap environments

**`create_static`** places the object in a **static buffer** inside `create()`; destruction runs **`~T()`** only. Use when dynamic allocation must be avoided (bare metal, certifiable subsets, or deterministic memory regions).

```cpp
#include <loki/singleton.hpp>
#include <iostream>

struct Telemetry {
    int samples = 0;
    void record() { ++samples; }
};

int main() {
    using Tel = loki::singleton_holder<Telemetry, loki::create_static,
        loki::default_lifetime, loki::single_threaded>;

    Tel::instance().record();
    std::cout << Tel::instance().samples << '\n';
    return 0;
}
```
