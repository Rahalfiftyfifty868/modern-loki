#include "doctest.h"
#include <loki/multi_methods.hpp>
#include <string>
#include <variant>

using namespace loki;

struct Shape {
    virtual ~Shape() = default;
};

struct Rectangle : Shape {};
struct Ellipse : Shape {};

TEST_CASE("basic_dispatcher") {
    basic_dispatcher<Shape, Shape, std::string> dispatcher;

    dispatcher.add<Rectangle, Ellipse>(
        [](Shape&, Shape&) -> std::string { return "rect-ellipse"; });
    dispatcher.add<Rectangle, Rectangle>(
        [](Shape&, Shape&) -> std::string { return "rect-rect"; });

    Rectangle r1, r2;
    Ellipse e;

    CHECK(dispatcher.go(r1, e) == "rect-ellipse");
    CHECK(dispatcher.go(r1, r2) == "rect-rect");
}

TEST_CASE("basic_dispatcher throws on unknown") {
    basic_dispatcher<Shape, Shape, std::string> dispatcher;
    Rectangle r;
    Ellipse e;
    CHECK_THROWS_AS(dispatcher.go(r, e), std::runtime_error);
}

TEST_CASE("fn_dispatcher const go") {
    fn_dispatcher<Shape, Shape, std::string> dispatcher;
    dispatcher.add<Rectangle, Ellipse>(
        std::function<std::string(Rectangle&, Ellipse&)>(
            [](Rectangle&, Ellipse&) -> std::string { return "const-fn"; }));

    const auto& cref = dispatcher;
    Rectangle r;
    Ellipse e;
    CHECK(cref.go(r, e) == "const-fn");
}

TEST_CASE("fn_dispatcher") {
    fn_dispatcher<Shape, Shape, std::string> dispatcher;

    dispatcher.add<Rectangle, Ellipse>(
        std::function<std::string(Rectangle&, Ellipse&)>(
            [](Rectangle&, Ellipse&) -> std::string { return "fn-rect-ellipse"; }));

    Rectangle r;
    Ellipse e;
    CHECK(dispatcher.go(r, e) == "fn-rect-ellipse");
}

struct Executor {
    std::string fire(Rectangle&, Ellipse&) { return "static-rect-ellipse"; }
    std::string fire(Rectangle&, Rectangle&) { return "static-rect-rect"; }

    template <typename L, typename R>
    std::string fire(L&, R&) { return on_error(); }

    std::string on_error() { return "error"; }
};

TEST_CASE("basic_dispatcher go is const-callable") {
    basic_dispatcher<Shape, Shape, std::string> dispatcher;
    dispatcher.add<Rectangle, Ellipse>(
        [](Shape&, Shape&) -> std::string { return "const-ok"; });

    const auto& cref = dispatcher;
    Rectangle r;
    Ellipse e;
    CHECK(cref.go(r, e) == "const-ok");
}

TEST_CASE("static_dispatcher with catch-all executor") {
    using Types = typelist<Rectangle, Ellipse>;
    using Dispatcher = static_dispatcher<Executor, Shape, Types, false,
                                          Shape, Types, std::string>;

    Rectangle r;
    Ellipse e;
    Executor exec;
    CHECK(Dispatcher::go(r, e, exec) == "static-rect-ellipse");
    CHECK(Dispatcher::go(r, r, exec) == "static-rect-rect");
    CHECK(Dispatcher::go(e, r, exec) == "error");
    CHECK(Dispatcher::go(e, e, exec) == "error");
}

struct VRect { int w = 0; };
struct VEllipse { double r = 0; };
using ShapeVar = std::variant<VRect, VEllipse>;

struct VarExecutor {
    std::string fire(VRect&, VEllipse&) { return "v-rect-ellipse"; }
    std::string fire(VRect&, VRect&) { return "v-rect-rect"; }
    std::string fire(VEllipse&, VRect&) { return "v-ellipse-rect"; }
    std::string fire(VEllipse&, VEllipse&) { return "v-ellipse-ellipse"; }
};

TEST_CASE("variant_dispatch O(1) double dispatch") {
    ShapeVar a = VRect{10};
    ShapeVar b = VEllipse{3.14};
    VarExecutor exec;

    CHECK(loki::variant_dispatch(a, b, exec) == "v-rect-ellipse");

    ShapeVar c = VRect{5};
    CHECK(loki::variant_dispatch(a, c, exec) == "v-rect-rect");

    ShapeVar d = VEllipse{1.0};
    CHECK(loki::variant_dispatch(d, a, exec) == "v-ellipse-rect");
    CHECK(loki::variant_dispatch(d, b, exec) == "v-ellipse-ellipse");
}

TEST_CASE("fn_dispatcher symmetric remove") {
    fn_dispatcher<Shape, Shape, std::string> dispatcher;
    dispatcher.add<Rectangle, Ellipse>(
        std::function<std::string(Rectangle&, Ellipse&)>(
            [](Rectangle&, Ellipse&) -> std::string { return "r-e"; }),
        true);

    Rectangle r;
    Ellipse e;
    CHECK(dispatcher.go(r, e) == "r-e");
    CHECK(dispatcher.go(e, r) == "r-e");

    CHECK(dispatcher.remove<Rectangle, Ellipse, true>());
    CHECK_THROWS_AS(dispatcher.go(r, e), std::runtime_error);
    CHECK_THROWS_AS(dispatcher.go(e, r), std::runtime_error);
}
