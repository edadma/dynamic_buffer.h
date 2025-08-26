cmake_minimum_required(VERSION 3.10)

project(dynamic_buffer 
    VERSION 0.1.0
    DESCRIPTION "Reference-counted byte buffer library for efficient I/O operations"
    LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG")
endif()

# Header-only library target
add_library(dynamic_buffer INTERFACE)
target_include_directories(dynamic_buffer INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Unity test framework
add_library(unity STATIC
    libs/unity/unity.c
)
target_include_directories(unity PUBLIC libs/unity)

# Test executable
add_executable(tests
    test.c
)
target_link_libraries(tests PRIVATE dynamic_buffer unity)
target_compile_definitions(tests PRIVATE DB_IMPLEMENTATION)

# Enable testing
enable_testing()
add_test(NAME dynamic_buffer_tests COMMAND tests)

# Install configuration
install(FILES dynamic_buffer.h
    DESTINATION include
)

install(TARGETS dynamic_buffer
    EXPORT dynamic_bufferTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(EXPORT dynamic_bufferTargets
    FILE dynamic_bufferTargets.cmake
    NAMESPACE dynamic_buffer::
    DESTINATION lib/cmake/dynamic_buffer
)

# Create config files
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    dynamic_bufferConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/dynamic_bufferConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/dynamic_bufferConfig.cmake
    INSTALL_DESTINATION lib/cmake/dynamic_buffer
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/dynamic_bufferConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/dynamic_bufferConfigVersion.cmake
    DESTINATION lib/cmake/dynamic_buffer
)