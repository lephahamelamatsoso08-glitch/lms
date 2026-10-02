#include "TenPercentPerDayPenalty.h"

#include <algorithm>
#include <stdexcept>

namespace lms::domain {

TenPercentPerDayPenalty::TenPercentPerDayPenalty(double ratePerDay, int maxLateDays)
    : ratePerDay_(ratePerDay), maxLateDays_(maxLateDays) {
    if (ratePerDay_ <= 0.0 || ratePerDay_ > 1.0) {
        throw std::invalid_argument("ratePerDay must be greater than 0 and at most 1");
    }
    if (maxLateDays_ < 0) {
        throw std::invalid_argument("maxLateDays must not be negative");
    }
}

double TenPercentPerDayPenalty::finalMark(double rawMark, double maxMark, int lateDays) const {
    if (maxMark <= 0.0) {
        throw std::invalid_argument("maxMark must be greater than 0");
    }
    if (rawMark < 0.0 || rawMark > maxMark) {
        throw std::invalid_argument("rawMark must be between 0 and maxMark");
    }
    if (lateDays < 0) {
        throw std::invalid_argument("lateDays must not be negative");
    }

    if (lateDays == 0) {
        return rawMark;
    }
    if (lateDays > maxLateDays_) {
        return 0.0;
    }
    const double deduction = ratePerDay_ * lateDays * maxMark;
    return std::max(0.0, rawMark - deduction);
}

}  // namespace lms::domain
