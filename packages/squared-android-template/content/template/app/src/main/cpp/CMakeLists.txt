cmake_minimum_required(VERSION 3.20)

project({{PROJECT_ID}} LANGUAGES C CXX)

foreach(required_variable SDL2_ROOT LUA_ROOT PROJECT_ROOT)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} was not provided")
    endif()
endforeach()

set(SDL2_INCLUDE_DIR "${SDL2_ROOT}/include/SDL2")
set(SDL2_LIBRARY_DIR "${SDL2_ROOT}/lib/arm64-v8a")

foreach(component SDL2 SDL2_ttf SDL2_mixer SDL2_image SDL2_net)
    set(component_library "${SDL2_LIBRARY_DIR}/lib${component}.so")
    if(NOT EXISTS "${component_library}")
        message(FATAL_ERROR "Library not found: ${component_library}")
    endif()

    add_library(${component} SHARED IMPORTED GLOBAL)
    set_target_properties(${component} PROPERTIES
        IMPORTED_LOCATION "${component_library}"
    )
endforeach()

if(NOT EXISTS "${SDL2_INCLUDE_DIR}/SDL.h")
    message(FATAL_ERROR "SDL2 headers not found: ${SDL2_INCLUDE_DIR}")
endif()

find_library(SQUARED_GLES2_LIBRARY NAMES GLESv2)
if(NOT SQUARED_GLES2_LIBRARY)
    foreach(candidate
        "/system/lib64/libGLESv2.so"
        "/system/lib/libGLESv2.so"
    )
        if(EXISTS "${candidate}")
            set(SQUARED_GLES2_LIBRARY "${candidate}")
            break()
        endif()
    endforeach()
endif()
if(NOT SQUARED_GLES2_LIBRARY)
    message(FATAL_ERROR
        "OpenGL ES 2 link library not found; expected an Android NDK "
        "GLESv2 stub or /system/lib64/libGLESv2.so"
    )
endif()
message(STATUS
    "Squared OpenGL ES 2 library: ${SQUARED_GLES2_LIBRARY}"
)

file(GLOB LUA_SOURCES CONFIGURE_DEPENDS "${LUA_ROOT}/src/*.c")
list(FILTER LUA_SOURCES EXCLUDE REGEX "/lua\\.c$")
list(FILTER LUA_SOURCES EXCLUDE REGEX "/luac\\.c$")

add_library(lua_runtime STATIC ${LUA_SOURCES})
set_target_properties(lua_runtime PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
target_compile_definitions(lua_runtime PRIVATE LUA_USE_LINUX)
target_include_directories(lua_runtime PUBLIC "${LUA_ROOT}/src")

set_property(GLOBAL PROPERTY SQUARED_MODULE_TARGETS "")

function(squared_register_module target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR
            "Squared module attempted to register missing target: ${target}"
        )
    endif()
    set_property(GLOBAL APPEND PROPERTY
        SQUARED_MODULE_TARGETS "${target}"
    )
endfunction()

file(GLOB SQUARED_MODULE_DIRECTORIES
    CONFIGURE_DEPENDS
    LIST_DIRECTORIES true
    "${PROJECT_ROOT}/modules/*"
)
list(SORT SQUARED_MODULE_DIRECTORIES)
foreach(module_directory IN LISTS SQUARED_MODULE_DIRECTORIES)
    if(IS_DIRECTORY "${module_directory}" AND
       EXISTS "${module_directory}/CMakeLists.txt")
        get_filename_component(module_name "${module_directory}" NAME)
        add_subdirectory(
            "${module_directory}"
            "${CMAKE_CURRENT_BINARY_DIR}/modules/${module_name}"
        )
    endif()
endforeach()

get_property(SQUARED_MODULE_TARGETS GLOBAL
    PROPERTY SQUARED_MODULE_TARGETS
)
if(NOT TARGET squared_application)
    message(FATAL_ERROR
        "The default Android profile requires the Squared Application module"
    )
endif()
if(NOT TARGET squared_graphics)
    message(FATAL_ERROR
        "The default Android profile requires the Squared Graphics module"
    )
endif()
if(NOT TARGET squared_math)
    message(FATAL_ERROR
        "The default Android profile requires the Squared Math module"
    )
endif()
if(NOT TARGET squared_graphics2d)
    message(FATAL_ERROR
        "The default Android profile requires the Squared Graphics2D module"
    )
endif()
if(NOT TARGET squared_scene2d)
    message(FATAL_ERROR
        "The default Android profile requires the Squared Scene2D module"
    )
endif()
if(NOT TARGET squared_messaging)
    message(FATAL_ERROR
        "The default Android profile requires the Squared Messaging module"
    )
endif()

add_subdirectory(application)

add_library(main SHARED
    platform/sdl_main.cpp
)

target_compile_features(main PRIVATE cxx_std_20)
target_compile_options(main PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
target_include_directories(main PRIVATE
    "${SDL2_INCLUDE_DIR}"
)
target_link_libraries(main PRIVATE
    project_application
    SDL2_image
    SDL2_mixer
    SDL2_ttf
    SDL2_net
    SDL2
    dl
    m
)
set_target_properties(main PROPERTIES OUTPUT_NAME "main")
