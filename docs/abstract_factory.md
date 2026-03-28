# `loki::abstract_factory`

The header `<loki/abstract_factory.hpp>` provides a tuple-based abstract factory: no virtual inheritance, no diamond hierarchies, and no RTTI for dispatch. Product creators are stored in order of the template parameter pack; after `freeze()`, further `set_creator` calls assert in debug builds.

**`abstract_factory<Products...>`** — Declares one abstract base type per product slot. `create<T>()` returns `std::unique_ptr<T>` (or `nullptr` if no creator was set). Subclasses wire creators via protected `set_creator<T>(std::function<std::unique_ptr<T>()>)` and call `freeze()` when wiring is complete.

**`concrete_factory<AF, ConcreteProducts...>`** — `AF` must be `abstract_factory<AbstractProducts...>`. For each abstract product, the matching concrete type is wired in constructor order, then the factory is frozen.

**`simple_concrete_factory<AF>`** — Exposes `set_creator<I>(...)` with a numeric index `I` matching the product position in `AF`, and `done()` to freeze after runtime wiring.

---

### Basic widget factory

A desktop toolkit exposes abstract `Button` and `Checkbox` interfaces; a single concrete factory pairs each with one implementation for the default theme.

```cpp
#include <iostream>
#include <loki/abstract_factory.hpp>
#include <memory>
#include <string>

struct Button {
    virtual ~Button() = default;
    virtual std::string label() const = 0;
};

struct Checkbox {
    virtual ~Checkbox() = default;
    virtual bool checked() const = 0;
};

struct DefaultButton : Button {
    std::string label() const override { return "OK"; }
};

struct DefaultCheckbox : Checkbox {
    bool checked() const override { return false; }
};

int main() {
    using AF = loki::abstract_factory<Button, Checkbox>;
    loki::concrete_factory<AF, DefaultButton, DefaultCheckbox> factory;

    auto btn = factory.create<Button>();
    auto box = factory.create<Checkbox>();
    std::cout << btn->label() << " " << (box->checked() ? "on" : "off") << "\n";
    return 0;
}
```

---

### Cross-platform UI factory

The same abstract API is implemented twice: one `concrete_factory` for Windows controls and another for Linux (GTK-style) controls; the application picks the implementation at startup based on the host OS.

```cpp
#include <iostream>
#include <loki/abstract_factory.hpp>
#include <memory>
#include <string>

struct WindowBase {
    virtual ~WindowBase() = default;
    virtual std::string platform() const = 0;
};

struct MenuBase {
    virtual ~MenuBase() = default;
    virtual int item_count() const = 0;
};

struct WinWindow : WindowBase {
    std::string platform() const override { return "win32"; }
};

struct WinMenu : MenuBase {
    int item_count() const override { return 3; }
};

struct LinuxWindow : WindowBase {
    std::string platform() const override { return "wayland"; }
};

struct LinuxMenu : MenuBase {
    int item_count() const override { return 3; }
};

int main() {
    using AF = loki::abstract_factory<WindowBase, MenuBase>;
#if defined(_WIN32)
    loki::concrete_factory<AF, WinWindow, WinMenu> factory;
#else
    loki::concrete_factory<AF, LinuxWindow, LinuxMenu> factory;
#endif
    auto w = factory.create<WindowBase>();
    auto m = factory.create<MenuBase>();
    std::cout << w->platform() << " menu_items=" << m->item_count() << "\n";
    return 0;
}
```

---

### Game entity factory

A spawn system registers one concrete type per role: the hero, a hostile mob, and a merchant. Each role is a distinct abstract interface so `create<T>()` can target `Player`, `Enemy`, or `Npc` unambiguously.

```cpp
#include <iostream>
#include <loki/abstract_factory.hpp>
#include <memory>
#include <string>

struct Player {
    virtual ~Player() = default;
    virtual std::string role() const = 0;
};

struct Enemy {
    virtual ~Enemy() = default;
    virtual int threat() const = 0;
};

struct Npc {
    virtual ~Npc() = default;
    virtual std::string dialogue_id() const = 0;
};

struct HeroPlayer : Player {
    std::string role() const override { return "hero"; }
};

struct GoblinEnemy : Enemy {
    int threat() const override { return 5; }
};

struct MerchantNpc : Npc {
    std::string dialogue_id() const override { return "shop_intro"; }
};

int main() {
    using AF = loki::abstract_factory<Player, Enemy, Npc>;
    loki::concrete_factory<AF, HeroPlayer, GoblinEnemy, MerchantNpc> spawn;

    auto hero = spawn.create<Player>();
    auto foe = spawn.create<Enemy>();
    auto shopkeeper = spawn.create<Npc>();
    std::cout << hero->role() << " vs threat " << foe->threat()
              << " npc " << shopkeeper->dialogue_id() << "\n";
    return 0;
}
```

---

### Document renderers

A viewer application supports multiple backends: PDF, HTML, and Markdown. Each format is an abstract `DocumentRenderer` specialization (separate abstract bases) so the factory can instantiate the correct exporter or preview sink.

```cpp
#include <iostream>
#include <loki/abstract_factory.hpp>
#include <memory>
#include <string>

struct PdfRenderer {
    virtual ~PdfRenderer() = default;
    virtual std::string mime() const = 0;
};

struct HtmlRenderer {
    virtual ~HtmlRenderer() = default;
    virtual std::string mime() const = 0;
};

struct MarkdownRenderer {
    virtual ~MarkdownRenderer() = default;
    virtual std::string mime() const = 0;
};

struct PdfiumBackend : PdfRenderer {
    std::string mime() const override { return "application/pdf"; }
};

struct WebKitHtml : HtmlRenderer {
    std::string mime() const override { return "text/html"; }
};

struct CommonMarkMd : MarkdownRenderer {
    std::string mime() const override { return "text/markdown"; }
};

int main() {
    using AF = loki::abstract_factory<PdfRenderer, HtmlRenderer, MarkdownRenderer>;
    loki::concrete_factory<AF, PdfiumBackend, WebKitHtml, CommonMarkMd> backends;

    std::cout << backends.create<PdfRenderer>()->mime() << "\n";
    std::cout << backends.create<HtmlRenderer>()->mime() << "\n";
    std::cout << backends.create<MarkdownRenderer>()->mime() << "\n";
    return 0;
}
```

---

### Database connections and queries

A data access layer abstracts `Connection` and `Query` for relational backends. A `concrete_factory` binds MySQL or PostgreSQL client types to those interfaces for a given deployment profile.

```cpp
#include <iostream>
#include <loki/abstract_factory.hpp>
#include <memory>
#include <string>

struct Connection {
    virtual ~Connection() = default;
    virtual std::string dsn() const = 0;
};

struct Query {
    virtual ~Query() = default;
    virtual std::string dialect() const = 0;
};

struct MySqlConnection : Connection {
    std::string dsn() const override { return "mysql://localhost/app"; }
};

struct MySqlQuery : Query {
    std::string dialect() const override { return "mysql"; }
};

struct PgConnection : Connection {
    std::string dsn() const override { return "postgresql://localhost/app"; }
};

struct PgQuery : Query {
    std::string dialect() const override { return "postgresql"; }
};

int main() {
    using AF = loki::abstract_factory<Connection, Query>;
    loki::concrete_factory<AF, MySqlConnection, MySqlQuery> mysql_stack;

    auto conn = mysql_stack.create<Connection>();
    auto q = mysql_stack.create<Query>();
    std::cout << conn->dsn() << " " << q->dialect() << "\n";

    loki::concrete_factory<AF, PgConnection, PgQuery> pg_stack;
    auto conn2 = pg_stack.create<Connection>();
    std::cout << conn2->dsn() << "\n";
    return 0;
}
```

---

### `simple_concrete_factory` and runtime configuration

An app reads a config string (`"mysql"` or `"postgres"`) and wires the matching driver types by index before calling `done()`, so the same binary can select the backend without recompiling separate `concrete_factory` specializations.

```cpp
#include <iostream>
#include <loki/abstract_factory.hpp>
#include <memory>
#include <string>

struct Connection {
    virtual ~Connection() = default;
    virtual std::string backend() const = 0;
};

struct Query {
    virtual ~Query() = default;
    virtual std::string backend() const = 0;
};

struct MySqlConnection : Connection {
    std::string backend() const override { return "mysql"; }
};

struct MySqlQuery : Query {
    std::string backend() const override { return "mysql"; }
};

struct PgConnection : Connection {
    std::string backend() const override { return "postgres"; }
};

struct PgQuery : Query {
    std::string backend() const override { return "postgres"; }
};

int main() {
    using AF = loki::abstract_factory<Connection, Query>;
    loki::simple_concrete_factory<AF> factory;

    const std::string cfg = "postgres"; // e.g. from file or env
    if (cfg == "mysql") {
        factory.set_creator<0>([] { return std::make_unique<MySqlConnection>(); });
        factory.set_creator<1>([] { return std::make_unique<MySqlQuery>(); });
    } else {
        factory.set_creator<0>([] { return std::make_unique<PgConnection>(); });
        factory.set_creator<1>([] { return std::make_unique<PgQuery>(); });
    }
    factory.done();

    std::cout << factory.create<Connection>()->backend() << "\n";
    return 0;
}
```

---

### Passing the factory by const reference

UI code that only creates widgets should take `const abstract_factory&` so it cannot rewire creators; `create<T>()` remains usable on a const factory.

```cpp
#include <iostream>
#include <loki/abstract_factory.hpp>
#include <memory>
#include <string>

struct Toolbar {
    virtual ~Toolbar() = default;
    virtual std::string id() const = 0;
};

struct StatusBar {
    virtual ~StatusBar() = default;
    virtual std::string id() const = 0;
};

struct QtToolbar : Toolbar {
    std::string id() const override { return "qt_toolbar"; }
};

struct QtStatusBar : StatusBar {
    std::string id() const override { return "qt_status"; }
};

void build_chrome(const loki::abstract_factory<Toolbar, StatusBar>& factory) {
    auto t = factory.create<Toolbar>();
    auto s = factory.create<StatusBar>();
    std::cout << t->id() << " " << s->id() << "\n";
}

int main() {
    using AF = loki::abstract_factory<Toolbar, StatusBar>;
    const loki::concrete_factory<AF, QtToolbar, QtStatusBar> factory;
    build_chrome(factory);
    return 0;
}
```

---

### Rendering pipeline with three or more products

A graphics engine groups stages as separate abstract types: vertex processing, rasterization, and a post-process pass. One factory wires the whole pipeline for a given API (e.g. Vulkan vs Metal) by listing four concrete implementations in order.

```cpp
#include <iostream>
#include <loki/abstract_factory.hpp>
#include <memory>
#include <string>

struct VertexStage {
    virtual ~VertexStage() = default;
    virtual std::string name() const = 0;
};

struct GeometryStage {
    virtual ~GeometryStage() = default;
    virtual std::string name() const = 0;
};

struct RasterStage {
    virtual ~RasterStage() = default;
    virtual std::string name() const = 0;
};

struct PostProcessPass {
    virtual ~PostProcessPass() = default;
    virtual std::string name() const = 0;
};

struct VkVertex : VertexStage {
    std::string name() const override { return "vk_vert"; }
};
struct VkGeometry : GeometryStage {
    std::string name() const override { return "vk_geom"; }
};
struct VkRaster : RasterStage {
    std::string name() const override { return "vk_rast"; }
};
struct VkPost : PostProcessPass {
    std::string name() const override { return "vk_post"; }
};

int main() {
    using AF = loki::abstract_factory<VertexStage, GeometryStage, RasterStage, PostProcessPass>;
    loki::concrete_factory<AF, VkVertex, VkGeometry, VkRaster, VkPost> pipeline;

    std::cout << pipeline.create<VertexStage>()->name() << " -> "
              << pipeline.create<PostProcessPass>()->name() << "\n";
    return 0;
}
```

---

### Swapping implementations at runtime via base pointer

The abstract factory type is fixed (`abstract_factory<Widget, Gadget>`), but the owning `std::unique_ptr` can point at different concrete factories (e.g. debug vs release skins) and be reassigned while the rest of the code keeps using the base pointer.

```cpp
#include <iostream>
#include <loki/abstract_factory.hpp>
#include <memory>
#include <string>

struct Widget {
    virtual ~Widget() = default;
    virtual std::string skin() const = 0;
};

struct Gadget {
    virtual ~Gadget() = default;
    virtual std::string skin() const = 0;
};

struct LightWidget : Widget {
    std::string skin() const override { return "light"; }
};
struct LightGadget : Gadget {
    std::string skin() const override { return "light"; }
};

struct DarkWidget : Widget {
    std::string skin() const override { return "dark"; }
};
struct DarkGadget : Gadget {
    std::string skin() const override { return "dark"; }
};

int main() {
    using AF = loki::abstract_factory<Widget, Gadget>;

    std::unique_ptr<AF> ui =
        std::make_unique<loki::concrete_factory<AF, LightWidget, LightGadget>>();
    std::cout << ui->create<Widget>()->skin() << "\n";

    ui = std::make_unique<loki::concrete_factory<AF, DarkWidget, DarkGadget>>();
    std::cout << ui->create<Gadget>()->skin() << "\n";
    return 0;
}
```

---

### Plugin-style factory per platform

A host loads a logical “UI kit” that returns a fully wired factory for that platform (Win32 vs Cocoa). Callers store `unique_ptr<abstract_factory<...>>` and treat it like a plugin-provided entry point.

```cpp
#include <iostream>
#include <loki/abstract_factory.hpp>
#include <memory>
#include <string>

struct NativeView {
    virtual ~NativeView() = default;
    virtual std::string toolkit() const = 0;
};

struct NativeMenu {
    virtual ~NativeMenu() = default;
    virtual std::string toolkit() const = 0;
};

struct WinView : NativeView {
    std::string toolkit() const override { return "Win32"; }
};
struct WinMenu : NativeMenu {
    std::string toolkit() const override { return "Win32"; }
};

struct CocoaView : NativeView {
    std::string toolkit() const override { return "Cocoa"; }
};
struct CocoaMenu : NativeMenu {
    std::string toolkit() const override { return "Cocoa"; }
};

using UIKitAF = loki::abstract_factory<NativeView, NativeMenu>;

std::unique_ptr<UIKitAF> load_platform_ui_plugin() {
#if defined(_WIN32)
    return std::make_unique<loki::concrete_factory<UIKitAF, WinView, WinMenu>>();
#else
    return std::make_unique<loki::concrete_factory<UIKitAF, CocoaView, CocoaMenu>>();
#endif
}

int main() {
    std::unique_ptr<UIKitAF> plugin = load_platform_ui_plugin();
    std::cout << plugin->create<NativeView>()->toolkit() << "\n";
    return 0;
}
```
