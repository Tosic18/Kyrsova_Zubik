cmake_minimum_required(VERSION 3.16)
project(PayrollOOP VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Створюємо бібліотеку core разом із sqlite3.c
add_library(payroll_core
    src/Domain.cpp
    src/PayPolicy.cpp
    src/SQLiteRepository.cpp
    src/PayrollService.cpp
    src/Reports.cpp
    src/ConsoleApp.cpp
    src/sqlite3.c
)

# Підключаємо папки з заголовочними файлами
target_include_directories(payroll_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/payroll
)

# Обов'язкові системні бібліотеки для SQLite у Windows/MSVC
if(WIN32)
    target_link_libraries(payroll_core PUBLIC shell32)
endif()

# Головний виконуваний файл програми
add_executable(payroll src/main.cpp)
target_link_libraries(payroll PRIVATE payroll_core)

# Тести (якщо є)
enable_testing()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/payroll_tests.cpp")
    add_executable(payroll_tests tests/payroll_tests.cpp)
    target_link_libraries(payroll_tests PRIVATE payroll_core)
    add_test(NAME payroll_tests COMMAND payroll_tests)
endif()