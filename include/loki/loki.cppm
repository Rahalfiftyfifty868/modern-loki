// C++20 Named Module interface for Modern Loki.
//
// Usage:   import loki;
// Fallback: #include <loki/loki.hpp>  (for compilers without module support)
//
// This file provides a named module wrapper around the header-only library.
// All standard library headers are pre-included in the global module fragment
// so that #include directives inside the library headers become no-ops.

module;

#include <algorithm>
#include <atomic>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

export module loki;

export extern "C++" {
#include "typelist.hpp"
#include "type_traits.hpp"
#include "threads.hpp"
#include "singleton.hpp"
#include "smart_ptr.hpp"
#include "functor.hpp"
#include "factory.hpp"
#include "abstract_factory.hpp"
#include "visitor.hpp"
#include "multi_methods.hpp"
#include "small_obj.hpp"
#include "assoc_vector.hpp"
#include "hierarchy_generators.hpp"
}
