#pragma once
#include "payroll/Repository.hpp"

namespace payroll {

// Фасад бізнес-логіки: валідація, вибір поліморфної політики, обчислення.
class PayrollService {
public:
    explicit PayrollService(IRepository& repository) : repository_(repository) {}
    int hire(Employee employee);
    void edit(Employee employee);
    void archive(int id);
    void registerWork(WorkRecord work);
    int registerAdjustment(Adjustment adjustment);
    void removeAdjustment(int id);
    void configureTaxes(TaxSettings settings);
    PayrollRun calculate(const std::string& period);  // повертає збережений знімок
    IRepository& data() noexcept {
        return repository_;
    }
    const IRepository& data() const noexcept {
        return repository_;
    }
private:
    IRepository& repository_;
};
} // namespace payroll
