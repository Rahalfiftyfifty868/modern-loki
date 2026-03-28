#include "doctest.h"
#include <loki/visitor.hpp>
#include <string>
#include <variant>

using namespace loki;

struct Circle : visitable<Circle> {
    double radius = 5.0;
    Circle() = default;
    explicit Circle(double r) : radius(r) {}
};

struct Square : visitable<Square> {
    double side = 4.0;
    Square() = default;
    explicit Square(double s) : side(s) {}
};

class AreaVisitor : public base_visitor,
                    public visitor<Circle>,
                    public visitor<Square> {
public:
    double result = 0.0;

    void visit(Circle& c) override { result = 3.14159 * c.radius * c.radius; }
    void visit(Square& s) override { result = s.side * s.side; }
};

TEST_CASE("acyclic visitor - circle") {
    Circle c;
    AreaVisitor v;
    c.accept(v);
    CHECK(v.result == doctest::Approx(78.5397).epsilon(0.01));
}

TEST_CASE("acyclic visitor - square") {
    Square s;
    AreaVisitor v;
    s.accept(v);
    CHECK(v.result == 16.0);
}

TEST_CASE("variant_visit") {
    using Shape = std::variant<Circle, Square>;
    Shape shape = Circle(3.0);

    double area = variant_visit(shape,
        [](Circle& c) { return 3.14159 * c.radius * c.radius; },
        [](Square& s) { return s.side * s.side; }
    );

    CHECK(area == doctest::Approx(28.274).epsilon(0.01));
}

TEST_CASE("overloaded pattern") {
    using Var = std::variant<int, double, std::string>;
    Var v = std::string("hello");

    std::string result = std::visit(overloaded{
        [](int i) { return std::to_string(i); },
        [](double d) { return std::to_string(d); },
        [](const std::string& s) { return s; }
    }, v);

    CHECK(result == "hello");
}

TEST_CASE("multi_visitor") {
    struct IntVisitor : multi_visitor<void, Circle, Square> {
        int count = 0;
        void visit(Circle&) override { count += 1; }
        void visit(Square&) override { count += 10; }
    };

    IntVisitor iv;
    Circle c;
    Square s;
    iv.visit(c);
    iv.visit(s);
    CHECK(iv.count == 11);
}
