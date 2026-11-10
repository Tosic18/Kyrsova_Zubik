#include "payroll/PayrollService.hpp"
#include "payroll/PayPolicy.hpp"
#include <stdexcept>
#include <sstream>

namespace payroll {

int PayrollService::hire(Employee employee) {
    validateEmployee(employee);
    return repository_.addEmployee(employee);
}

void PayrollService::edit(Employee employee) {
    validateEmployee(employee);
    auto prior = repository_.employee(employee.id);
    if (!prior || !prior->active) {
        throw std::invalid_argument("Працівника не знайдено або його архівовано");
    }
    employee.active = true;
    repository_.updateEmployee(employee);
}

void PayrollService::archive(int id) {
    auto prior = repository_.employee(id);
    if (!prior || !prior->active) {
        throw std::invalid_argument("Працівника не знайдено або його вже архівовано");
    }
    repository_.archiveEmployee(id);
}

void PayrollService::registerWork(WorkRecord work) {
    validateWork(work);
    auto e = repository_.employee(work.employeeId);
    if (!e || !e->active) {
        throw std::invalid_argument("Потрібен активний працівник");
    }
    repository_.saveWork(work);
}

int PayrollService::registerAdjustment(Adjustment a) {
    validateAdjustment(a);
    auto e = repository_.employee(a.employeeId);
    if (!e || !e->active) {
        throw std::invalid_argument("Потрібен активний працівник");
    }
    return repository_.addAdjustment(a);
}

void PayrollService::removeAdjustment(int id) {
    repository_.deleteAdjustment(id);
}

void PayrollService::configureTaxes(TaxSettings settings) {
    validateTaxSettings(settings);
    repository_.setTaxSettings(settings);
}

PayrollRun PayrollService::calculate(const std::string& period) {
    validatePeriod(period);
    auto employees = repository_.employees();
    if (employees.empty()) {
        throw std::invalid_argument("Немає активних працівників");
    }
    PayrollRun run;
    run.period = period;
    run.settings = repository_.taxSettings();
    validateTaxSettings(run.settings);
    // Не створюємо часткову відомість: спочатку перевіряємо наявність усіх табелів.
    std::ostringstream missing;
    for (const auto& employee : employees) {
        if (!repository_.work(employee.id, period)) missing << ' ' << employee.id;
    }
    if (!missing.str().empty())
        throw std::invalid_argument("Відсутній облік часу для працівників ID:" + missing.str());

    for (const auto& employee : employees) {
        const WorkRecord w = *repository_.work(employee.id, period);
        Earnings earned = PayPolicyFactory::create(employee.kind)->calculate(employee.rate, w);
        Money bonuses, deductions;
        for (const auto& a : repository_.adjustments(employee.id, period)) {
            if (a.kind == AdjustmentKind::Bonus) {
                bonuses = bonuses + a.amount;
            } else {
                deductions = deductions + a.amount;
            }
        }
        Payslip p;
        p.employeeId = employee.id;
        p.employeeName = employee.name;
        p.position = employee.position;
        p.kind = employee.kind;
        p.rate = employee.rate;
        p.work = w;
        p.regular = earned.regular;
        p.overtime = earned.overtime;
        p.bonus = bonuses;
        p.gross = p.regular + p.overtime + p.bonus;
        p.incomeTax = Money(roundRatio(p.gross.kopecks(), run.settings.incomeTaxBp, 10000));
        p.militaryTax = Money(roundRatio(p.gross.kopecks(), run.settings.militaryTaxBp, 10000));
        p.extraDeduction = deductions;
        const Money taxes = p.incomeTax + p.militaryTax;
        if (taxes.kopecks() > p.gross.kopecks() ||
            deductions.kopecks() > p.gross.kopecks() - taxes.kopecks()) {
            throw std::invalid_argument("Утримання перевищують нарахування, ID: " + std::to_string(employee.id));
        }
        p.net = p.gross - taxes - deductions;
        run.slips.push_back(p);
    }
    run.id = repository_.saveRun(run);
    // Зчитування гарантує фактичну дату SQLite і незмінний знімок відомості.
    return *repository_.run(run.id);
}
} // namespace payroll
