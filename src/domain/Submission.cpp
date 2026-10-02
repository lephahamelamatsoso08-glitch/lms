#include "Submission.h"

namespace lms::domain {

Submission::Submission(int id, int assignmentId, int studentId, SubmissionStatus status,
                       int lateDays)
    : id_(id),
      assignmentId_(assignmentId),
      studentId_(studentId),
      status_(status),
      lateDays_(lateDays) {
    if (id_ < 0) {
        throw std::invalid_argument("id must not be negative");
    }
    if (assignmentId_ <= 0 || studentId_ <= 0) {
        throw std::invalid_argument("a submission needs an assignment and a student");
    }
    if (lateDays_ < 0) {
        throw std::invalid_argument("lateDays must not be negative");
    }
    if (status_ == SubmissionStatus::Submitted && lateDays_ != 0) {
        throw std::invalid_argument("an on-time submission has no late days");
    }
    if (status_ == SubmissionStatus::Late && lateDays_ < 1) {
        throw std::invalid_argument("a late submission has at least one late day");
    }
}

bool Submission::isAllowed(SubmissionStatus from, SubmissionStatus to) {
    switch (from) {
        case SubmissionStatus::Submitted:
            return to == SubmissionStatus::Submitted || to == SubmissionStatus::Marked;
        case SubmissionStatus::Late:
            return to == SubmissionStatus::Marked;
        case SubmissionStatus::Marked:
            return to == SubmissionStatus::Marked || to == SubmissionStatus::Moderated;
        case SubmissionStatus::Moderated:
            return to == SubmissionStatus::Released;
        case SubmissionStatus::Released:
            return to == SubmissionStatus::Released;
    }
    return false;
}

void Submission::transitionTo(SubmissionStatus next) {
    if (!isAllowed(status_, next)) {
        throw std::logic_error("cannot move a submission from " + toString(status_) + " to " +
                               toString(next));
    }
    status_ = next;
}

}  // namespace lms::domain
