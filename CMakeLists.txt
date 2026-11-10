cmake_minimum_required(VERSION 3.16)
project(PayrollOOP VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)

add_library(payroll_core
    src/Domain.cpp
    src/PayPolicy.cpp
    src/SQLiteRepository.cpp
    src/PayrollService.cpp
    src/Reports.cpp
    src/ConsoleApp.cpp
)
target_include_directories(payroll_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(payroll_core PUBLIC SQLite::SQLite3)
if (MSVC)
    target_compile_options(payroll_core PRIVATE /W4)
else()
    target_compile_options(payroll_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(payroll src/main.cpp)
target_link_libraries(payroll PRIVATE payroll_core)

include(CTest)
if (BUILD_TESTING)
    add_executable(payroll_tests tests/payroll_tests.cpp)
    target_link_libraries(payroll_tests PRIVATE payroll_core)
    add_test(NAME payroll_tests COMMAND payroll_tests)
endif()
