#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace payroll {

// Гроші зберігаються як ціла кількість копійок: округлення лише на кроці розрахунку.
class Money {
public:
    explicit Money(std::int64_t kopecks = 0);
    std::int64_t kopecks() const noexcept {
        return kopecks_;
    }
    static Money parse(const std::string& text);
    std::string str() const;
    Money operator+(Money other) const;
    Money operator-(Money other) const;
    bool operator==(Money other) const noexcept {
        return kopecks_ == other.kopecks_;
    }
private:
    std::int64_t kopecks_;
};

// Округлення до копійки: половини — у більший бік, усі аргументи невід'ємні.
std::int64_t roundRatio(std::int64_t amount, std::int64_t numerator,
                        std::int64_t denominator);
void validatePeriod(const std::string& period);

// 10000 базисних пунктів = 100%.
struct TaxSettings {
    int incomeTaxBp = 1800;
    int militaryTaxBp = 500;
};

enum class PayKind { Hourly, Salaried };
enum class AdjustmentKind { Bonus, Deduction };
std::string toString(PayKind type);
PayKind payKindFromString(const std::string& text);
std::string toString(AdjustmentKind type);
AdjustmentKind adjustmentKindFromString(const std::string& text);

struct Employee {
    int id = 0;
    std::string name;
    std::string position;
    PayKind kind = PayKind::Hourly;
    Money rate;       // Для Hourly — грн/год; для Salaried — грн/місяць.
    bool active = true;
};

struct WorkRecord {
    int employeeId = 0;
    std::string period; // YYYY-MM
    int regularMinutes = 0;
    int overtimeMinutes = 0;
    int normMinutes = 9600; // 160 годин за умовчанням, змінюється для кожного запису.
};

struct Adjustment {
    int id = 0;
    int employeeId = 0;
    std::string period;
    AdjustmentKind kind = AdjustmentKind::Bonus;
    Money amount;
    std::string note;
};

struct Earnings {
    Money regular;
    Money overtime;
};

struct Payslip {
    int employeeId = 0;
    std::string employeeName;
    std::string position;
    PayKind kind = PayKind::Hourly;
    Money rate;
    WorkRecord work;
    Money regular;
    Money overtime;
    Money bonus;
    Money gross;
    Money incomeTax;
    Money militaryTax;
    Money extraDeduction;
    Money net;
};

struct PayrollRun {
    int id = 0;
    std::string period;
    std::string createdAt;
    TaxSettings settings;
    std::vector<Payslip> slips;
};

struct RunSummary {
    int id = 0;
    std::string period;
    std::string createdAt;
    int employeeCount = 0;
};

void validateEmployee(const Employee& e);
void validateWork(const WorkRecord& w);
void validateAdjustment(const Adjustment& adjustment);
void validateTaxSettings(const TaxSettings& settings);

} // namespace payroll
