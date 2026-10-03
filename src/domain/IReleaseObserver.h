#pragma once

#include <vector>

namespace lms::domain {

// Design pattern: Observer. The moderation service tells every observer when
// marks are released, and does not need to know what the observers do (FR18).
class IReleaseObserver {
public:
    virtual ~IReleaseObserver() = default;
    virtual void onMarksReleased(int assignmentId, const std::vector<int>& studentIds) = 0;
};

}  // namespace lms::domain
