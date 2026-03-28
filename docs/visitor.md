# visitor.hpp

Modern visitor pattern implementations: acyclic visitor (RTTI-based), cyclic visitor (closed set), and `std::variant`-based visitation.

## API

| Component | Description |
|-----------|-------------|
| `base_visitor` | Abstract base with virtual destructor. |
| `visitor<T, R>` | Interface: `virtual R visit(T&) = 0`. |
| `multi_visitor<R, Ts...>` | Inherits `visitor<T,R>` for each T. |
| `base_visitable<R>` | Abstract: `virtual R accept(base_visitor&) = 0`; protected `accept_impl`. |
| `visitable<Derived, R>` | CRTP helper: auto-implements `accept`. Inherits `base_visitable<R>`. |
| `cyclic_visitor<R, Ts...>` | Closed-set visitor with `generic_visit<T>()`. No RTTI. |
| `overloaded<Fs...>` | Aggregate inheriting all Fs with `using Fs::operator()...`. |
| `variant_visit(var, visitors...)` | Wraps `std::visit` with `overloaded`. |

---

### 1. AST Visitor -- Evaluate an Expression Tree

Use the acyclic visitor to walk an AST and compute a result. Concrete nodes inherit `base_visitable<R>` and implement `accept` via `accept_impl`.

```cpp
#include <loki/visitor.hpp>
#include <iostream>
#include <memory>

struct Expr {
    virtual double accept(loki::base_visitor& v) = 0;
    virtual ~Expr() = default;
};

struct Num : Expr {
    double value;
    explicit Num(double v) : value(v) {}
    double accept(loki::base_visitor& guest) override {
        if (auto* p = dynamic_cast<loki::visitor<Num, double>*>(&guest))
            return p->visit(*this);
        return 0.0;
    }
};

struct Add : Expr {
    std::unique_ptr<Expr> left, right;
    Add(std::unique_ptr<Expr> l, std::unique_ptr<Expr> r)
        : left(std::move(l)), right(std::move(r)) {}
    double accept(loki::base_visitor& guest) override {
        if (auto* p = dynamic_cast<loki::visitor<Add, double>*>(&guest))
            return p->visit(*this);
        return 0.0;
    }
};

struct Evaluator : loki::base_visitor,
                   loki::visitor<Num, double>,
                   loki::visitor<Add, double> {
    double visit(Num& n) override { return n.value; }
    double visit(Add& a) override {
        return a.left->accept(*this) + a.right->accept(*this);
    }
};

int main() {
    auto expr = std::make_unique<Add>(
        std::make_unique<Num>(3.0),
        std::make_unique<Num>(4.0));

    Evaluator eval;
    double result = expr->accept(eval);
    std::cout << "3 + 4 = " << result << "\n";
}
```

---

### 2. Document Element Visitor -- Serialize to HTML

Visit document elements to produce HTML output. Each element uses `visitable<Derived>` as its sole base.

```cpp
#include <loki/visitor.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

struct Paragraph : loki::visitable<Paragraph> {
    std::string text;
    explicit Paragraph(std::string t) : text(std::move(t)) {}
};

struct Heading : loki::visitable<Heading> {
    std::string text;
    int level;
    Heading(int lvl, std::string t) : text(std::move(t)), level(lvl) {}
};

struct Image : loki::visitable<Image> {
    std::string src;
    explicit Image(std::string s) : src(std::move(s)) {}
};

struct HtmlSerializer : loki::base_visitor,
                        loki::visitor<Paragraph>,
                        loki::visitor<Heading>,
                        loki::visitor<Image> {
    void visit(Paragraph& p) override {
        std::cout << "<p>" << p.text << "</p>\n";
    }
    void visit(Heading& h) override {
        std::cout << "<h" << h.level << ">" << h.text
                  << "</h" << h.level << ">\n";
    }
    void visit(Image& img) override {
        std::cout << "<img src=\"" << img.src << "\"/>\n";
    }
};

int main() {
    std::vector<std::unique_ptr<loki::base_visitable<>>> doc;
    doc.push_back(std::make_unique<Heading>(1, "Hello"));
    doc.push_back(std::make_unique<Paragraph>("Welcome to the page."));
    doc.push_back(std::make_unique<Image>("photo.png"));

    HtmlSerializer html;
    for (auto& elem : doc) elem->accept(html);
}
```

---

### 3. Multi-Visitor for Shape Rendering

Use `multi_visitor` to handle multiple shape types in a single visitor class.

```cpp
#include <loki/visitor.hpp>
#include <iostream>
#include <memory>
#include <vector>

struct Circle    : loki::visitable<Circle>    { double radius = 5.0; };
struct Rectangle : loki::visitable<Rectangle> { double w = 10, h = 20; };
struct Triangle  : loki::visitable<Triangle>  { double base = 8, height = 6; };

struct Renderer : loki::base_visitor,
                  loki::multi_visitor<void, Circle, Rectangle, Triangle> {
    void visit(Circle& c) override {
        std::cout << "Draw circle r=" << c.radius << "\n";
    }
    void visit(Rectangle& r) override {
        std::cout << "Draw rect " << r.w << "x" << r.h << "\n";
    }
    void visit(Triangle& t) override {
        std::cout << "Draw triangle base=" << t.base << "\n";
    }
};

int main() {
    std::vector<std::unique_ptr<loki::base_visitable<>>> shapes;
    shapes.push_back(std::make_unique<Circle>());
    shapes.push_back(std::make_unique<Rectangle>());
    shapes.push_back(std::make_unique<Triangle>());

    Renderer renderer;
    for (auto& s : shapes) s->accept(renderer);
}
```

---

### 4. Cyclic Visitor for Game Entities

When all entity types are known at compile time, `cyclic_visitor` avoids RTTI overhead.

```cpp
#include <loki/visitor.hpp>
#include <iostream>

struct Player { int hp = 100; };
struct Enemy  { int damage = 25; };
struct NPC    { std::string dialog = "Hello!"; };

struct GameVisitor : loki::cyclic_visitor<void, Player, Enemy, NPC> {
    void visit(Player& p) override {
        std::cout << "Player HP: " << p.hp << "\n";
    }
    void visit(Enemy& e) override {
        std::cout << "Enemy DMG: " << e.damage << "\n";
    }
    void visit(NPC& n) override {
        std::cout << "NPC says: " << n.dialog << "\n";
    }
};

int main() {
    GameVisitor gv;
    Player p;
    Enemy e;
    NPC n;

    gv.generic_visit(p);
    gv.generic_visit(e);
    gv.generic_visit(n);
}
```

---

### 5. Variant-Based Calculator

Use `variant_visit` for a simple calculator that handles multiple operand types.

```cpp
#include <loki/visitor.hpp>
#include <iostream>
#include <string>
#include <variant>

using Value = std::variant<int, double, std::string>;

void print_value(const Value& v) {
    loki::variant_visit(v,
        [](int i)                { std::cout << "int: " << i << "\n"; },
        [](double d)             { std::cout << "double: " << d << "\n"; },
        [](const std::string& s) { std::cout << "string: " << s << "\n"; }
    );
}

Value add_values(const Value& a, const Value& b) {
    return loki::variant_visit(a,
        [&](int ai) -> Value {
            return loki::variant_visit(b,
                [&](int bi) -> Value              { return ai + bi; },
                [&](double bd) -> Value            { return ai + bd; },
                [&](const std::string&) -> Value   { return std::string("error"); }
            );
        },
        [&](double ad) -> Value {
            return loki::variant_visit(b,
                [&](int bi) -> Value              { return ad + bi; },
                [&](double bd) -> Value            { return ad + bd; },
                [&](const std::string&) -> Value   { return std::string("error"); }
            );
        },
        [&](const std::string& as) -> Value {
            return loki::variant_visit(b,
                [&](int) -> Value                  { return std::string("error"); },
                [&](double) -> Value               { return std::string("error"); },
                [&](const std::string& bs) -> Value { return as + bs; }
            );
        }
    );
}

int main() {
    print_value(42);
    print_value(3.14);
    print_value(std::string("hello"));

    auto result = add_values(Value{10}, Value{20});
    print_value(result);
}
```

---

### 6. Overloaded Pattern for Error Handling

Use `overloaded` with `std::variant` to pattern-match different error types.

```cpp
#include <loki/visitor.hpp>
#include <iostream>
#include <string>
#include <variant>

struct NetworkError { int code; };
struct ParseError   { std::string detail; };
struct AuthError    { std::string user; };

using AppError = std::variant<NetworkError, ParseError, AuthError>;

void handle_error(const AppError& err) {
    std::visit(loki::overloaded{
        [](const NetworkError& e) {
            std::cout << "Network error " << e.code << ": retrying...\n";
        },
        [](const ParseError& e) {
            std::cout << "Parse error: " << e.detail << "\n";
        },
        [](const AuthError& e) {
            std::cout << "Auth failed for user: " << e.user << "\n";
        }
    }, err);
}

int main() {
    handle_error(NetworkError{503});
    handle_error(ParseError{"unexpected token at line 42"});
    handle_error(AuthError{"alice"});
}
```

---

### 7. Visitor Returning a Value -- Compute Shape Area

Use a return-type-bearing visitor to compute areas. Shapes use `visitable<T, double>`.

```cpp
#define _USE_MATH_DEFINES
#include <cmath>
#include <loki/visitor.hpp>
#include <iostream>
#include <memory>
#include <vector>

struct Circle : loki::visitable<Circle, double> {
    double r;
    explicit Circle(double r) : r(r) {}
};

struct Rect : loki::visitable<Rect, double> {
    double w, h;
    Rect(double w, double h) : w(w), h(h) {}
};

struct AreaCalc : loki::base_visitor,
                  loki::visitor<Circle, double>,
                  loki::visitor<Rect, double> {
    double visit(Circle& c) override { return M_PI * c.r * c.r; }
    double visit(Rect& r) override   { return r.w * r.h; }
};

int main() {
    std::vector<std::unique_ptr<loki::base_visitable<double>>> shapes;
    shapes.push_back(std::make_unique<Circle>(5.0));
    shapes.push_back(std::make_unique<Rect>(3.0, 4.0));

    AreaCalc calc;
    for (auto& s : shapes) {
        std::cout << "Area: " << s->accept(calc) << "\n";
    }
}
```

---

### 8. CRTP Visitable Base Class

`visitable<Derived>` removes the need for manually writing `accept()`.

```cpp
#include <loki/visitor.hpp>
#include <iostream>
#include <memory>

struct Button : loki::visitable<Button> {
    std::string label = "OK";
};

struct Slider : loki::visitable<Slider> {
    int min_val = 0, max_val = 100;
};

struct Inspector : loki::base_visitor,
                   loki::visitor<Button>,
                   loki::visitor<Slider> {
    void visit(Button& b) override {
        std::cout << "Button [" << b.label << "]\n";
    }
    void visit(Slider& s) override {
        std::cout << "Slider [" << s.min_val << ".." << s.max_val << "]\n";
    }
};

int main() {
    std::unique_ptr<loki::base_visitable<>> w1 = std::make_unique<Button>();
    std::unique_ptr<loki::base_visitable<>> w2 = std::make_unique<Slider>();

    Inspector insp;
    w1->accept(insp);
    w2->accept(insp);
}
```

---

### 9. variant_visit for Message Dispatch

Pattern-match incoming network messages stored as a variant.

```cpp
#include <loki/visitor.hpp>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

struct Connect    { std::string user; };
struct Disconnect { std::string reason; };
struct Data       { std::vector<uint8_t> payload; };

using Message = std::variant<Connect, Disconnect, Data>;

void dispatch(const Message& msg) {
    loki::variant_visit(msg,
        [](const Connect& c) {
            std::cout << "User connected: " << c.user << "\n";
        },
        [](const Disconnect& d) {
            std::cout << "Disconnected: " << d.reason << "\n";
        },
        [](const Data& d) {
            std::cout << "Received " << d.payload.size() << " bytes\n";
        }
    );
}

int main() {
    std::vector<Message> inbox;
    inbox.push_back(Connect{"alice"});
    inbox.push_back(Data{{0x01, 0x02, 0x03}});
    inbox.push_back(Disconnect{"timeout"});

    for (const auto& msg : inbox) dispatch(msg);
}
```

---

### 10. Combining Acyclic and Variant Visitors

Use the acyclic visitor for an open class hierarchy and `variant_visit` for closed value types within the same system.

```cpp
#include <loki/visitor.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <variant>

struct Literal : loki::visitable<Literal> {
    using Value = std::variant<int, double, std::string>;
    Value val;
    explicit Literal(Value v) : val(std::move(v)) {}
};

struct BinOp : loki::visitable<BinOp> {
    char op;
    std::unique_ptr<loki::base_visitable<>> lhs, rhs;
    BinOp(char op, std::unique_ptr<loki::base_visitable<>> l,
                   std::unique_ptr<loki::base_visitable<>> r)
        : op(op), lhs(std::move(l)), rhs(std::move(r)) {}
};

struct Printer : loki::base_visitor,
                 loki::visitor<Literal>,
                 loki::visitor<BinOp> {
    void visit(Literal& lit) override {
        loki::variant_visit(lit.val,
            [](int i)                { std::cout << i; },
            [](double d)             { std::cout << d; },
            [](const std::string& s) { std::cout << '"' << s << '"'; }
        );
    }

    void visit(BinOp& b) override {
        std::cout << "(";
        b.lhs->accept(*this);
        std::cout << " " << b.op << " ";
        b.rhs->accept(*this);
        std::cout << ")";
    }
};

int main() {
    auto ast = std::make_unique<BinOp>('+',
        std::make_unique<Literal>(42),
        std::make_unique<Literal>(std::string("hello")));

    Printer p;
    ast->accept(p);
    std::cout << "\n";
}
```
