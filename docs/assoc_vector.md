# `loki::assoc_vector`

`loki::assoc_vector` is a sorted-vector–based associative container: a practical **flat map** polyfill with the same core API as `std::map`. Keys are kept in order using `Compare` (default `std::less<K>`). Lookups are O(log N); inserts and erases move elements and are O(N), which is often favorable for small or medium tables due to cache locality.

**Template parameters:** `K` (key type), `V` (mapped type), `Compare` (defaults to `std::less<K>`), `Allocator` (defaults to `std::allocator<std::pair<K, V>>`).

**Typical operations:** `operator[]`, `at`, `insert`, `emplace`, `try_emplace`, `erase`, `find`, `contains`, `count`, `lower_bound`, `upper_bound`, `equal_range`, `swap`, iterators, and comparison operators.

---

### HTTP header storage (case-insensitive keys)

HTTP field names are case-insensitive. A comparator that orders strings by ASCII case-folded characters lets one canonical map behave like a header set: equivalent names under the comparator cannot both exist.

```cpp
#include <loki/assoc_vector.hpp>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

struct ci_less {
    bool operator()(const std::string& a, const std::string& b) const {
        return std::lexicographical_compare(
            a.begin(), a.end(), b.begin(), b.end(),
            [](unsigned char x, unsigned char y) {
                return std::tolower(x) < std::tolower(y);
            });
    }
};

int main() {
    loki::assoc_vector<std::string, std::string, ci_less> headers;
    headers.insert({"Content-Type", "application/json"});
    headers["Accept"] = "text/html";
    headers.try_emplace("accept", "application/json"); // duplicate key: ignored

    std::cout << headers.at("ACCEPT") << "\n";
    std::cout << headers.contains("content-type") << "\n";
    return 0;
}
```

---

### Symbol table for a scripting language interpreter

Map each identifier to its slot index (or symbol id) as the lexer hands off names. Sorted order helps debugging dumps and stable iteration during codegen.

```cpp
#include <loki/assoc_vector.hpp>
#include <iostream>
#include <string>

int main() {
    loki::assoc_vector<std::string, int> symbols;
    symbols["pi"] = 0;
    symbols["answer"] = 1;
    symbols.try_emplace("answer", 99); // already present

    auto it = symbols.find("pi");
    if (it != symbols.end())
        std::cout << "slot " << it->second << "\n";
    std::cout << "count(answer)=" << symbols.count("answer") << "\n";
    return 0;
}
```

---

### Configuration key-value store with string keys

Load or build settings as string keys with string values. `operator[]` inserts default-constructed values for missing keys; use `at` or `contains` when you must not create entries.

```cpp
#include <loki/assoc_vector.hpp>
#include <iostream>
#include <string>

int main() {
    loki::assoc_vector<std::string, std::string> config;
    config["server.host"] = "127.0.0.1";
    config["server.port"] = "8080";
    config.emplace("log.level", "info");

    try {
        std::cout << config.at("server.host") << "\n";
    } catch (const std::out_of_range&) {
        return 1;
    }
    return 0;
}
```

---

### Leaderboard / high-score table (score → player)

Use the numeric score as the key and the player name as the value. Keys are unique; if two players share a score, only one entry can exist unless you fold scores into the value side.

```cpp
#include <loki/assoc_vector.hpp>
#include <iostream>
#include <string>

int main() {
    loki::assoc_vector<int, std::string> scores;
    scores.insert({100, "Alice"});
    scores.insert({250, "Bob"});
    scores.insert({180, "Carol"});

    for (auto it = scores.rbegin(); it != scores.rend(); ++it)
        std::cout << it->second << ": " << it->first << "\n";
    return 0;
}
```

---

### Caching DNS lookups (domain → IP)

Remember resolver results in-process. `try_emplace` avoids overwriting a warm cache entry when you only want to fill missing names.

```cpp
#include <loki/assoc_vector.hpp>
#include <iostream>
#include <string>

int main() {
    loki::assoc_vector<std::string, std::string> cache;
    cache.try_emplace("example.com", "93.184.216.34");
    cache.try_emplace("example.com", "0.0.0.0"); // still original IP

    if (auto it = cache.find("example.com"); it != cache.end())
        std::cout << it->second << "\n";
    return 0;
}
```

---

### Inventory system (item ID → quantity)

Track stock by stable numeric item id. `operator[]` is convenient for incrementing counts because it creates a zero quantity for new ids.

```cpp
#include <loki/assoc_vector.hpp>
#include <iostream>

int main() {
    loki::assoc_vector<long long, int> inventory;
    inventory[1001] += 5;
    inventory[1002] = 1;
    inventory[1001] += 2;

    std::cout << inventory.at(1001) << "\n";
    inventory.erase(1002);
    std::cout << inventory.size() << "\n";
    return 0;
}
```

---

### Batch insert from a CSV parser

After parsing rows into a temporary sequence of `(key, value)` pairs, bulk-insert them in one call. The container sorts the new elements, merges them into the existing order, and collapses duplicate keys to a single entry.

```cpp
#include <loki/assoc_vector.hpp>
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::vector<std::pair<std::string, std::string>> rows;
    rows.push_back({"country", "US"});
    rows.push_back({"city", "Boston"});
    rows.push_back({"region", "NE"});

    loki::assoc_vector<std::string, std::string> table;
    table.insert(rows.begin(), rows.end());

    std::cout << table["city"] << "\n";
    std::cout << table.size() << "\n";
    return 0;
}
```

---

### Range queries with `lower_bound` / `upper_bound` (price range)

With price as the key, walk the subrange `[lower_bound(min), upper_bound(max))` to list offers in a budget. Use unique prices or encode uniqueness in the value type if needed.

```cpp
#include <loki/assoc_vector.hpp>
#include <iostream>

int main() {
    loki::assoc_vector<double, const char*> catalog;
    catalog.insert({9.99, "mug"});
    catalog.insert({19.50, "tee"});
    catalog.insert({29.00, "hoodie"});

    const double lo = 10.0;
    const double hi = 30.0;
    auto first = catalog.lower_bound(lo);
    auto last = catalog.upper_bound(hi);
    for (auto it = first; it != last; ++it)
        std::cout << it->second << ": " << it->first << "\n";
    return 0;
}
```

---

### Custom comparator (reverse order)

Use `std::greater<K>` so keys sort descending. Iteration from `begin()` visits largest keys first.

```cpp
#include <functional>
#include <iostream>
#include <loki/assoc_vector.hpp>

int main() {
    loki::assoc_vector<int, const char*, std::greater<int>> rev;
    rev.insert({1, "low"});
    rev.insert({3, "high"});
    rev.insert({2, "mid"});

    for (const auto& p : rev)
        std::cout << p.first << ' ' << p.second << "\n";
    return 0;
}
```

---

### Merging two `assoc_vector`s

Copy entries from a secondary map into the primary one with iterator-range `insert`. The implementation sorts and merges the new range, then deduplicates by key so only one pair per key remains (the first in sorted order).

```cpp
#include <iostream>
#include <loki/assoc_vector.hpp>
#include <string>

int main() {
    loki::assoc_vector<std::string, int> primary;
    primary["x"] = 1;
    primary["y"] = 2;

    loki::assoc_vector<std::string, int> secondary;
    secondary["y"] = 99;
    secondary["z"] = 3;

    primary.insert(secondary.begin(), secondary.end());

    std::cout << primary.at("y") << "\n";
    std::cout << primary.at("z") << "\n";
    return 0;
}
```