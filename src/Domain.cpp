#include "payroll/Domain.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace payroll {

namespace {
constexpr std::int64_t kMaxInputMoney = 1'000'000'000'000LL; // 10 млрд грн.
std::string trim(const std::string& s) {
    auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

void boundedText(const std::string& s, const char* field, std::size_t maximum) {
    if (trim(s).empty() || s.size() > maximum ||
        std::any_of(s.begin(), s.end(), [](unsigned char c) { return c < 32; })) {
        throw std::invalid_argument(std::string("Некоректне поле: ") + field);
    }
}
} // namespace

Money::Money(std::int64_t kopecks) : kopecks_(kopecks) {
    if (kopecks < 0) {
        throw std::invalid_argument("Сума не може бути від'ємною");
    }
}

Money Money::parse(const std::string& raw) {
    auto s = trim(raw);
    if (s.empty()) {
        throw std::invalid_argument("Введіть суму");
    }
    std::replace(s.begin(), s.end(), ',', '.');
    const auto dot = s.find('.');
    if (dot != std::string::npos && s.find('.', dot + 1) != std::string::npos)
        throw std::invalid_argument("Некоректний формат суми");
    auto integer = s.substr(0, dot);
    auto decimal = dot == std::string::npos ? "" : s.substr(dot + 1);
    if (integer.empty() || (dot != std::string::npos && decimal.empty()) || decimal.size() > 2)
        throw std::invalid_argument("Вкажіть до двох знаків після коми");
    auto digits = [](const std::string& x) {
        return std::all_of(x.begin(), x.end(), [](unsigned char c) { return std::isdigit(c); });
    };
    if (!digits(integer) || !digits(decimal)) {
        throw std::invalid_argument("Сума містить недопустимі символи");
    }
    std::int64_t major;
    try {
        major = std::stoll(integer);
    } catch (const std::exception&) {
        throw std::invalid_argument("Завелика сума");
    }
    if (major > kMaxInputMoney / 100) {
        throw std::invalid_argument("Завелика сума");
    }
    int fractional = 0;
    if (!decimal.empty()) {
        fractional = decimal[0] - '0';
        fractional = fractional * 10 + (decimal.size() == 2 ? decimal[1] - '0' : 0);
    }
    const std::int64_t total = major * 100 + fractional;
    if (total > kMaxInputMoney) {
        throw std::invalid_argument("Завелика сума");
    }
    return Money(total);
}

std::string Money::str() const {
    std::ostringstream out;
    out << kopecks_ / 100 << '.' << std::setw(2) << std::setfill('0') << kopecks_ % 100;
    return out.str();
}

Money Money::operator+(Money other) const {
    if (other.kopecks_ > std::numeric_limits<std::int64_t>::max() - kopecks_)
        throw std::overflow_error("Переповнення суми");
    return Money(kopecks_ + other.kopecks_);
}

Money Money::operator-(Money other) const {
    if (kopecks_ < other.kopecks_) {
        throw std::invalid_argument("Результат не може бути від'ємним");
    }
    return Money(kopecks_ - other.kopecks_);
}

std::int64_t roundRatio(std::int64_t amount, std::int64_t numerator,
                        std::int64_t denominator) {
    if (amount < 0 || numerator < 0 || denominator <= 0)
        throw std::invalid_argument("Недопустимі аргументи для округлення");
    // Безпечний для обмежених прикладних значень алгоритм з перевірками переповнення.
    auto q = amount / denominator;
    auto r = amount % denominator;
    auto max = std::numeric_limits<std::int64_t>::max();
    if (numerator && (q > max / numerator || r > (max - denominator / 2) / numerator))
        throw std::overflow_error("Переповнення обчислень");
    auto main = q * numerator;
    auto remainder = (r * numerator + denominator / 2) / denominator;
    if (remainder > max - main) {
        throw std::overflow_error("Переповнення обчислень");
    }
    return main + remainder;
}

void validatePeriod(const std::string& p) {
    if (p.size() != 7 || p[4] != '-') {
        throw std::invalid_argument("Період має вигляд РРРР-ММ");
    }
    for (std::size_t i = 0; i < p.size(); ++i)
        if (i != 4 && !std::isdigit(static_cast<unsigned char>(p[i])))
            throw std::invalid_argument("Некоректний період");
    int y = std::stoi(p.substr(0, 4));
    int m = std::stoi(p.substr(5, 2));
    if (y < 2000 || y > 2100 || m < 1 || m > 12)
        throw std::invalid_argument("Період поза допустимими межами");
}

std::string toString(PayKind kind) {
    return kind == PayKind::Hourly ? "HOURLY" : "SALARIED";
}

PayKind payKindFromString(const std::string& s) {
    if (s == "HOURLY") {
        return PayKind::Hourly;
    }
    if (s == "SALARIED") {
        return PayKind::Salaried;
    }
    throw std::invalid_argument("Невідомий вид оплати: " + s);
}

std::string toString(AdjustmentKind kind) {
    return kind == AdjustmentKind::Bonus ? "BONUS" : "DEDUCTION";
}

AdjustmentKind adjustmentKindFromString(const std::string& s) {
    if (s == "BONUS") {
        return AdjustmentKind::Bonus;
    }
    if (s == "DEDUCTION") {
        return AdjustmentKind::Deduction;
    }
    throw std::invalid_argument("Невідомий вид коригування: " + s);
}

void validateEmployee(const Employee& e) {
    boundedText(e.name, "ПІБ", 200);
    boundedText(e.position, "Посада", 200);
    if (e.rate.kopecks() == 0 || e.rate.kopecks() > kMaxInputMoney)
        throw std::invalid_argument("Ставка має бути додатною і в допустимих межах");
}

void validateWork(const WorkRecord& w) {
    validatePeriod(w.period);
    constexpr int maxMinutes = 31 * 24 * 60;
    if (w.employeeId <= 0 || w.normMinutes <= 0 || w.normMinutes > maxMinutes ||
        w.regularMinutes < 0 || w.regularMinutes > w.normMinutes ||
        w.overtimeMinutes < 0 || w.overtimeMinutes > maxMinutes ||
        w.regularMinutes + w.overtimeMinutes > maxMinutes)
        throw std::invalid_argument("Некоректні хвилини обліку робочого часу");
}

void validateAdjustment(const Adjustment& a) {
    validatePeriod(a.period);
    if (a.employeeId <= 0 || a.amount.kopecks() == 0 ||
        a.amount.kopecks() > kMaxInputMoney || a.note.size() > 300)
        throw std::invalid_argument("Некоректне коригування");
}

void validateTaxSettings(const TaxSettings& s) {
    if (s.incomeTaxBp < 0 || s.militaryTaxBp < 0 ||
        s.incomeTaxBp > 10000 || s.militaryTaxBp > 10000 ||
        s.incomeTaxBp + s.militaryTaxBp > 10000)
        throw std::invalid_argument("Некоректні ставки утримань");
}
} // namespace payroll
