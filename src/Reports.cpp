#include "payroll/Reports.hpp"
#include <fstream>
#include <stdexcept>

namespace payroll {

namespace {
std::string hours(int minutes) {
    auto m = minutes % 60;
    return std::to_string(minutes / 60) + ":" + (m < 10 ? "0" : "") + std::to_string(m);
}

std::string csv(const std::string& raw) {
    // Екранування роздільника, лапок, переносів; захист від CSV formula injection.
    std::string value = raw;
    if (!value.empty() && (value[0] == '=' || value[0] == '+' || value[0] == '-' || value[0] == '@' ||
         value[0] == '\t' || value[0] == '\r')) value.insert(value.begin(), '\'');
    std::string result = "\"";
    for (auto ch : value) {
        if (ch == '"') result += "\"\"";
        else result += ch;
    }
    return result + "\"";
}
} // namespace

void Reports::statement(std::ostream& out, const PayrollRun& r) {
    out << "\n=== ВІДОМІСТЬ НАРАХУВАНЬ ЗА " << r.period << " ===\n"
        << "Версія (ID): " << r.id << "; створено (UTC): " << r.createdAt << "\n"
        << "Ставки утримань: " << r.settings.incomeTaxBp / 100.0
        << "% і " << r.settings.militaryTaxBp / 100.0 << "%\n";
    Money totalGross, totalTax, totalNet;
    for (const auto& p : r.slips) {
        const Money taxes = p.incomeTax + p.militaryTax + p.extraDeduction;
        out << "ID " << p.employeeId << " | " << p.employeeName << " | " << p.position
            << "\n  Нараховано: " << p.gross.str() << " грн; утримано: " << taxes.str()
            << " грн; до виплати: " << p.net.str() << " грн\n";
        totalGross = totalGross + p.gross;
        totalTax = totalTax + taxes;
        totalNet = totalNet + p.net;
    }
    out << "РАЗОМ: " << r.slips.size() << " осіб; нараховано " << totalGross.str()
        << " грн; утримано " << totalTax.str() << " грн; до виплати "
        << totalNet.str() << " грн\n";
}

void Reports::payslip(std::ostream& out, const PayrollRun& r, int employeeId) {
    for (const auto& p : r.slips) {
        if (p.employeeId != employeeId) {
            continue;
        }
        out << "\n=== РОЗРАХУНКОВИЙ ЛИСТ ===\n"
            << "Відомість: " << r.id << " | Період: " << r.period << " | Створено (UTC): " << r.createdAt << "\n"
            << "Працівник: " << p.employeeName << " (ID " << p.employeeId << ")\n"
            << "Посада: " << p.position << "\n"
            << "Оплата: " << (p.kind == PayKind::Hourly ? "погодинна" : "оклад")
            << "; ставка: " << p.rate.str() << " грн\n"
            << "Норма: " << hours(p.work.normMinutes) << "; звичайні години: "
            << hours(p.work.regularMinutes) << "; понаднормові: "
            << hours(p.work.overtimeMinutes) << " (коеф. 1.5)\n"
            << "Основна оплата:       " << p.regular.str() << " грн\n"
            << "Понаднормова:         " << p.overtime.str() << " грн\n"
            << "Премії:              " << p.bonus.str() << " грн\n"
            << "Нараховано:          " << p.gross.str() << " грн\n"
            << "Утримання 1 (" << r.settings.incomeTaxBp / 100.0 << "%): " << p.incomeTax.str() << " грн\n"
            << "Утримання 2 (" << r.settings.militaryTaxBp / 100.0 << "%): " << p.militaryTax.str() << " грн\n"
            << "Інші утримання:      " << p.extraDeduction.str() << " грн\n"
            << "ДО ВИПЛАТИ:          " << p.net.str() << " грн\n";
        return;
    }
    throw std::invalid_argument("У цій відомості немає працівника з указаним ID");
}

void Reports::exportCsv(const PayrollRun& run, const std::string& filename) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Не вдалося створити CSV-файл");
    }
    out << "\xEF\xBB\xBF"; // BOM для коректного відкриття кирилиці в Excel.
    out << "Відомість;Період;ID працівника;ПІБ;Посада;Основна оплата;Понаднормові;Премія;Нараховано;"
           "Утримання 1;Утримання 2;Інші утримання;До виплати\r\n";
    for (const auto& p : run.slips) {
        out << run.id << ';' << run.period << ';' << p.employeeId << ';'
            << csv(p.employeeName) << ';' << csv(p.position) << ';'
            << p.regular.str() << ';' << p.overtime.str() << ';' << p.bonus.str() << ';'
            << p.gross.str() << ';' << p.incomeTax.str() << ';' << p.militaryTax.str() << ';'
            << p.extraDeduction.str() << ';' << p.net.str() << "\r\n";
    }
    if (!out) {
        throw std::runtime_error("Помилка запису CSV");
    }
}
} // namespace payroll
