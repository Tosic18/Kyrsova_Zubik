#pragma once
#include "payroll/Domain.hpp"
#include <memory>

namespace payroll {

// Абстракція: розрахунок залежить від типу оплати, PayrollService не знає формули.
class PayPolicy {
public:
    virtual ~PayPolicy() = default;
    virtual Earnings calculate(Money rate, const WorkRecord& work) const = 0;
    virtual PayKind kind() const noexcept = 0;
};

class HourlyPayPolicy final : public PayPolicy {
public:
    Earnings calculate(Money rate, const WorkRecord& work) const override;
    PayKind kind() const noexcept override {
        return PayKind::Hourly;
    }
};

class SalariedPayPolicy final : public PayPolicy {
public:
    Earnings calculate(Money rate, const WorkRecord& work) const override;
    PayKind kind() const noexcept override {
        return PayKind::Salaried;
    }
};

// Проста фабрика приховує створення конкретної стратегії.
class PayPolicyFactory {
public:
    static std::unique_ptr<PayPolicy> create(PayKind kind);
};
} // namespace payroll
