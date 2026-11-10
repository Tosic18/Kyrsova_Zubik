#include "payroll/ConsoleApp.hpp"
#include "payroll/Reports.hpp"
#include <cctype>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace payroll {

namespace {

class EndOfInput final : public std::runtime_error {
public:
    EndOfInput() : std::runtime_error("Введення завершено") {}
};

std::string hourText(int minutes) {
    return std::to_string(minutes / 60) + ":" +
           (minutes % 60 < 10 ? "0" : "") + std::to_string(minutes % 60);
}
} // namespace

std::string ConsoleApp::read(const std::string& prompt) {
    std::cout << prompt;
    std::string text;
    if (!std::getline(std::cin, text)) {
        throw EndOfInput();
    }
    return text;
}

int ConsoleApp::readInt(const std::string& prompt, int lower, int upper) {
    const auto s = read(prompt);
    std::size_t position = 0;
    try {
        auto n = std::stoll(s, &position);
        if (position != s.size() || n < lower || n > upper)
            throw std::invalid_argument("межі");
        return static_cast<int>(n);
    } catch (const std::exception&) {
        throw std::invalid_argument("Очікувалося ціле число від " + std::to_string(lower) +
                                    " до " + std::to_string(upper));
    }
}

Money ConsoleApp::readMoney(const std::string& prompt) {
    return Money::parse(read(prompt));
}

int ConsoleApp::readMinutes(const std::string& prompt) {
    auto text = read(prompt + " (ГГ:ХХ): ");
    auto colon = text.find(':');
    if (colon == std::string::npos || colon != text.rfind(':'))
        throw std::invalid_argument("Час задається як ГГ:ХХ, наприклад 160:00");
    const auto lhs = text.substr(0, colon);
    const auto rhs = text.substr(colon+1);
    if (lhs.empty() || rhs.size() != 2) {
        throw std::invalid_argument("Невірний час");
    }
    for (char c : lhs + rhs)
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            throw std::invalid_argument("Невірний час");
        }
    if (lhs.size() > 3) {
        throw std::invalid_argument("Надто великий час");
    }
    auto hour = std::stoi(lhs), minute = std::stoi(rhs);
    if (hour > 744 || minute > 59 || 60 * hour + minute > 44640)
        throw std::invalid_argument("Час перевищує допустимий ліміт");
    return 60 * hour + minute;
}

PayKind ConsoleApp::readPayKind() {
    const auto n = readInt("Тип оплати (1 — погодинна; 2 — оклад): ", 1, 2);
    return n == 1 ? PayKind::Hourly : PayKind::Salaried;
}

std::string ConsoleApp::readPeriod() {
    auto period = read("Місяць (РРРР-ММ): ");
    validatePeriod(period);
    return period;
}

void ConsoleApp::printEmployees(const std::vector<Employee>& list) {
    if (list.empty()) {
        std::cout << "Немає працівників.\n";
        return;
    }
    for (const auto& e : list)
        std::cout << "ID " << e.id << " | " << e.name << " | " << e.position
                  << " | " << (e.kind == PayKind::Hourly ? "погодинна" : "оклад")
                  << " | " << e.rate.str() << " грн | "
                  << (e.active ? "активний" : "архів") << "\n";
}

void ConsoleApp::employeeMenu() {
    for (;;) {
        std::cout << "\nПРАЦІВНИКИ: 1 — список; 2 — пошук; 3 — додати; 4 — змінити; 5 — архівувати; 0 — назад\n";
        int option = readInt("> ", 0, 5);
        if (option == 0) {
            return;
        }
        try {
            if (option == 1) {
                bool includeArchived = readInt("Показати архів? (1 — так, 0 — ні): ", 0, 1) != 0;
                printEmployees(service_.data().employees(includeArchived));
            } else if (option == 2) {
                auto keyword = read("Частина ПІБ або посади (пошук із врахуванням регістру): ");
                printEmployees(service_.data().searchEmployees(keyword));
            } else if (option == 3) {
                Employee e;
                e.name = read("ПІБ: ");
                e.position = read("Посада: ");
                e.kind = readPayKind();
                e.rate = readMoney(e.kind == PayKind::Hourly ? "Ставка (грн/год): " : "Оклад (грн/місяць): ");
                std::cout << "Створено працівника ID " << service_.hire(e) << "\n";
            } else if (option == 4) {
                int id = readInt("ID працівника: ", 1, 1000000000);
                auto existing = service_.data().employee(id);
                if (!existing || !existing->active) {
                    throw std::invalid_argument("Активного працівника не знайдено");
                }
                Employee e = *existing;
                std::cout << "Поточні дані:\n";
                printEmployees({e});
                auto name = read("Нове ПІБ (Enter — залишити): ");
                auto position = read("Нова посада (Enter — залишити): ");
                if (!name.empty()) e.name = name;
                if (!position.empty()) e.position = position;
                auto kind = read("Тип оплати (Enter — залишити, 1 — погодинна, 2 — оклад): ");
                if (!kind.empty()) {
                    if (kind != "1" && kind != "2") {
                        throw std::invalid_argument("Невірний тип оплати");
                    }
                    e.kind = kind == "1" ? PayKind::Hourly : PayKind::Salaried;
                }
                auto rate = read("Нова ставка (Enter — залишити): ");
                if (!rate.empty()) {
                    e.rate = Money::parse(rate);
                } else if (!kind.empty() && e.kind != existing->kind) {
                    throw std::invalid_argument("При зміні типу оплати потрібно вказати нову ставку");
                }
                service_.edit(e);
                std::cout << "Дані оновлено.\n";
            } else {
                int id = readInt("ID працівника: ", 1, 1000000000);
                if (read("Архівувати? (так/ні): ") == "так") {
                    service_.archive(id);
                    std::cout << "Працівника перенесено до архіву; історію збережено.\n";
                }
            }
        } catch (const EndOfInput&) {
            throw;
        } catch (const std::exception& e) {
            std::cout << "Помилка: " << e.what() << "\n";
        }
    }
}

void ConsoleApp::workMenu() {
    int id = readInt("ID працівника: ", 1, 1000000000);
    auto period = readPeriod();
    auto old = service_.data().work(id, period);
    if (old) {
        std::cout << "Збережений табель: звичайні " << hourText(old->regularMinutes)
                  << ", понаднормові " << hourText(old->overtimeMinutes)
                  << ", норма " << hourText(old->normMinutes) << ". Буде замінено.\n";
    }
    WorkRecord work;
    work.employeeId = id;
    work.period = period;
    work.normMinutes = readMinutes("Норма робочого часу");
    work.regularMinutes = readMinutes("Звичайний відпрацьований час");
    work.overtimeMinutes = readMinutes("Понаднормовий час");
    service_.registerWork(work);
    std::cout << "Табель збережено.\n";
}

void ConsoleApp::adjustmentsMenu() {
    for (;;) {
        std::cout << "\nПРЕМІЇ / УТРИМАННЯ: 1 — додати премію; 2 — додати утримання; "
                     "3 — переглянути; 4 — видалити запис; 0 — назад\n";
        auto option = readInt("> ", 0, 4);
        if (!option) {
            return;
        }
        try {
            if (option == 4) {
                service_.removeAdjustment(readInt("ID коригування: ", 1, 1000000000));
                std::cout << "Запис видалено (існуючі відомості не змінюються).\n";
            } else {
                int id = readInt("ID працівника: ", 1, 1000000000);
                auto period = readPeriod();
                if (option == 3) {
                    auto list = service_.data().adjustments(id, period);
                    if (list.empty()) {
                        std::cout << "Немає записів.\n";
                    }
                    for (const auto& a : list)
                        std::cout << "#" << a.id << " | "
                                  << (a.kind == AdjustmentKind::Bonus ? "премія" : "утримання")
                                  << " | " << a.amount.str() << " грн | " << a.note << "\n";
                } else {
                    Adjustment a;
                    a.employeeId = id;
                    a.period = period;
                    a.kind = option == 1 ? AdjustmentKind::Bonus : AdjustmentKind::Deduction;
                    a.amount = readMoney("Сума (грн): ");
                    a.note = read("Примітка: ");
                    std::cout << "Додано коригування #" << service_.registerAdjustment(a) << "\n";
                }
            }
        } catch (const EndOfInput&) {
            throw;
        } catch (const std::exception& e) {
            std::cout << "Помилка: " << e.what() << "\n";
        }
    }
}

void ConsoleApp::taxMenu() {
    auto current = service_.data().taxSettings();
    std::cout << "Поточні ставки: 1) " << current.incomeTaxBp / 100.0
              << "%; 2) " << current.militaryTaxBp / 100.0 << "%\n";
    std::cout << "Навчальна модель! Перевіряйте ставки і правила обліку для реальних виплат.\n";
    auto a = readMoney("Утримання 1 (%): ");
    auto b = readMoney("Утримання 2 (%): ");
    if (a.kopecks() > 10000 || b.kopecks() > 10000)
        throw std::invalid_argument("Відсоток не може перевищувати 100");
    service_.configureTaxes({static_cast<int>(a.kopecks()), static_cast<int>(b.kopecks())});
    std::cout << "Ставки збережено.\n";
}

void ConsoleApp::generate() {
    const auto period = readPeriod();
    std::cout << "Нова відомість створює нову версію, не перезаписує історію.\n";
    auto r = service_.calculate(period);
    Reports::statement(std::cout, r);
}

void ConsoleApp::statement() {
    const auto period = readPeriod();
    const auto run = service_.data().latestRun(period);
    if (!run) {
        std::cout << "Нарахування за цей місяць відсутні.\n";
        return;
    }
    Reports::statement(std::cout, *run);
}

void ConsoleApp::payslip() {
    auto id = readInt("ID відомості: ", 1, 1000000000);
    auto r = service_.data().run(id);
    if (!r) {
        throw std::invalid_argument("Відомість не знайдено");
    }
    auto employee = readInt("ID працівника: ", 1, 1000000000);
    Reports::payslip(std::cout, *r, employee);
}

void ConsoleApp::history() {
    auto list = service_.data().history();
    if (list.empty()) {
        std::cout << "Історія порожня.\n";
        return;
    }
    for (const auto& r : list)
        std::cout << "Відомість #" << r.id << " | " << r.period
                  << " | " << r.createdAt << " UTC | осіб: " << r.employeeCount << "\n";
    auto id = readInt("ID для перегляду (0 — назад): ", 0, 1000000000);
    if (!id) {
        return;
    }
    auto run = service_.data().run(id);
    if (!run) {
        throw std::invalid_argument("Відомість не знайдено");
    }
    Reports::statement(std::cout, *run);
}

void ConsoleApp::exportCsv() {
    auto id = readInt("ID відомості: ", 1, 1000000000);
    auto r = service_.data().run(id);
    if (!r) {
        throw std::invalid_argument("Відомість не знайдено");
    }
    const auto filename = "payroll_" + r->period + "_run_" + std::to_string(id) + ".csv";
    Reports::exportCsv(*r, filename);
    std::cout << "Збережено CSV у поточній папці: " << filename << "\n";
}

void ConsoleApp::run() {
    std::cout << "=== СИСТЕМА НАРАХУВАННЯ ЗАРОБІТНОЇ ПЛАТИ (навчальна модель) ===\n";
    for (;;) {
        std::cout << "\n1 — Працівники\n2 — Табель часу\n3 — Премії та утримання\n"
                     "4 — Ставки утримань\n5 — Нарахувати за місяць\n"
                     "6 — Відомість за місяць\n7 — Розрахунковий лист\n"
                     "8 — Історія нарахувань\n9 — Експорт CSV\n0 — Вихід\n";
        try {
            auto option = readInt("> ", 0, 9);
            switch (option) {
                case 0:
                    std::cout << "Роботу завершено.\n";
                    return;
                case 1:
                    employeeMenu();
                    break;
                case 2:
                    workMenu();
                    break;
                case 3:
                    adjustmentsMenu();
                    break;
                case 4:
                    taxMenu();
                    break;
                case 5:
                    generate();
                    break;
                case 6:
                    statement();
                    break;
                case 7:
                    payslip();
                    break;
                case 8:
                    history();
                    break;
                case 9:
                    exportCsv();
                    break;
            }
        } catch (const EndOfInput&) {
            std::cout << "\nВведення завершено.\n";
            return;
        } catch (const std::exception& e) {
            std::cout << "Помилка: " << e.what() << "\n";
        }
    }
}
} // namespace payroll
