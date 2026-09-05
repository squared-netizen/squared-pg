# The engine: native domain capability (§2.2).
#
# Two targets. squared_pg_engine is the control surface and knows nothing about
# Lua; squared_pg_lua is the binding layer and is the only thing here that has
# ever seen a lua_State. Keeping them separate is what makes §2.1.6's promise
# real — an embedder may link the engine alone and never pay for a Lua runtime.

add_library(squared_pg_engine STATIC
    src/value.cpp
    src/version.cpp
    src/identity.cpp
    src/error.cpp
    src/engine.cpp
    src/support/support.cpp
    src/services/filesystem_service.cpp
    src/services/resource_service.cpp
    src/services/resolve_services.cpp
    src/services/validation_service.cpp
    src/services/project_service.cpp
    src/services/transaction_service.cpp)

add_library(squared::pg ALIAS squared_pg_engine)

# include/ is public; src/ is not. §1.3 requires the boundary to be explicit,
# and an include path is a stronger statement of it than a review convention.
target_include_directories(squared_pg_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(squared_pg_engine
    PUBLIC  sqcart::read
    PRIVATE yyjson squared_pg_sha256)

target_compile_definitions(squared_pg_engine PRIVATE
    SQUARED_PG_VERSION="${SQUARED_PG_VERSION}")

target_compile_options(squared_pg_engine PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)

# ---------------------------------------------------------------------------
# The Lua binding layer (§2.6)
# ---------------------------------------------------------------------------

add_library(squared_pg_lua STATIC lua/src/bindings.cpp)
add_library(squared::pg_lua ALIAS squared_pg_lua)

target_include_directories(squared_pg_lua
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lua/include>)

target_link_libraries(squared_pg_lua PUBLIC squared_pg_engine lua)

target_compile_options(squared_pg_lua PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)
