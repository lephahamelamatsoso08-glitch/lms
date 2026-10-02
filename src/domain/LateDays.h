#pragma once

#include <chrono>

namespace lms::domain {

using TimePoint = std::chrono::system_clock::time_point;

// Rule C3: each full or part 24-hour period after the deadline counts as
// one late day. On time (or exactly at the deadline) is 0 days.
int calculateLateDays(TimePoint deadline, TimePoint submittedAt);

}  // namespace lms::domain
