# small_obj.hpp

Chunk-based pooling for small allocations: fixed-size pool, multi-size facade, and a CRTP base class that routes `operator new`/`delete` through a singleton allocator.

## API

| Component | Description |
|-----------|-------------|
| `fixed_allocator(block_size, chunk_size)` | Pool for one block size. `allocate()`, `deallocate(void*)`. Move-only. |
| `small_obj_allocator(chunk_size, max_obj_size)` | One `fixed_allocator` per size 1..max. Falls back to `::operator new` for larger requests. |
| `small_object<Threading, chunk, max>` | Base class: overrides `new`/`delete` via singleton allocator. Default: `single_threaded`, 4096, 256. |
| `default_chunk_size` | 4096 |
| `max_small_object_size` | 256 |

---

### AST nodes inheriting from small_object

Compilers allocate huge numbers of small AST nodes; `small_object` routes them through the pool.

```cpp
#include <loki/small_obj.hpp>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

enum class AstKind { Number, Add, Mul };

struct AstNode : loki::small_object<> {
    AstKind kind{};
    virtual ~AstNode() = default;
};

struct NumberNode : AstNode {
    std::int64_t value{};
    explicit NumberNode(std::int64_t v) : value(v) { kind = AstKind::Number; }
};

struct BinaryNode : AstNode {
    std::unique_ptr<AstNode> left;
    std::unique_ptr<AstNode> right;
    BinaryNode(AstKind k, std::unique_ptr<AstNode> l, std::unique_ptr<AstNode> r)
        : left(std::move(l)), right(std::move(r)) { kind = k; }
};

int main() {
    auto expr = std::make_unique<BinaryNode>(
        AstKind::Add,
        std::make_unique<NumberNode>(40),
        std::make_unique<NumberNode>(2));
    auto* lhs = static_cast<NumberNode*>(expr->left.get());
    auto* rhs = static_cast<NumberNode*>(expr->right.get());
    std::cout << lhs->value << " + " << rhs->value
              << " = " << (lhs->value + rhs->value) << "\n";
    return 0;
}
```

---

### Linked list nodes with pool allocation

Every node has the same size, so a `fixed_allocator` gives a dedicated pool with fast reuse.

```cpp
#include <loki/small_obj.hpp>
#include <cstddef>
#include <iostream>
#include <new>

template <typename T>
struct ListNode {
    T value{};
    ListNode* next = nullptr;
};

template <typename T>
class IntrusiveList {
    loki::fixed_allocator pool_;
public:
    explicit IntrusiveList(std::size_t chunk_bytes = loki::default_chunk_size)
        : pool_(sizeof(ListNode<T>), chunk_bytes) {}

    ListNode<T>* push_front(ListNode<T>* head, const T& v) {
        void* p = pool_.allocate();
        auto* n = new (p) ListNode<T>{v, head};
        return n;
    }

    void free_list(ListNode<T>* head) {
        while (head) {
            ListNode<T>* n = head->next;
            head->~ListNode<T>();
            pool_.deallocate(head);
            head = n;
        }
    }
};

int main() {
    IntrusiveList<int> list;
    ListNode<int>* h = nullptr;
    h = list.push_front(h, 3);
    h = list.push_front(h, 2);
    h = list.push_front(h, 1);
    for (auto* cur = h; cur; cur = cur->next)
        std::cout << cur->value << ' ';
    std::cout << '\n';
    list.free_list(h);
    return 0;
}
```

---

### Event objects in a trading system

Short-lived quote/trade events are small structs; `small_object` keeps hot-path allocations in fixed-size buckets.

```cpp
#include <loki/small_obj.hpp>
#include <cstdint>
#include <iostream>

enum class EventType : std::uint8_t { Quote, Trade, Cancel };

struct MarketEvent : loki::small_object<> {
    EventType type{};
    std::uint64_t seq{};
    std::int64_t price_ticks{};
    std::int32_t qty{};
    virtual ~MarketEvent() = default;
};

struct QuoteEvent : MarketEvent {
    std::int32_t bid_px{};
    std::int32_t ask_px{};
    QuoteEvent() { type = EventType::Quote; }
};

int main() {
    auto* q = new QuoteEvent();
    q->seq = 1;
    q->bid_px = 100;
    q->ask_px = 101;
    std::cout << "quote seq=" << q->seq
              << " bid=" << q->bid_px << " ask=" << q->ask_px << "\n";
    delete q;
    return 0;
}
```

---

### fixed_allocator directly for a custom memory pool

Give a subsystem its own `fixed_allocator` so peak usage and fragmentation stay predictable.

```cpp
#include <loki/small_obj.hpp>
#include <cstddef>
#include <iostream>
#include <vector>

struct Job {
    int id{};
    void (*run)(Job&) = nullptr;
};

class JobPool {
    loki::fixed_allocator mem_;
    std::vector<Job*> free_;
public:
    JobPool() : mem_(sizeof(Job), 2048) {}

    Job* acquire() {
        if (!free_.empty()) {
            Job* j = free_.back();
            free_.pop_back();
            return new (j) Job{};
        }
        void* p = mem_.allocate();
        return new (p) Job{};
    }

    void release(Job* j) {
        j->~Job();
        free_.push_back(j);
    }
};

int main() {
    JobPool pool;
    Job* a = pool.acquire();
    Job* b = pool.acquire();
    a->id = 7;
    b->id = 13;
    std::cout << "job A=" << a->id << " job B=" << b->id << "\n";
    pool.release(a);
    pool.release(b);
    return 0;
}
```

---

### Particle system with small particle objects

Simulations spawn and recycle many tiny records; `small_object` avoids per-object trips through the default allocator.

```cpp
#include <loki/small_obj.hpp>
#include <iostream>
#include <vector>

struct Particle : loki::small_object<> {
    float x = 0.f, y = 0.f;
    float vx = 0.f, vy = 0.f;
    float life = 1.f;
    virtual ~Particle() = default;
};

int main() {
    std::vector<Particle*> alive;
    constexpr int N = 10'000;
    alive.reserve(N);
    for (int i = 0; i < N; ++i) {
        auto* p = new Particle();
        p->x = static_cast<float>(i % 100);
        p->y = static_cast<float>(i / 100);
        alive.push_back(p);
    }
    std::cout << "spawned " << alive.size() << " particles\n";
    std::cout << "last pos=(" << alive.back()->x << "," << alive.back()->y << ")\n";
    for (Particle* p : alive) {
        delete p;
    }
    return 0;
}
```

---

### Game entity components via small_object

ECS-style components under `max_small_object_size` share one allocator policy through a common base.

```cpp
#include <loki/small_obj.hpp>
#include <cstdint>
#include <iostream>

struct Component : loki::small_object<> {
    std::uint32_t entity_id = 0;
    virtual ~Component() = default;
};

struct Transform : Component {
    float x = 0.f, y = 0.f, z = 0.f;
};

struct Velocity : Component {
    float dx = 0.f, dy = 0.f, dz = 0.f;
};

int main() {
    auto* t = new Transform();
    auto* v = new Velocity();
    t->entity_id = 1;
    t->x = 5.f;
    v->entity_id = 1;
    v->dx = 2.f;
    std::cout << "entity " << t->entity_id
              << " pos.x=" << t->x << " vel.dx=" << v->dx << "\n";
    delete t;
    delete v;
    return 0;
}
```

---

### Network packet headers from pool

Fixed-size protocol headers fit a `fixed_allocator` keyed to `sizeof(Header)`.

```cpp
#include <loki/small_obj.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>

#pragma pack(push, 1)
struct NetHeader {
    std::uint16_t len = 0;
    std::uint8_t type = 0;
    std::uint8_t flags = 0;
    std::uint32_t seq = 0;
};
#pragma pack(pop)

class HeaderPool {
    loki::fixed_allocator pool_;
public:
    HeaderPool() : pool_(sizeof(NetHeader), loki::default_chunk_size) {}

    NetHeader* acquire() {
        void* p = pool_.allocate();
        auto* h = new (p) NetHeader{};
        return h;
    }

    void release(NetHeader* h) {
        h->~NetHeader();
        pool_.deallocate(h);
    }
};

int main() {
    HeaderPool pool;
    NetHeader* h = pool.acquire();
    h->len = sizeof(NetHeader);
    h->type = 1;
    h->seq = 42;
    std::cout << "header type=" << static_cast<int>(h->type)
              << " seq=" << h->seq << " len=" << h->len << "\n";
    pool.release(h);
    return 0;
}
```

---

### small_obj_allocator with custom chunk size

Smaller chunks cap worst-case waste per size class while still avoiding `::operator new` for each small request.

```cpp
#include <loki/small_obj.hpp>
#include <cstddef>
#include <cstdint>
#include <iostream>

int main() {
    constexpr std::size_t chunk = 512;
    constexpr std::size_t max_small = 64;
    loki::small_obj_allocator alloc(chunk, max_small);

    void* a = alloc.allocate(24);
    void* b = alloc.allocate(200);
    std::cout << "24-byte alloc: " << (a ? "ok" : "fail")
              << ", 200-byte alloc: " << (b ? "ok" : "fail") << "\n";
    alloc.deallocate(a, 24);
    alloc.deallocate(b, 200);
    return 0;
}
```

---

### Thread-safe small_object with class_level_lockable

When several threads allocate/destroy derived types, `class_level_lockable` guards the singleton allocator with one process-wide mutex.

```cpp
#include <loki/small_obj.hpp>
#include <loki/threads.hpp>
#include <iostream>
#include <thread>
#include <vector>

struct SharedTask : loki::small_object<loki::class_level_lockable> {
    int work = 0;
    virtual ~SharedTask() = default;
};

int main() {
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([t] {
            for (int i = 0; i < 1000; ++i) {
                auto* job = new SharedTask();
                job->work = t + i;
                delete job;
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    std::cout << "4 threads x 1000 alloc/dealloc cycles completed\n";
    return 0;
}
```

---

### fixed_allocator for free-list object pool

Preallocate from a `fixed_allocator`, chain freed instances on an intrusive freelist, and take fresh blocks from the same pool when the freelist empties.

```cpp
#include <loki/small_obj.hpp>
#include <cstddef>
#include <cstdint>
#include <iostream>

struct PooledItem {
    std::uint32_t payload = 0;
    PooledItem* next_free = nullptr;
};

class ObjectPool {
    loki::fixed_allocator blocks_;
    PooledItem* free_ = nullptr;
public:
    ObjectPool() : blocks_(sizeof(PooledItem), 4096) {}

    PooledItem* acquire() {
        if (free_) {
            PooledItem* p = free_;
            free_ = p->next_free;
            return new (p) PooledItem{};
        }
        void* raw = blocks_.allocate();
        return new (raw) PooledItem{};
    }

    void release(PooledItem* p) {
        p->~PooledItem();
        p->next_free = free_;
        free_ = p;
    }
};

int main() {
    ObjectPool pool;
    PooledItem* a = pool.acquire();
    PooledItem* b = pool.acquire();
    a->payload = 1;
    b->payload = 2;
    std::cout << "a=" << a->payload << " b=" << b->payload << "\n";
    pool.release(a);
    PooledItem* c = pool.acquire();
    std::cout << "reused slot: payload=" << c->payload << "\n";
    pool.release(c);
    pool.release(b);
    return 0;
}
```
