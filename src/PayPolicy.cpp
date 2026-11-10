#include "payroll/PayPolicy.hpp"
#include <stdexcept>

namespace payroll {

Earnings HourlyPayPolicy::calculate(Money rate, const WorkRecord& w) const {
    validateWork(w);
    return {
        Money(roundRatio(rate.kopecks(), w.regularMinutes, 60)),
        Money(roundRatio(rate.kopecks(), 3LL * w.overtimeMinutes, 120))
    };
}

Earnings SalariedPayPolicy::calculate(Money rate, const WorkRecord& w) const {
    validateWork(w);
    return {
        Money(roundRatio(rate.kopecks(), w.regularMinutes, w.normMinutes)),
        Money(roundRatio(rate.kopecks(), 3LL * w.overtimeMinutes, 2LL * w.normMinutes))
    };
}

std::unique_ptr<PayPolicy> PayPolicyFactory::create(PayKind kind) {
    switch (kind) {
        case PayKind::Hourly:
            return std::make_unique<HourlyPayPolicy>();
        case PayKind::Salaried:
            return std::make_unique<SalariedPayPolicy>();
    }
    throw std::invalid_argument("Невідомий тип оплати");
}
} // namespace payroll
