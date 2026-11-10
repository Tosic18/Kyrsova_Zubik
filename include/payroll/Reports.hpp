#pragma once
#include "payroll/Domain.hpp"
#include <iosfwd>
#include <string>

namespace payroll {

// Представлення не залежить від зберігання чи формул нарахування.
class Reports {
public:
    static void statement(std::ostream& out, const PayrollRun& run);
    static void payslip(std::ostream& out, const PayrollRun& run, int employeeId);
    static void exportCsv(const PayrollRun& run, const std::string& filename);
};
}
