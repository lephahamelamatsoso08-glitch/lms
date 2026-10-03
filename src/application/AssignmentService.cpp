#include "AssignmentService.h"

#include <chrono>
#include <optional>
#include <stdexcept>
#include <utility>

#include "SubmissionStatus.h"

namespace lms::application {

using lms::domain::Action;
using lms::domain::Assignment;
using lms::domain::Course;
using lms::domain::Extension;
using lms::domain::Submission;
using lms::domain::SubmissionStatus;
using lms::domain::TimePoint;
using lms::domain::User;

AssignmentService::AssignmentService(lms::domain::IAssignmentRepository& assignments,
                                     const lms::domain::ICourseRepository& courses,
                                     const lms::domain::ISubmissionRepository& submissions,
                                     Clock clock)
    : assignments_(assignments),
      courses_(courses),
      submissions_(submissions),
      clock_(std::move(clock)) {
    if (!clock_) {
        clock_ = &AssignmentService::systemNow;
    }
}

TimePoint AssignmentService::systemNow() { return std::chrono::system_clock::now(); }

void AssignmentService::requireTeaches(const User& lecturer, Action action, int courseId) const {
    if (!lecturer.canPerform(action)) {
        throw AccessDeniedError("Only an active lecturer can do this.");
    }
    if (!courses_.isLecturerAssigned(lecturer.id(), courseId)) {
        throw AccessDeniedError("You do not teach this course.");
    }
}

std::vector<Course> AssignmentService::myCourses(const User& lecturer) const {
    if (!lecturer.canPerform(Action::CreateAssignment)) {
        throw AccessDeniedError("Only an active lecturer can do this.");
    }
    return courses_.findForLecturer(lecturer.id());
}

std::vector<Assignment> AssignmentService::listAssignments(const User& lecturer,
                                                           int courseId) const {
    requireTeaches(lecturer, Action::CreateAssignment, courseId);
    return assignments_.findByCourse(courseId);
}

std::vector<Submission> AssignmentService::listSubmissions(const User& lecturer,
                                                           int assignmentId) const {
    const std::optional<Assignment> assignment = assignments_.findById(assignmentId);
    if (!assignment) {
        throw std::invalid_argument("Assignment not found.");
    }
    requireTeaches(lecturer, Action::MarkSubmission, assignment->courseId());
    return submissions_.findByAssignment(assignmentId);
}

int AssignmentService::createAssignment(const User& lecturer, int courseId,
                                        const std::string& title, TimePoint deadline,
                                        double maxMark) {
    requireTeaches(lecturer, Action::CreateAssignment, courseId);
    if (deadline <= clock_()) {
        throw std::invalid_argument("The deadline must be in the future.");
    }
    return assignments_.add(Assignment(0, courseId, title, deadline, maxMark));
}

int AssignmentService::grantExtension(const User& lecturer, int assignmentId, int studentId,
                                      TimePoint newDeadline, const std::string& reason) {
    const std::optional<Assignment> assignment = assignments_.findById(assignmentId);
    if (!assignment) {
        throw std::invalid_argument("Assignment not found.");
    }
    requireTeaches(lecturer, Action::GrantExtension, assignment->courseId());
    if (!courses_.isStudentEnrolled(studentId, assignment->courseId())) {
        throw std::invalid_argument("The student is not enrolled in this course.");
    }

    const std::optional<Submission> existing =
        submissions_.findByStudentAndAssignment(studentId, assignmentId);
    if (existing && existing->status() != SubmissionStatus::Submitted) {
        throw std::logic_error(
            "An extension cannot be granted after the work was submitted late or marked.");
    }

    // The assignment checks that the new deadline is later and that there is a reason.
    return assignments_.saveExtension(assignment->grantExtension(studentId, newDeadline, reason));
}

}  // namespace lms::application
