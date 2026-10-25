#pragma once

#include <optional>
#include <stdexcept>
#include <string>

#include "LateDays.h"
#include "SubmissionStatus.h"

namespace lms::domain {

// One student's submission for one assignment (FR9, FR12, FR16).
// The status can only change along the state machine in the design.
class Submission {
public:
    Submission(int id, int assignmentId, int studentId, SubmissionStatus status, int lateDays);

    // Full constructor, used to restore a saved submission.
    Submission(int id, int assignmentId, int studentId, SubmissionStatus status, int lateDays,
               std::optional<double> rawMark, std::optional<double> finalMark,
               std::string feedback, std::optional<TimePoint> releasedAt);

    int id() const { return id_; }  // 0 means "not saved yet"
    int assignmentId() const { return assignmentId_; }
    int studentId() const { return studentId_; }
    SubmissionStatus status() const { return status_; }
    int lateDays() const { return lateDays_; }
    const std::optional<double>& rawMark() const { return rawMark_; }
    const std::optional<double>& finalMark() const { return finalMark_; }
    const std::string& feedback() const { return feedback_; }
    const std::optional<TimePoint>& releasedAt() const { return releasedAt_; }

    // True if the state machine allows the move from -> to.
    static bool isAllowed(SubmissionStatus from, SubmissionStatus to);

    // Throws std::logic_error if the move is not allowed. The status is
    // left unchanged in that case.
    void transitionTo(SubmissionStatus next);

    // FR12: the lecturer marks the work (also a re-mark). Status becomes Marked.
    // The final mark is the raw mark after the late penalty, so it cannot be higher.
    // Throws std::invalid_argument for bad values, std::logic_error if the
    // submission is already Moderated or Released. Nothing changes on error.
    void applyMark(double newRawMark, double newFinalMark, const std::string& newFeedback);

    // FR13: the HoD changes the final mark. Only while the status is Marked.
    void changeFinalMark(double newFinalMark);

    // FR14: Moderated -> Released, and records the time.
    void release(TimePoint when);

private:
    int id_;
    int assignmentId_;
    int studentId_;
    SubmissionStatus status_;
    int lateDays_;
    std::optional<double> rawMark_;
    std::optional<double> finalMark_;
    std::string feedback_;
    std::optional<TimePoint> releasedAt_;
};

}  // namespace lms::domain
