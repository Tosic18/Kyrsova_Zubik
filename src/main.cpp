#include "payroll/ConsoleApp.hpp"
#include "payroll/Repository.hpp"
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    try {
        // Аргумент 1 — шлях до SQLite; без аргументу база створюється біля запуску.
        const std::string file = argc > 1 ? argv[1] : "payroll.db";
        payroll::SQLiteRepository repository(file);
        payroll::PayrollService service(repository);
        payroll::ConsoleApp app(service);
        app.run();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Критична помилка: " << ex.what() << '\n';
        return 1;
    }
}
