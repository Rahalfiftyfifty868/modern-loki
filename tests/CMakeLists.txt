add_executable(loki_tests
    test_typelist.cpp
    test_type_traits.cpp
    test_singleton.cpp
    test_functor.cpp
    test_factory.cpp
    test_abstract_factory.cpp
    test_visitor.cpp
    test_multi_methods.cpp
    test_assoc_vector.cpp
    test_smart_ptr.cpp
    test_hierarchy_generators.cpp
    test_threads.cpp
    test_small_obj.cpp
)

target_link_libraries(loki_tests PRIVATE modern_loki)
target_include_directories(loki_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(loki_tests PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(loki_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Full suite
add_test(NAME all COMMAND loki_tests)

# Per-component targets using doctest --test-case filter.
# Filters use doctest's comma-separated wildcard syntax.
add_test(NAME typelist           COMMAND loki_tests "--test-case=typelist *")
add_test(NAME type_traits        COMMAND loki_tests "--test-case=type_traits *,type2type *,conversion *")
add_test(NAME singleton          COMMAND loki_tests "--test-case=singleton*,meyers_singleton*,create_using_pmr*")
add_test(NAME functor            COMMAND loki_tests "--test-case=functor *,make_functor*,bind_first,chain,functor_ref *")
add_test(NAME factory            COMMAND loki_tests "--test-case=factory *,clone_factory")
add_test(NAME abstract_factory   COMMAND loki_tests "--test-case=concrete_factory*,simple_concrete_factory*,abstract_factory*")
add_test(NAME visitor            COMMAND loki_tests "--test-case=acyclic visitor*,variant_visit,overloaded pattern,multi_visitor")
add_test(NAME multi_methods      COMMAND loki_tests "--test-case=basic_dispatcher*,fn_dispatcher*,static_dispatcher*,variant_dispatch*")
add_test(NAME assoc_vector       COMMAND loki_tests "--test-case=assoc_vector *")
add_test(NAME smart_ptr          COMMAND loki_tests "--test-case=smart_ptr *")
add_test(NAME hierarchy_generators COMMAND loki_tests "--test-case=gen_scatter_hierarchy*,gen_linear_hierarchy*,tuple_unit*,apply_to_all*")
add_test(NAME threads            COMMAND loki_tests "--test-case=single_threaded*,object_level_lockable*,class_level_lockable*,rw_lockable*")
add_test(NAME small_obj          COMMAND loki_tests "--test-case=fixed_allocator*,small_obj_allocator*,small_object*")
