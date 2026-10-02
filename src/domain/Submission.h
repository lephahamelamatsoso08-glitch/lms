#pragma once

#include <stdexcept>

#include "SubmissionStatus.h"

namespace lms::domain {

// One student's submission for one assignment (FR9, FR16).
// The status can only change along the state machine in the design.
class Submission {
public:
    Submission(int id, int assignmentId, int studentId, SubmissionStatus status, int lateDays);

    int id() const { return id_; }  // 0 means "not saved yet"
    int assignmentId() const { return assignmentId_; }
    int studentId() const { return studentId_; }
    SubmissionStatus status() const { return status_; }
    int lateDays() const { return lateDays_; }

    // True if the state machine allows the move from -> to.
    static bool isAllowed(SubmissionStatus from, SubmissionStatus to);

    // Throws std::logic_error if the move is not allowed. The status is
    // left unchanged in that case.
    void transitionTo(SubmissionStatus next);

private:
    int id_;
    int assignmentId_;
    int studentId_;
    SubmissionStatus status_;
    int lateDays_;
};

}  // namespace lms::domain
