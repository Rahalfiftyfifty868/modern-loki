# functor.hpp

Owning and non-owning type-erased callable wrappers, plus composition helpers.

## API

| Component | Description |
|-----------|-------------|
| `functor<R, Args...>` | Owning callable wrapper over `std::function`. Supports function pointers, lambdas, and member-function-pointer + object. Asserts on empty call; test with `explicit operator bool()`. |
| `functor_ref<R, Args...>` | Non-owning callable reference. No heap allocation. C++20 polyfill for `std::function_ref` (C++26). Blocks rvalue construction for non-trivially-copyable types to prevent dangling. |
| `bind_first(f, bound)` | Partial application: fixes the first argument, returns `functor<R, Rest...>`. |
| `chain(f, g)` | Composition: returns a functor that computes `f(g(args...))`. |
| `make_functor(...)` | Deduction helpers for function pointers, lambdas, `std::function`, and member functions. |
| `Callable<F, R, Args...>` | Concept: `std::invocable<F, Args...>` and result convertible to `R` (or `R` is `void`). |

Note: the template signature is `functor<R, Args...>`, **not** `functor<R(Args...)>`. This differs from `std::function`.

---

### Event callback system (GUI button click handler)

A button stores an owning `functor<void>` so clicks dispatch to any callable.

```cpp
#include <loki/functor.hpp>
#include <iostream>
#include <string>

struct Button {
    std::string label;
    loki::functor<void> on_click;

    void click() const {
        if (on_click) {
            on_click();
        }
    }
};

int main() {
    int count = 0;
    Button ok{"OK", [&count] {
        ++count;
        std::cout << "clicked " << count << "\n";
    }};
    ok.click();
    ok.click();
    return count == 2 ? 0 : 1;
}
```

---

### Signal/slot pattern with functor

A lightweight signal holds a vector of `functor<void, T>` slots; emitting invokes every subscriber.

```cpp
#include <loki/functor.hpp>
#include <iostream>
#include <vector>

template <typename T>
struct Signal {
    std::vector<loki::functor<void, T>> slots;

    void connect(loki::functor<void, T> slot) {
        slots.push_back(std::move(slot));
    }

    void emit(T value) const {
        for (const auto& s : slots) {
            s(value);
        }
    }
};

int main() {
    Signal<int> temperature_changed;
    temperature_changed.connect([](int c) { std::cout << "A: " << c << "C\n"; });
    temperature_changed.connect([](int c) { std::cout << "B: " << c << "C\n"; });
    temperature_changed.emit(21);
    return 0;
}
```

---

### Non-owning callback with functor_ref (hot loop, zero allocation)

A worker accepts a `functor_ref` so per-iteration work runs without allocating. The caller keeps the lambda alive as a named lvalue.

```cpp
#include <loki/functor.hpp>
#include <cstddef>
#include <iostream>

static int sum = 0;

void sum_indices(std::size_t n, const loki::functor_ref<void, std::size_t>& on_iter) {
    for (std::size_t i = 0; i < n; ++i) {
        on_iter(i);
    }
}

int main() {
    auto body = [](std::size_t i) { sum += static_cast<int>(i); };
    sum_indices(100, loki::functor_ref<void, std::size_t>{body});
    std::cout << "sum(0..99) = " << sum << "\n";
    return sum == 4950 ? 0 : 1;
}
```

---

### Member function binding (game entity update method)

`functor` can wrap a member pointer and object so each entity's `update` is stored uniformly without a virtual table.

```cpp
#include <functional>
#include <iostream>
#include <loki/functor.hpp>
#include <string>

struct Entity {
    std::string name;
    int hp = 0;

    void update(int delta_ms) {
        hp += delta_ms > 0 ? 1 : 0;
    }
};

int main() {
    Entity player{"hero", 10};
    loki::functor<void, int> tick{std::ref(player), &Entity::update};
    tick(16);
    std::cout << player.name << " hp=" << player.hp << "\n";
    return player.hp > 10 ? 0 : 1;
}
```

---

### Composing a data pipeline with chain()

`chain(f, g)` builds `f(g(x))`. Nesting chains applies stages from innermost to outermost.

```cpp
#include <loki/functor.hpp>
#include <iostream>
#include <string>

int main() {
    auto parse = loki::make_functor([](std::string raw) { return raw; });

    auto validate = loki::make_functor([](std::string s) -> std::string {
        return s.empty() ? std::string{"default"} : s;
    });

    auto transform = loki::make_functor([](std::string s) {
        return static_cast<int>(s.size());
    });

    auto pipeline = loki::chain(transform, loki::chain(validate, parse));
    int n = pipeline(std::string{"hi"});
    std::cout << "\"hi\" -> length " << n << "\n";
    return n == 2 ? 0 : 1;
}
```

---

### Partial application with bind_first

`bind_first` fixes the first argument of a multi-argument functor. Here a severity level is baked into a logging function.

```cpp
#include <loki/functor.hpp>
#include <iostream>
#include <string>

enum class Level { Info = 1, Error = 3 };

int main() {
    loki::functor<void, Level, const std::string&> log{
        [](Level lv, const std::string& msg) {
            std::cout << static_cast<int>(lv) << ": " << msg << "\n";
        }};

    auto error_only = loki::bind_first(log, Level::Error);
    error_only(std::string{"disk full"});
    return 0;
}
```

---

### Command queue with functor

Deferred work is queued as owning nullary functors and flushed in order.

```cpp
#include <loki/functor.hpp>
#include <iostream>
#include <vector>

int main() {
    std::vector<loki::functor<void>> commands;
    int state = 0;

    commands.push_back([&state] { state += 1; });
    commands.push_back([&state] { state *= 2; });

    for (auto& cmd : commands) {
        cmd();
    }

    std::cout << "state after replay: " << state << "\n";
    return state == 2 ? 0 : 1;
}
```

---

### Strategy pattern (sorting with swappable comparators)

The comparison policy is a `functor<bool, int, int>` injected into `sort`, so behavior changes without editing the sorter.

```cpp
#include <loki/functor.hpp>
#include <algorithm>
#include <iostream>
#include <vector>

void sort_ints(std::vector<int>& v, const loki::functor<bool, int, int>& less) {
    std::sort(v.begin(), v.end(), [&less](int a, int b) { return less(a, b); });
}

int main() {
    std::vector<int> data{3, 1, 4, 1, 5};
    loki::functor<bool, int, int> ascending{[](int a, int b) { return a < b; }};
    sort_ints(data, ascending);
    for (int x : data) std::cout << x << ' ';
    std::cout << '\n';
    return data.front() == 1 && data.back() == 5 ? 0 : 1;
}
```

---

### make_functor from lambda and from member function

`make_functor` deduces a `functor` from a closure or from an object plus a member pointer.

```cpp
#include <loki/functor.hpp>
#include <iostream>
#include <string>

struct Greeter {
    std::string msg;
    std::string speak(const std::string& name) const { return msg + ", " + name; }
};

int main() {
    auto from_lambda = loki::make_functor([](int x) { return x * x; });
    std::cout << "7^2 = " << from_lambda(7) << "\n";

    Greeter g{"Hello"};
    auto from_member = loki::make_functor(g, &Greeter::speak);
    std::cout << from_member(std::string{"Ada"}) << "\n";

    return 0;
}
```

---

### Functor in a thread pool task queue

Tasks are stored as `functor<void>` and drained by a worker thread.

```cpp
#include <loki/functor.hpp>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

struct ThreadPool {
    std::mutex mx;
    std::condition_variable cv;
    std::queue<loki::functor<void>> tasks;
    bool stop = false;

    void submit(loki::functor<void> job) {
        std::lock_guard<std::mutex> lock(mx);
        tasks.push(std::move(job));
        cv.notify_one();
    }

    void worker() {
        std::unique_lock<std::mutex> lock(mx);
        for (;;) {
            cv.wait(lock, [&] { return stop || !tasks.empty(); });
            while (!tasks.empty()) {
                auto job = std::move(tasks.front());
                tasks.pop();
                lock.unlock();
                job();
                lock.lock();
            }
            if (stop) return;
        }
    }
};

int main() {
    ThreadPool pool;
    int result = 0;
    pool.submit([&result] { result = 42; });
    std::thread t([&pool] { pool.worker(); });
    {
        std::lock_guard<std::mutex> lock(pool.mx);
        pool.stop = true;
    }
    pool.cv.notify_all();
    t.join();
    std::cout << "result from worker: " << result << "\n";
    return result == 42 ? 0 : 1;
}
```
