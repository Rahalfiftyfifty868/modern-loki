# threads.hpp

Four threading policy templates of the form `template <typename Host> class ...`. Each provides a nested `lock` type, `volatile_type`, `int_type`, and static atomic helpers.

## API

| Policy | Lock | int_type | Notes |
|--------|------|----------|-------|
| `single_threaded<Host>` | No-op, default-constructible | `int` (plain load/store) | Zero overhead. |
| `object_level_lockable<Host>` | Per-instance `std::mutex`. **Not default-constructible** — needs `const Host&`. | `std::atomic<int>` | Host must inherit the policy. |
| `class_level_lockable<Host>` | Static `std::mutex` shared by all instances. Default-constructible. | `std::atomic<int>` | Works with `singleton_holder`. |
| `rw_lockable<Host>` | Per-instance `std::shared_mutex`. `read_lock` (shared), `write_lock` (exclusive), `using lock = write_lock`. | `std::atomic<int>` | Host must inherit the policy. |

Static helpers on all policies: `atomic_add`, `atomic_subtract`, `atomic_increment`, `atomic_decrement`, `atomic_assign`, `atomic_load`.

---

### Thread-safe counter with class_level_lockable

Many threads increment using the policy's atomic helpers; the class-level mutex is available for guarding other shared state.

```cpp
#include <loki/threads.hpp>
#include <iostream>
#include <thread>
#include <vector>

struct GlobalCounter {
    using Threading = loki::class_level_lockable<GlobalCounter>;
    typename Threading::int_type hits{0};

    void record_hit() { Threading::atomic_increment(hits); }
    int total() const { return Threading::atomic_load(hits); }
};

int main() {
    GlobalCounter c;
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&c] {
            for (int k = 0; k < 1000; ++k) {
                c.record_hit();
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    std::cout << "total hits: " << c.total() << '\n';
    return 0;
}
```

---

### Object-level locked database connection

Each connection owns a mutex; callers take a `lock` bound to that instance so unrelated connections never block each other.

```cpp
#include <loki/threads.hpp>
#include <iostream>
#include <string>

struct DbConnection : loki::object_level_lockable<DbConnection> {
    using Threading = loki::object_level_lockable<DbConnection>;
    std::string dsn;

    explicit DbConnection(std::string d) : dsn(std::move(d)) {}

    std::string query(const char* sql) {
        typename Threading::lock guard(*this);
        (void)sql;
        return "ok:" + dsn;
    }
};

int main() {
    DbConnection a("db1"), b("db2");
    std::cout << a.query("SELECT 1") << ' ' << b.query("SELECT 2") << '\n';
    return 0;
}
```

---

### Single-threaded for zero overhead

When the host is only ever used from one thread, `single_threaded` makes locks no-ops and uses plain `int`.

```cpp
#include <loki/threads.hpp>
#include <iostream>

struct HotPathStats {
    using Threading = loki::single_threaded<HotPathStats>;
    typename Threading::int_type processed{0};

    void on_packet() { Threading::atomic_increment(processed); }
    int count() const { return Threading::atomic_load(processed); }
};

int main() {
    HotPathStats stats;
    for (int i = 0; i < 1'000'000; ++i) {
        stats.on_packet();
    }
    std::cout << "processed: " << stats.count() << '\n';
    return 0;
}
```

---

### Reader-writer lock for shared cache

`read_lock` for concurrent lookups, `write_lock` for mutations. Readers don't block each other.

```cpp
#include <loki/threads.hpp>
#include <iostream>
#include <string>
#include <unordered_map>

struct StringCache : loki::rw_lockable<StringCache> {
    using Threading = loki::rw_lockable<StringCache>;
    std::unordered_map<std::string, std::string> map_;

    std::string get(const std::string& key) const {
        typename Threading::read_lock r(*this);
        auto it = map_.find(key);
        return it == map_.end() ? std::string{} : it->second;
    }

    void put(std::string key, std::string val) {
        typename Threading::write_lock w(*this);
        map_[std::move(key)] = std::move(val);
    }
};

int main() {
    StringCache cache;
    cache.put("user", "alice");
    std::cout << "user=" << cache.get("user") << '\n';
    return 0;
}
```

---

### Atomic operations on lock-free counter

The policy's `atomic_*` helpers update `int_type` without taking a mutex.

```cpp
#include <loki/threads.hpp>
#include <iostream>

struct Metrics {
    using Threading = loki::class_level_lockable<Metrics>;
    typename Threading::int_type errors{0};
    typename Threading::int_type warnings{0};
};

int main() {
    Metrics m;
    Metrics::Threading::atomic_increment(m.errors);
    Metrics::Threading::atomic_add(m.warnings, 3);
    std::cout << "errors=" << Metrics::Threading::atomic_load(m.errors)
              << " warnings=" << Metrics::Threading::atomic_load(m.warnings) << '\n';
    return 0;
}
```

---

### Protecting shared data with class_level_lockable

A process-wide registry serializes access to a non-atomic map with the static mutex.

```cpp
#include <loki/threads.hpp>
#include <iostream>
#include <string>
#include <unordered_map>

struct PluginRegistry {
    using Threading = loki::class_level_lockable<PluginRegistry>;
    static inline std::unordered_map<std::string, int> plugins_;

    static void add(std::string name, int ver) {
        [[maybe_unused]] typename Threading::lock g;
        plugins_[std::move(name)] = ver;
    }

    static int version_of(const std::string& name) {
        [[maybe_unused]] typename Threading::lock g;
        auto it = plugins_.find(name);
        return it == plugins_.end() ? -1 : it->second;
    }
};

int main() {
    PluginRegistry::add("codec", 2);
    std::cout << "codec v" << PluginRegistry::version_of("codec") << '\n';
    return 0;
}
```

---

### Per-object mutex for fine-grained locking

Independent `Account` objects each serialize their own balance updates so one account never waits on unrelated accounts.

```cpp
#include <loki/threads.hpp>
#include <iostream>

struct Account : loki::object_level_lockable<Account> {
    using Threading = loki::object_level_lockable<Account>;
    int balance_cents{0};

    void deposit(int cents) {
        typename Threading::lock g(*this);
        balance_cents += cents;
    }

    int balance() const {
        typename Threading::lock g(*this);
        return balance_cents;
    }
};

int main() {
    Account alice, bob;
    alice.deposit(100);
    bob.deposit(50);
    std::cout << "alice=" << alice.balance() << " bob=" << bob.balance() << '\n';
    return 0;
}
```

---

### Threading policy in a custom container

Parameterize a guarded queue on the threading model so tests use `single_threaded` and production uses `object_level_lockable`.

```cpp
#include <loki/threads.hpp>
#include <deque>
#include <iostream>
#include <optional>
#include <string>

template <typename T, template <typename> class ThreadingModel = loki::object_level_lockable>
class GuardedQueue {
    using Host = GuardedQueue<T, ThreadingModel>;
    ThreadingModel<Host> policy_;
    std::deque<T> items_;

public:
    void push(T x) {
        typename ThreadingModel<Host>::lock lock(policy_);
        items_.push_back(std::move(x));
    }

    std::optional<T> pop() {
        typename ThreadingModel<Host>::lock lock(policy_);
        if (items_.empty()) {
            return std::nullopt;
        }
        T out = std::move(items_.front());
        items_.pop_front();
        return out;
    }
};

int main() {
    GuardedQueue<std::string> q;
    q.push("first");
    q.push("second");
    if (auto v = q.pop()) {
        std::cout << "popped: " << *v << '\n';
    }
    return 0;
}
```

---

### Combining with singleton_holder

`singleton_holder` requires a default-constructible lock; use `class_level_lockable`.

```cpp
#include <loki/singleton.hpp>
#include <loki/threads.hpp>
#include <iostream>
#include <string>

struct AppConfig {
    std::string theme = "dark";
};

int main() {
    using Holder = loki::singleton_holder<
        AppConfig,
        loki::create_using_new,
        loki::default_lifetime,
        loki::class_level_lockable>;

    std::cout << "theme=" << Holder::instance().theme << '\n';
    return 0;
}
```

---

### Read-heavy workload with rw_lockable

Many threads call `snapshot` under shared locks; writers take an exclusive lock to replace the backing string.

```cpp
#include <loki/threads.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

struct ConfigService : loki::rw_lockable<ConfigService> {
    using Threading = loki::rw_lockable<ConfigService>;
    std::string data_ = "v0";
    mutable typename Threading::int_type reads{0};

    std::string snapshot() const {
        typename Threading::read_lock r(*this);
        Threading::atomic_increment(reads);
        return data_;
    }

    void publish(std::string next) {
        typename Threading::write_lock w(*this);
        data_ = std::move(next);
    }
};

int main() {
    ConfigService cfg;
    std::vector<std::thread> readers;
    for (int i = 0; i < 16; ++i) {
        readers.emplace_back([&cfg] {
            for (int k = 0; k < 100; ++k) {
                (void)cfg.snapshot();
            }
        });
    }
    cfg.publish("v1");
    for (auto& t : readers) {
        t.join();
    }
    std::cout << "latest: " << cfg.snapshot()
              << " total_reads: " << ConfigService::Threading::atomic_load(cfg.reads) << '\n';
    return 0;
}
```
