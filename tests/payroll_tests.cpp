#include "payroll/PayrollService.hpp"
#include "payroll/PayPolicy.hpp"
#include "payroll/Reports.hpp"
#include "payroll/ConsoleApp.hpp"

#include <filesystem>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace payroll;
namespace {
int cases = 0;
void require(bool condition, const std::string& msg) {
    ++cases;
    if (!condition) {
        throw std::runtime_error("Тест не пройдено: " + msg);
    }
}

template <class F>
void mustThrow(F&& func, const std::string& msg) {
    try {
        func();
    } catch (const std::exception&) {
        require(true, msg);
        return;
    }
    require(false, msg);
}

void moneyTests() {
    require(Money::parse("102.05").kopecks() == 10205, "парсинг суми з копійками");
    require(Money::parse(" 12,5 ").str() == "12.50", "парсинг із комою");
    require(Money::parse("0").str() == "0.00", "нуль");
    require((Money(120) + Money(30)).kopecks() == 150, "додавання");
    require((Money(120) - Money(30)).kopecks() == 90, "віднімання");
    require(roundRatio(1, 1, 2) == 1, "половина копійки в більший бік");
    require(roundRatio(10000, 1800, 10000) == 1800, "18 відсотків");
    mustThrow([]{ Money::parse("100.999"); }, "зайві копійки");
    mustThrow([]{ Money::parse("-5"); }, "від'ємна сума");
    mustThrow([]{ Money(2)-Money(3); }, "від'ємний результат");
    mustThrow([]{ validatePeriod("2026-13"); }, "помилковий місяць");
    mustThrow([]{ validateWork({1, "2026-10", 10000, 0, 9600}); }, "перевищення норми");
    mustThrow([]{ validateTaxSettings({9000, 2000}); }, "відсотки понад 100");
}

void strategyTests() {
    const WorkRecord w{1, "2026-10", 4800, 240, 9600};
    auto hourly = PayPolicyFactory::create(PayKind::Hourly);
    require(hourly->kind() == PayKind::Hourly, "тип фабричного об'єкта");
    const auto h = hourly->calculate(Money::parse("200"), w);
    require(h.regular == Money::parse("16000"), "80 годин * 200");
    require(h.overtime == Money::parse("1200"), "4 години * 200 * 1.5");
    auto salaried = PayPolicyFactory::create(PayKind::Salaried);
    const auto s = salaried->calculate(Money::parse("30000"), w);
    require(s.regular == Money::parse("15000"), "оклад за половину норми");
    require(s.overtime == Money::parse("1125"), "понаднормові від окладу");
    const WorkRecord partial{1, "2026-10", 1, 0, 9600};
    require(salaried->calculate(Money::parse("16000"), partial).regular == Money::parse("1.67"),
            "округлення часткової години");
}

void integrationTests() {
    SQLiteRepository db(":memory:");
    PayrollService svc(db);
    auto one = svc.hire({0, "Олена Тестова", "Бухгалтер", PayKind::Salaried, Money::parse("30000"), true});
    auto two = svc.hire({0, "Андрій Приклад", "Менеджер", PayKind::Hourly, Money::parse("200"), true});
    require(one == 1 && two == 2, "ID працівників");
    require(db.searchEmployees("Тестова").size() == 1, "пошук ПІБ");
    require(db.employees().size() == 2, "активні працівники");
    svc.registerWork({one, "2026-10", 9600, 480, 9600});
    mustThrow([&]{ svc.calculate("2026-10"); }, "немає табеля другого працівника");
    require(db.history().empty(), "відсутні часткові нарахування");
    svc.registerWork({two, "2026-10", 4800, 240, 9600});
    svc.registerAdjustment({0, one, "2026-10", AdjustmentKind::Bonus, Money::parse("1000"), "Результати роботи"});
    svc.registerAdjustment({0, one, "2026-10", AdjustmentKind::Deduction, Money::parse("300"), "Аванс"});
    auto bonusId = svc.registerAdjustment({0, two, "2026-10", AdjustmentKind::Bonus, Money::parse("500"), "Премія"});
    auto r1 = svc.calculate("2026-10");
    require(r1.id == 1 && r1.slips.size() == 2, "відомість із двох осіб");
    require(r1.slips[0].regular == Money::parse("30000"), "оклад за норму");
    require(r1.slips[0].overtime == Money::parse("2250"), "понаднормові окладника");
    require(r1.slips[0].gross == Money::parse("33250"), "нарахування окладника");
    require(r1.slips[0].incomeTax == Money::parse("5985"), "утримання 1, 18%");
    require(r1.slips[0].militaryTax == Money::parse("1662.50"), "утримання 2, 5%");
    require(r1.slips[0].net == Money::parse("25302.50"), "сума на руки окладника");
    require(r1.slips[1].gross == Money::parse("17700"), "нарахування погодинного");
    require(r1.slips[1].net == Money::parse("13629"), "погодинний до виплати");
    require(db.latestRun("2026-10")->id == 1, "останній знімок");
    require(db.history().size() == 1, "історія");
    std::ostringstream report;
    Reports::statement(report, r1);
    require(report.str().find("25302.50") != std::string::npos, "відомість на екрані");
    std::ostringstream slip;
    Reports::payslip(slip, r1, one);
    require(slip.str().find("Олена Тестова") != std::string::npos, "розрахунковий лист");
    mustThrow([&]{ Reports::payslip(slip, r1, 999); }, "відсутній працівник у звіті");

    auto e = *db.employee(one);
    e.rate = Money::parse("40000");
    svc.edit(e);
    svc.configureTaxes({1000, 200});
    svc.removeAdjustment(bonusId);
    svc.archive(two);
    require(db.employees().size() == 1, "архівування");
    require(db.employees(true).size() == 2, "перегляд з архівом");
    auto r2 = svc.calculate("2026-10");
    require(r2.id == 2 && r2.slips.size() == 1, "нова версія і виключення архівного");
    require(r2.slips[0].gross == Money::parse("44000"), "новий оклад у другій версії");
    require(r2.slips[0].net == Money::parse("38420"), "нові утримання");
    require(db.run(1)->slips[0].net == Money::parse("25302.50"), "старий знімок не змінився");
    require(db.run(1)->slips[1].employeeName == "Андрій Приклад", "архівний працівник у старій версії");
    require(db.latestRun("2026-10")->id == 2, "остання версія");
    require(db.history().size() == 2, "історія обох версій");
    mustThrow([&]{ svc.registerWork({two, "2026-11", 9600, 0, 9600}); }, "не можна обліковувати архівного");
    mustThrow([&]{ svc.registerAdjustment({0, one, "2026-10", AdjustmentKind::Deduction,
                                             Money::parse("10000000001"), "завелика сума"}); },
              "перевищення максимальної суми коригування");
    svc.registerAdjustment({0, one, "2026-10", AdjustmentKind::Deduction,
                            Money::parse("1000000000"), "значне утримання"});
    mustThrow([&]{ svc.calculate("2026-10"); }, "відрахування понад нарахування");
    require(db.history().size() == 2, "помилка не створила відомість");
}

void persistenceAndCsvTests() {
    const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto filename = (std::filesystem::temp_directory_path() / ("payroll_oop_test_" + suffix + ".db")).string();
    const auto reportPath = (std::filesystem::temp_directory_path() / ("payroll_oop_report_" + suffix + ".csv")).string();
    {
        SQLiteRepository db(filename);
        PayrollService service(db);
        const int id = service.hire({0, "=FORMULA()", "QA, тест", PayKind::Hourly, Money::parse("100"), true});
        service.registerWork({id, "2026-09", 60, 0, 9600});
        auto run = service.calculate("2026-09");
        Reports::exportCsv(run, reportPath);
    }
    {
        SQLiteRepository db(filename);
        require(db.employees().size() == 1, "SQLite після перезапуску");
        require(db.latestRun("2026-09")->slips[0].net == Money::parse("77"), "знімок після перезапуску");
    }
    {
        std::ifstream f(reportPath, std::ios::binary);
        std::string data((std::istreambuf_iterator<char>(f)), {});
        require(data.find("\xEF\xBB\xBF") == 0, "BOM у CSV");
        require(data.find("\"'=FORMULA()\"") != std::string::npos, "захист від формул у CSV");
    }
    std::filesystem::remove(filename);
    std::filesystem::remove(reportPath);
}

// Регресія: при завершенні потоку у вкладеному меню застосунок має завершуватися,
// а не нескінченно друкувати помилку введення.
std::string runConsoleScenario(const std::string& commands) {
    SQLiteRepository db(":memory:");
    PayrollService service(db);
    ConsoleApp app(service);

    std::istringstream input(commands);
    std::ostringstream output;
    auto* originalInput = std::cin.rdbuf(input.rdbuf());
    auto* originalOutput = std::cout.rdbuf(output.rdbuf());
    std::cin.clear();

    try {
        app.run();
    } catch (...) {
        std::cin.rdbuf(originalInput);
        std::cin.clear();
        std::cout.rdbuf(originalOutput);
        throw;
    }

    std::cin.rdbuf(originalInput);
    std::cin.clear();
    std::cout.rdbuf(originalOutput);
    return output.str();
}

void consoleTests() {
    const auto partialEmployee = runConsoleScenario("1\n3\n");
    require(partialEmployee.find("Введення завершено") != std::string::npos,
            "завершення введення всередині меню працівників");

    const auto partialAdjustment = runConsoleScenario("3\n1\n");
    require(partialAdjustment.find("Введення завершено") != std::string::npos,
            "завершення введення всередині меню премій");

    const auto ordinaryExit = runConsoleScenario("0\n");
    require(ordinaryExit.find("Роботу завершено") != std::string::npos,
            "штатний вихід через пункт 0");
}

} // namespace

int main() {
    try {
        moneyTests();
        strategyTests();
        integrationTests();
        persistenceAndCsvTests();
        consoleTests();
        std::cout << "OK: " << cases << " перевірок пройдено.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
