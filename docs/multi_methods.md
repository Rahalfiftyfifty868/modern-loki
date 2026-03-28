# Multi-methods (double dispatch)

Loki’s multi-method utilities route a call to the correct implementation based on the **dynamic types** of two operands (`BaseLhs&`, `BaseRhs&`). This is the classic *double dispatch* problem (e.g. which collision routine applies to a circle meeting a rectangle).

## API overview

- **`static_dispatcher<Executor, BaseLhs, LhsTypes, symmetric, BaseRhs, RhsTypes, ResultType>`** — Resolve pairs at **compile time** by walking `dynamic_cast` chains over the typelists. `Executor` must provide overloads `fire(ConcreteLhs&, ConcreteRhs&)` for handled pairs, and **`on_error()`** when no pair matches. Optional **`symmetric`** swaps argument order for commutative cases when the left type appears after the right in the list.

- **`basic_dispatcher<BaseLhs, BaseRhs, ResultType, CallbackType>`** — **Runtime** table: `std::map<std::pair<std::type_index, std::type_index>, CallbackType>`. Register with **`add<L, R>(callback)`**, unregister with **`remove<L, R>()`**, invoke with **`go(lhs, rhs)`** (throws `std::runtime_error` if missing).

- **`fn_dispatcher<BaseLhs, BaseRhs, ResultType>`** — Wraps `basic_dispatcher` with **`dynamic_cast`** inside stored callables. Add either a **free function pointer** (template **`add<SomeLhs, SomeRhs, &fn, symmetric>()`**) or **`std::function<...>`** with an optional **`symmetric`** flag. **`remove<SomeLhs, SomeRhs, symmetric>()`** removes one or both entries when symmetric.

- **`static_caster<To, From>`**, **`dynamic_caster<To, From>`** — Small **cast policy** helpers: `static To& cast(From&)` vs `dynamic To& cast(From&)`. Use them when you build your own dispatch glue or want a uniform name for the cast strategy.

---

### Collision detection between shape types

Use a `static_dispatcher` so each concrete pair maps to one `fire` overload; unhandled combinations fall through to `on_error()`.

```cpp
#include <loki/multi_methods.hpp>
#include <iostream>

struct Shape {
    virtual ~Shape() = default;
};
struct Circle : Shape {
    double r{};
};
struct Rectangle : Shape {
    double w{}, h{};
};

struct CollisionExecutor {
    bool fire(Circle& a, Circle& b) {
        const double d = 0.0; /* omitted: center distance */
        return d < a.r + b.r;
    }
    bool fire(Circle&, Rectangle&) { return true; }
    bool fire(Rectangle&, Circle&) { return true; }

    template <typename L, typename R>
    bool fire(L&, R&) { return on_error(); }

    bool on_error() { return false; }
};

int main() {
    using Types = loki::typelist<Circle, Rectangle>;
    using Dispatch = loki::static_dispatcher<CollisionExecutor, Shape, Types, false,
                                             Shape, Types, bool>;

    Circle c1, c2;
    Rectangle r;
    CollisionExecutor exec;
    std::cout << std::boolalpha << Dispatch::go(c1, c2, exec) << "\n";
    std::cout << Dispatch::go(c1, r, exec) << "\n";
    return 0;
}
```


---

### Static dispatcher for game entity interactions

`Executor` implements gameplay rules per pair (e.g. player damages enemy); the catch-all template forwards unknown pairs to `on_error()`.

```cpp
#include <loki/multi_methods.hpp>
#include <iostream>
#include <string>

struct Entity {
    virtual ~Entity() = default;
};
struct Player : Entity {};
struct Enemy : Entity {};
struct Npc : Entity {};

struct InteractionExecutor {
    std::string fire(Player&, Enemy&) { return "combat"; }
    std::string fire(Player&, Npc&) { return "dialog"; }

    template <typename L, typename R>
    std::string fire(L&, R&) { return on_error(); }

    std::string on_error() { return "ignore"; }
};

int main() {
    using Types = loki::typelist<Player, Enemy, Npc>;
    using D = loki::static_dispatcher<InteractionExecutor, Entity, Types, false,
                                    Entity, Types, std::string>;

    Player p;
    Enemy e;
    Npc n;
    InteractionExecutor exec;
    std::cout << D::go(p, e, exec) << "\n";
    std::cout << D::go(p, n, exec) << "\n";
    return 0;
}
```


---

### Physics: resolve contact for rigid-body types

Model bodies with a common base and resolve contact in one place; `static_dispatcher` picks the concrete resolver without `if`/`dynamic_cast` ladders in user code.

```cpp
#include <loki/multi_methods.hpp>

struct RigidBody {
    virtual ~RigidBody() = default;
};
struct Box : RigidBody {};
struct Sphere : RigidBody {};
struct Plane : RigidBody {};

struct ContactExecutor {
    void fire(Box&, Box&) { /* accumulate impulses */ }
    void fire(Sphere&, Plane&) { /* normal + penetration */ }

    template <typename L, typename R>
    void fire(L&, R&) { on_error(); }

    void on_error() {}
};

int main() {
    using Types = loki::typelist<Box, Sphere, Plane>;
    using D = loki::static_dispatcher<ContactExecutor, RigidBody, Types, false,
                                      RigidBody, Types, void>;

    Box a, b;
    Sphere s;
    Plane p;
    ContactExecutor exec;
    D::go(a, b, exec);
    D::go(s, p, exec);
    return 0;
}
```


---

### Document element intersection (text vs image)

`basic_dispatcher` stores callbacks keyed by `type_index`; ideal when pairs are registered once at startup.

```cpp
#include <loki/multi_methods.hpp>
#include <iostream>
#include <string>

struct Element {
    virtual ~Element() = default;
};
struct Text : Element {};
struct Image : Element {};

int main() {
    loki::basic_dispatcher<Element, Element, std::string> dispatch;

    dispatch.add<Text, Image>(
        [](Element&, Element&) { return "text-image"; });
    dispatch.add<Image, Image>(
        [](Element&, Element&) { return "image-image"; });

    Text t;
    Image i1, i2;
    std::cout << dispatch.go(t, i1) << "\n";
    std::cout << dispatch.go(i1, i2) << "\n";
    return 0;
}
```


---

### Symmetric dispatch (order of operands should not matter)

With **`fn_dispatcher`** and **`symmetric = true`**, one registration handles both `(L,R)` and `(R,L)` by swapping arguments before calling your function.

```cpp
#include <loki/multi_methods.hpp>
#include <functional>
#include <iostream>
#include <string>

struct Node {
    virtual ~Node() = default;
};
struct Atype : Node {};
struct Btype : Node {};

std::string collide(Atype&, Btype&) { return "symmetric-pair"; }

int main() {
    loki::fn_dispatcher<Node, Node, std::string> d;
    d.add<Atype, Btype>(
        std::function<std::string(Atype&, Btype&)>(collide), true);

    Atype a;
    Btype b;
    std::cout << d.go(a, b) << "\n";
    std::cout << d.go(b, a) << "\n";
    return 0;
}
```


---

### `fn_dispatcher` with lambdas for UI event handling

Runtime registration with **`std::function`** keeps widget types separate while sharing a common `Widget` interface.

```cpp
#include <loki/multi_methods.hpp>
#include <functional>
#include <iostream>
#include <string>

struct Widget {
    virtual ~Widget() = default;
};
struct Button : Widget {};
struct Slider : Widget {};

int main() {
    loki::fn_dispatcher<Widget, Widget, std::string> ui;

    ui.add<Button, Slider>(
        std::function<std::string(Button&, Slider&)>(
            [](Button&, Slider&) { return "drag-from-button"; }));

    Button b;
    Slider s;
    std::cout << ui.go(b, s) << "\n";
    return 0;
}
```


---

### Adding and removing handlers at runtime

`basic_dispatcher` supports **`remove<L,R>()`** so plugins or modes can unregister pairs without rebuilding the table from scratch.

```cpp
#include <loki/multi_methods.hpp>
#include <iostream>

struct GameObject {
    virtual ~GameObject() = default;
};
struct Bullet : GameObject {};
struct Shield : GameObject {};

int main() {
    loki::basic_dispatcher<GameObject, GameObject, int> d;
    d.add<Bullet, Shield>([](GameObject&, GameObject&) { return 10; });

    Bullet x;
    Shield y;
    std::cout << d.go(x, y) << "\n";

    d.remove<Bullet, Shield>();
    try {
        d.go(x, y);
    } catch (const std::runtime_error& e) {
        std::cout << "removed: " << e.what() << "\n";
    }
    return 0;
}
```


---

### Multi-method returning a value (intersection area)

Set **`ResultType`** to the numeric result; both `static_dispatcher` and `fn_dispatcher` propagate it from `fire` or from the stored callable.

```cpp
#include <loki/multi_methods.hpp>
#include <cmath>
#include <iostream>

struct Shape {
    virtual ~Shape() = default;
};
struct Circle : Shape {
    double radius{1.0};
};
struct Square : Shape {
    double side{1.0};
};

double overlap_area(Circle& c, Square& s) {
    (void)c;
    (void)s;
    return 0.5;
}

int main() {
    loki::fn_dispatcher<Shape, Shape, double> d;
    d.add<Circle, Square>(
        std::function<double(Circle&, Square&)>(overlap_area));

    Circle c;
    Square q;
    std::cout << d.go(c, q) << "\n";
    return 0;
}
```


---

### Error handling with `on_error()` as fallback

For **`static_dispatcher`**, implement **`on_error()`** and a template **`fire`** that forwards to it so unknown dynamic pairs do not leave the call undefined.

```cpp
#include <loki/multi_methods.hpp>
#include <iostream>
#include <string>

struct Animal {
    virtual ~Animal() = default;
};
struct Cat : Animal {};
struct Dog : Animal {};

struct PetExecutor {
    std::string fire(Cat&, Dog&) { return "chase"; }

    template <typename L, typename R>
    std::string fire(L&, R&) { return on_error(); }

    std::string on_error() { return "fallback"; }
};

int main() {
    using Types = loki::typelist<Cat, Dog>;
    using D = loki::static_dispatcher<PetExecutor, Animal, Types, false,
                                      Animal, Types, std::string>;

    Cat c;
    Dog d;
    Cat c2;
    PetExecutor exec;
    std::cout << D::go(c, d, exec) << "\n";
    std::cout << D::go(c, c2, exec) << "\n";
    return 0;
}
```


---

### Symmetric remove after symmetric add

Register with **`symmetric: true`** on **`fn_dispatcher::add`**, then **`remove<L, R, true>`** clears both orientations.

```cpp
#include <loki/multi_methods.hpp>
#include <functional>
#include <iostream>

struct Tag {
    virtual ~Tag() = default;
};
struct X : Tag {};
struct Y : Tag {};

int main() {
    loki::fn_dispatcher<Tag, Tag, int> d;
    d.add<X, Y>(
        std::function<int(X&, Y&)>([](X&, Y&) { return 42; }), true);

    X x;
    Y y;
    std::cout << d.go(x, y) << " " << d.go(y, x) << "\n";

    d.remove<X, Y, true>();
    try {
        d.go(x, y);
    } catch (const std::runtime_error&) {
        std::cout << "both directions removed\n";
    }
    return 0;
}
```

---

Cast policies **`loki::static_caster<T, U>`** and **`loki::dynamic_caster<T, U>`** are useful when you write custom wrappers: call `T& u = loki::dynamic_caster<T, U>::cast(ref);` instead of repeating `dynamic_cast` at every site.
