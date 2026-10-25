#include "LateDays.h"

namespace lms::domain {

int calculateLateDays(TimePoint deadline, TimePoint submittedAt) {
    if (submittedAt <= deadline) {
        return 0;
    }
    const auto late = std::chrono::ceil<std::chrono::seconds>(submittedAt - deadline);
    const long long secondsPerDay = 24LL * 60 * 60;
    return static_cast<int>((late.count() + secondsPerDay - 1) / secondsPerDay);
}

}  // namespace lms::domain
