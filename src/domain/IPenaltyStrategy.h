#pragma once

namespace lms::domain {

// Strategy for the late-work rule (design pattern: Strategy).
class IPenaltyStrategy {
public:
    virtual ~IPenaltyStrategy() = default;

    // Returns the final mark after the late penalty. Never below 0.
    virtual double finalMark(double rawMark, double maxMark, int lateDays) const = 0;
};

}  // namespace lms::domain
