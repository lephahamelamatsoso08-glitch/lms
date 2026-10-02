#pragma once

#include <string>

#include "LateDays.h"

namespace lms::domain {

// A personal deadline for one student on one assignment (FR7).
class Extension {
public:
    Extension(int id, int assignmentId, int studentId, TimePoint newDeadline, std::string reason);

    int id() const { return id_; }  // 0 means "not saved yet"
    int assignmentId() const { return assignmentId_; }
    int studentId() const { return studentId_; }
    TimePoint newDeadline() const { return newDeadline_; }
    const std::string& reason() const { return reason_; }

private:
    int id_;
    int assignmentId_;
    int studentId_;
    TimePoint newDeadline_;
    std::string reason_;
};

}  // namespace lms::domain
