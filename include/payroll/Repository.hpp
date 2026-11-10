#pragma once
#include "payroll/Domain.hpp"
#include <optional>
#include <string>
#include <vector>

struct sqlite3; // Попереднє оголошення: інтерфейс не залежить від SQLite-заголовка.

namespace payroll {

// DIP: сервіс залежить від контракту, а не від SQLite API.
class IRepository {
public:
    virtual ~IRepository() = default;
    virtual int addEmployee(const Employee&) = 0;
    virtual void updateEmployee(const Employee&) = 0;
    virtual void archiveEmployee(int employeeId) = 0;
    virtual std::optional<Employee> employee(int id) const = 0;
    virtual std::vector<Employee> employees(bool includeArchived = false) const = 0;
    virtual std::vector<Employee> searchEmployees(const std::string& keyword) const = 0;
    virtual void saveWork(const WorkRecord&) = 0;
    virtual std::optional<WorkRecord> work(int employeeId, const std::string& period) const = 0;
    virtual int addAdjustment(const Adjustment&) = 0;
    virtual void deleteAdjustment(int id) = 0;
    virtual std::vector<Adjustment> adjustments(int employeeId, const std::string& period) const = 0;
    virtual TaxSettings taxSettings() const = 0;
    virtual void setTaxSettings(const TaxSettings&) = 0;
    virtual int saveRun(const PayrollRun&) = 0;
    virtual std::optional<PayrollRun> run(int id) const = 0;
    virtual std::optional<PayrollRun> latestRun(const std::string& period) const = 0;
    virtual std::vector<RunSummary> history() const = 0;
};

class SQLiteRepository final : public IRepository {
public:
    explicit SQLiteRepository(const std::string& filename);
    ~SQLiteRepository() override;
    SQLiteRepository(const SQLiteRepository&) = delete;
    SQLiteRepository& operator=(const SQLiteRepository&) = delete;

    int addEmployee(const Employee&) override;
    void updateEmployee(const Employee&) override;
    void archiveEmployee(int) override;
    std::optional<Employee> employee(int) const override;
    std::vector<Employee> employees(bool includeArchived = false) const override;
    std::vector<Employee> searchEmployees(const std::string&) const override;
    void saveWork(const WorkRecord&) override;
    std::optional<WorkRecord> work(int, const std::string&) const override;
    int addAdjustment(const Adjustment&) override;
    void deleteAdjustment(int) override;
    std::vector<Adjustment> adjustments(int, const std::string&) const override;
    TaxSettings taxSettings() const override;
    void setTaxSettings(const TaxSettings&) override;
    int saveRun(const PayrollRun&) override;
    std::optional<PayrollRun> run(int) const override;
    std::optional<PayrollRun> latestRun(const std::string&) const override;
    std::vector<RunSummary> history() const override;
private:
    ::sqlite3* db_ = nullptr;
    void execute(const char* sql) const;
};
} // namespace payroll
