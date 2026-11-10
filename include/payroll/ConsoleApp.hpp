#pragma once
#include "payroll/PayrollService.hpp"
#include <string>

namespace payroll {

// Межа інтерфейсу користувача: ні SQL, ні формул розрахунку тут немає.
class ConsoleApp {
public:
    explicit ConsoleApp(PayrollService& service) : service_(service) {}
    void run();
private:
    PayrollService& service_;
    static std::string read(const std::string& prompt);
    static int readInt(const std::string& prompt, int lower, int upper);
    static Money readMoney(const std::string& prompt);
    static int readMinutes(const std::string& prompt);
    static PayKind readPayKind();
    static std::string readPeriod();
    static void printEmployees(const std::vector<Employee>& list);
    void employeeMenu();
    void workMenu();
    void adjustmentsMenu();
    void taxMenu();
    void generate();
    void statement();
    void payslip();
    void history();
    void exportCsv();
};
}
