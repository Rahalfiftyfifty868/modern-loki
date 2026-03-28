# factory.hpp

`factory` and `clone_factory`: registries that map identifiers to object constructors or cloners.

## API

| Component | Description |
|-----------|-------------|
| `factory<AbstractProduct, IdType, Creator, ErrorPolicy>` | String-keyed (default) object factory backed by `std::map`. `register_type(id, creator)`, `unregister(id)`, `create(id)`, `is_registered(id)`, `size()`. |
| `auto_register<Base, Derived, IdType>` | RAII helper: constructor calls `register_type` with a `make_unique<Derived>` lambda. |
| `clone_factory<AbstractProduct, Cloner, ErrorPolicy>` | Deep-copy factory keyed by `std::type_index`. `register_type<T>(cloner)`, `create(model)`. |
| `default_factory_error` | Default error policy: throws `std::runtime_error` on unknown type. |

---

### Shape factory with string IDs

Register concrete shapes under human-readable names and build instances from configuration strings.

```cpp
#include <loki/factory.hpp>
#include <iostream>
#include <memory>
#include <string>

struct Shape {
    virtual ~Shape() = default;
    virtual const char* name() const = 0;
};

struct Circle : Shape {
    const char* name() const override { return "Circle"; }
};
struct Rectangle : Shape {
    const char* name() const override { return "Rectangle"; }
};
struct Triangle : Shape {
    const char* name() const override { return "Triangle"; }
};

int main() {
    loki::factory<Shape> shapes;
    shapes.register_type("circle", [] { return std::make_unique<Circle>(); });
    shapes.register_type("rectangle", [] { return std::make_unique<Rectangle>(); });
    shapes.register_type("triangle", [] { return std::make_unique<Triangle>(); });

    for (const char* id : {"circle", "rectangle", "triangle"}) {
        auto s = shapes.create(id);
        std::cout << s->name() << '\n';
    }
    return 0;
}
```

---

### Serialization format factory

Select a parser implementation by format id while keeping a single abstract reader interface.

```cpp
#include <loki/factory.hpp>
#include <iostream>
#include <memory>
#include <string>

struct Reader {
    virtual ~Reader() = default;
    virtual const char* format() const = 0;
    virtual void parse(const char* path) = 0;
};

struct JsonReader : Reader {
    const char* format() const override { return "JSON"; }
    void parse(const char* path) override { (void)path; }
};
struct XmlReader : Reader {
    const char* format() const override { return "XML"; }
    void parse(const char* path) override { (void)path; }
};
struct YamlReader : Reader {
    const char* format() const override { return "YAML"; }
    void parse(const char* path) override { (void)path; }
};

int main() {
    loki::factory<Reader> readers;
    readers.register_type("json", [] { return std::make_unique<JsonReader>(); });
    readers.register_type("xml", [] { return std::make_unique<XmlReader>(); });
    readers.register_type("yaml", [] { return std::make_unique<YamlReader>(); });

    for (const char* fmt : {"json", "xml", "yaml"}) {
        auto r = readers.create(fmt);
        std::cout << fmt << " -> " << r->format() << "\n";
    }
    return 0;
}
```

---

### Logger backend factory

Swap sinks (console, file, network) behind a `Logger` interface; registration mirrors deployment config.

```cpp
#include <loki/factory.hpp>
#include <iostream>
#include <memory>
#include <string>

struct Logger {
    virtual ~Logger() = default;
    virtual const char* sink() const = 0;
    virtual void log(const char* msg) = 0;
};

struct ConsoleLogger : Logger {
    const char* sink() const override { return "console"; }
    void log(const char* msg) override { std::cout << "[console] " << msg << "\n"; }
};
struct FileLogger : Logger {
    const char* sink() const override { return "file"; }
    void log(const char* msg) override { std::cout << "[file] " << msg << "\n"; }
};
struct NetworkLogger : Logger {
    const char* sink() const override { return "network"; }
    void log(const char* msg) override { std::cout << "[network] " << msg << "\n"; }
};

int main() {
    loki::factory<Logger> loggers;
    loggers.register_type("console", [] { return std::make_unique<ConsoleLogger>(); });
    loggers.register_type("file", [] { return std::make_unique<FileLogger>(); });
    loggers.register_type("network", [] { return std::make_unique<NetworkLogger>(); });

    auto lg = loggers.create("console");
    lg->log("server started");
    std::cout << "sink: " << lg->sink() << "\n";
    return 0;
}
```

---

### Auto-registering plugins at static init time

Use `auto_register` at file scope so each plugin registers itself before `main` runs.

```cpp
#include <loki/factory.hpp>
#include <iostream>
#include <memory>
#include <string>

struct Plugin {
    virtual ~Plugin() = default;
    virtual const char* id() const = 0;
};

struct AlphaPlugin : Plugin {
    const char* id() const override { return "alpha"; }
};
struct BetaPlugin : Plugin {
    const char* id() const override { return "beta"; }
};

loki::factory<Plugin> g_plugins;

namespace {
loki::auto_register<Plugin, AlphaPlugin> reg_alpha(g_plugins, "alpha");
loki::auto_register<Plugin, BetaPlugin> reg_beta(g_plugins, "beta");
}

int main() {
    std::cout << "registered: " << g_plugins.size() << " plugins\n";
    for (const char* name : {"alpha", "beta"}) {
        auto p = g_plugins.create(name);
        std::cout << "  " << p->id() << "\n";
    }
    return 0;
}
```

---

### Clone factory for deep-copying polymorphic game objects

Duplicate entities by dispatching on the dynamic type of a base reference using `typeid`.

```cpp
#include <loki/factory.hpp>
#include <iostream>
#include <memory>

struct GameObject {
    virtual ~GameObject() = default;
    virtual int hp() const = 0;
    virtual const char* kind() const = 0;
};

struct Enemy : GameObject {
    int hp() const override { return 10; }
    const char* kind() const override { return "enemy"; }
};
struct Pickup : GameObject {
    int hp() const override { return 0; }
    const char* kind() const override { return "pickup"; }
};

int main() {
    loki::clone_factory<GameObject> cloners;
    cloners.register_type<Enemy>([](const GameObject& m) {
        return std::make_unique<Enemy>(static_cast<const Enemy&>(m));
    });
    cloners.register_type<Pickup>([](const GameObject& m) {
        return std::make_unique<Pickup>(static_cast<const Pickup&>(m));
    });

    Enemy e;
    auto copy = cloners.create(e);
    std::cout << "original: " << e.kind() << " hp=" << e.hp() << "\n";
    std::cout << "clone:    " << copy->kind() << " hp=" << copy->hp() << "\n";
    return 0;
}
```

---

### Factory with integer IDs (protocol message types)

Use an integral identifier type for compact wire enums or opcode tables.

```cpp
#include <loki/factory.hpp>
#include <cstdint>
#include <iostream>
#include <memory>

enum class MsgId : std::uint8_t { Ping = 1, Pong = 2, Quit = 3 };

struct Message {
    virtual ~Message() = default;
    virtual const char* name() const = 0;
};

struct PingMsg : Message { const char* name() const override { return "Ping"; } };
struct PongMsg : Message { const char* name() const override { return "Pong"; } };
struct QuitMsg : Message { const char* name() const override { return "Quit"; } };

int main() {
    loki::factory<Message, MsgId> factory;
    factory.register_type(MsgId::Ping, [] { return std::make_unique<PingMsg>(); });
    factory.register_type(MsgId::Pong, [] { return std::make_unique<PongMsg>(); });
    factory.register_type(MsgId::Quit, [] { return std::make_unique<QuitMsg>(); });

    for (auto id : {MsgId::Ping, MsgId::Pong, MsgId::Quit}) {
        auto m = factory.create(id);
        std::cout << "opcode " << static_cast<int>(id) << " -> " << m->name() << "\n";
    }
    return 0;
}
```

---

### Unregistering and re-registering types at runtime

Hot-reload or A/B test implementations by removing a creator and binding a new one under the same id.

```cpp
#include <loki/factory.hpp>
#include <iostream>
#include <memory>
#include <string>

struct Service {
    virtual ~Service() = default;
    virtual int version() const = 0;
};

struct V1 : Service { int version() const override { return 1; } };
struct V2 : Service { int version() const override { return 2; } };

int main() {
    loki::factory<Service> f;
    f.register_type("api", [] { return std::make_unique<V1>(); });
    auto a = f.create("api");
    std::cout << "before: v" << a->version() << "\n";

    f.unregister("api");
    f.register_type("api", [] { return std::make_unique<V2>(); });
    auto b = f.create("api");
    std::cout << "after:  v" << b->version() << "\n";
    return 0;
}
```

---

### Custom error policy returning a default object

Replace throws with a fallback instance when a codec or handler name is unrecognized.

```cpp
#include <loki/factory.hpp>
#include <iostream>
#include <memory>
#include <string>

struct Decoder {
    virtual ~Decoder() = default;
    virtual const char* kind() const = 0;
};

struct NullDecoder : Decoder {
    const char* kind() const override { return "null"; }
};
struct Mp3Decoder : Decoder {
    const char* kind() const override { return "mp3"; }
};

template <typename Id, typename AbstractProduct>
struct fallback_decoder_error {
    static std::unique_ptr<AbstractProduct> on_unknown_type(const Id&) {
        return std::make_unique<NullDecoder>();
    }
};

int main() {
    loki::factory<Decoder, std::string, std::function<std::unique_ptr<Decoder>()>,
                  fallback_decoder_error>
        codecs;
    codecs.register_type("mp3", [] { return std::make_unique<Mp3Decoder>(); });

    auto known = codecs.create("mp3");
    auto unknown = codecs.create("ogg");
    std::cout << "mp3 -> " << known->kind() << "\n";
    std::cout << "ogg -> " << unknown->kind() << " (fallback)\n";
    return 0;
}
```

---

### Audio codec factory (MP3, FLAC, WAV decoders)

Same pattern as other string-keyed factories: one interface, many binary formats, creators registered once at startup.

```cpp
#include <loki/factory.hpp>
#include <iostream>
#include <memory>
#include <string>

struct AudioDecoder {
    virtual ~AudioDecoder() = default;
    virtual const char* codec() const = 0;
    virtual void decode(const void* data, std::size_t n) = 0;
};

struct Mp3Decoder : AudioDecoder {
    const char* codec() const override { return "mp3"; }
    void decode(const void* data, std::size_t n) override { (void)data; (void)n; }
};
struct FlacDecoder : AudioDecoder {
    const char* codec() const override { return "flac"; }
    void decode(const void* data, std::size_t n) override { (void)data; (void)n; }
};
struct WavDecoder : AudioDecoder {
    const char* codec() const override { return "wav"; }
    void decode(const void* data, std::size_t n) override { (void)data; (void)n; }
};

int main() {
    loki::factory<AudioDecoder> codecs;
    codecs.register_type("mp3", [] { return std::make_unique<Mp3Decoder>(); });
    codecs.register_type("flac", [] { return std::make_unique<FlacDecoder>(); });
    codecs.register_type("wav", [] { return std::make_unique<WavDecoder>(); });

    std::cout << "registered " << codecs.size() << " codecs\n";
    for (const char* name : {"mp3", "flac", "wav"}) {
        auto dec = codecs.create(name);
        std::cout << "  " << name << " -> " << dec->codec() << "\n";
    }
    return 0;
}
```

---

### Command pattern factory (Undo/Redo system)

Map command names to command factories; the editor stack holds `unique_ptr<Command>` produced by the factory.

```cpp
#include <loki/factory.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

struct Command {
    virtual ~Command() = default;
    virtual const char* name() const = 0;
    virtual void execute() = 0;
    virtual void undo() = 0;
};

struct InsertText : Command {
    const char* name() const override { return "insert"; }
    void execute() override { std::cout << "  exec: insert text\n"; }
    void undo() override    { std::cout << "  undo: remove text\n"; }
};
struct DeleteRange : Command {
    const char* name() const override { return "delete"; }
    void execute() override { std::cout << "  exec: delete range\n"; }
    void undo() override    { std::cout << "  undo: restore range\n"; }
};

int main() {
    loki::factory<Command> commands;
    commands.register_type("insert", [] { return std::make_unique<InsertText>(); });
    commands.register_type("delete", [] { return std::make_unique<DeleteRange>(); });

    std::vector<std::unique_ptr<Command>> history;
    for (const char* op : {"insert", "delete", "insert"}) {
        auto cmd = commands.create(op);
        cmd->execute();
        history.push_back(std::move(cmd));
    }
    std::cout << "undo:\n";
    while (!history.empty()) {
        history.back()->undo();
        history.pop_back();
    }
    return 0;
}
```
