#include "StudentViewService.h"

#include <optional>
#include <stdexcept>

#include "Action.h"
#include "Assignment.h"
#include "SubmissionStatus.h"

namespace lms::application {

using lms::domain::Action;
using lms::domain::Assignment;
using lms::domain::Course;
using lms::domain::Submission;
using lms::domain::SubmissionStatus;
using lms::domain::User;

namespace {

// The only status words a student ever sees (rule C1).
std::string statusText(SubmissionStatus status) {
    switch (status) {
        case SubmissionStatus::Submitted: return "Submitted";
        case SubmissionStatus::Late:      return "Submitted late";
        case SubmissionStatus::Marked:    return "Marked, awaiting moderation";
        case SubmissionStatus::Moderated: return "Moderated, awaiting release";
        case SubmissionStatus::Released:  return "Released";
    }
    return "Unknown";
}

}  // namespace

StudentViewService::StudentViewService(const lms::domain::ISubmissionRepository& submissions,
                                       const lms::domain::IAssignmentRepository& assignments,
                                       const lms::domain::ICourseRepository& courses)
    : submissions_(submissions), assignments_(assignments), courses_(courses) {}

void StudentViewService::requireStudent(const User& student) {
    if (!student.canPerform(Action::ViewOwnCourses)) {
        throw AccessDeniedError("Only active students can use this page.");
    }
}

void StudentViewService::requireEnrolled(const User& student, int courseId) const {
    if (!courses_.isStudentEnrolled(student.id(), courseId)) {
        throw AccessDeniedError("You are not enrolled in this course.");
    }
}

std::vector<Course> StudentViewService::myCourses(const User& student) const {
    requireStudent(student);
    return courses_.findForStudent(student.id());
}

std::vector<StudentAssignmentItem> StudentViewService::myAssignments(const User& student,
                                                                     int courseId) const {
    requireStudent(student);
    requireEnrolled(student, courseId);

    std::vector<StudentAssignmentItem> items;
    for (const Assignment& assignment : assignments_.findByCourse(courseId)) {
        StudentAssignmentItem item;
        item.assignmentId = assignment.id();
        item.title = assignment.title();
        item.maxMark = assignment.maxMark();
        item.deadline = assignments_.getDeadlineFor(student.id(), assignment.id());

        const std::optional<Submission> submission =
            submissions_.findByStudentAndAssignment(student.id(), assignment.id());
        item.statusLabel = submission ? statusText(submission->status()) : "Not submitted";
        items.push_back(item);
    }
    return items;
}

StudentSubmissionView StudentViewService::mySubmission(const User& student,
                                                       int assignmentId) const {
    requireStudent(student);
    const std::optional<Assignment> assignment = assignments_.findById(assignmentId);
    if (!assignment) {
        throw std::invalid_argument("Assignment not found.");
    }
    requireEnrolled(student, assignment->courseId());

    StudentSubmissionView view;
    view.assignmentId = assignment->id();
    view.assignmentTitle = assignment->title();
    view.maxMark = assignment->maxMark();
    view.deadline = assignments_.getDeadlineFor(student.id(), assignmentId);

    const std::optional<Submission> submission =
        submissions_.findByStudentAndAssignment(student.id(), assignmentId);
    if (!submission) {
        view.statusLabel = "Not submitted";
        return view;
    }

    view.submitted = true;
    view.statusLabel = statusText(submission->status());
    view.lateDays = submission->lateDays();
    view.versionCount = static_cast<int>(submissions_.findVersions(submission->id()).size());

    // Rule C1: marks and feedback are copied only after the HoD has released them.
    if (submission->status() == SubmissionStatus::Released) {
        view.rawMark = submission->rawMark();
        view.finalMark = submission->finalMark();
        view.feedback = submission->feedback();
        view.releasedAt = submission->releasedAt();
    }
    return view;
}

}  // namespace lms::application
