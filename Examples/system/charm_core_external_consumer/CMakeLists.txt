cmake_minimum_required(VERSION 4.0)
project(charm-core-external-consumer LANGUAGES CXX)

find_package(CharmCore CONFIG REQUIRED)

add_executable(charm-core-external-consumer main.cpp)
target_link_libraries(charm-core-external-consumer PRIVATE Charm::core)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(charm-core-external-consumer PRIVATE
        -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME charm_core_external_consumer
         COMMAND charm-core-external-consumer)
set_tests_properties(charm_core_external_consumer PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[charm-core-external-consumer\\] ok")
