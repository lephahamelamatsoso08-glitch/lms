#pragma once

#include "IPenaltyStrategy.h"

namespace lms::domain {

// Rule C3: each late day removes 10% of the MAXIMUM mark, for up to
// three days. After that the final mark is 0.
class TenPercentPerDayPenalty : public IPenaltyStrategy {
public:
    explicit TenPercentPerDayPenalty(double ratePerDay = 0.10, int maxLateDays = 3);

    double finalMark(double rawMark, double maxMark, int lateDays) const override;

private:
    double ratePerDay_;
    int maxLateDays_;
};

}  // namespace lms::domain
