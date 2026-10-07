#include "LecturerViewService.h"

#include <stdexcept>

#include "Action.h"
#include "Assignment.h"
#include "StudentInfo.h"
#include "Submission.h"

namespace lms::application {

using lms::domain::Action;
using lms::domain::Assignment;
using lms::domain::StudentInfo;
using lms::domain::Submission;
using lms::domain::User;

LecturerViewService::LecturerViewService(const lms::domain::ISubmissionRepository& submissions,
                                         const lms::domain::IAssignmentRepository& assignments,
                                         const lms::domain::ICourseRepository& courses)
    : submissions_(submissions), assignments_(assignments), courses_(courses) {}

std::vector<LecturerSubmissionRow> LecturerViewService::rows(const User& lecturer,
                                                             int assignmentId) const {
    if (!lecturer.canPerform(Action::MarkSubmission)) {
        throw AccessDeniedError("Only an active lecturer can do this.");
    }
    const std::optional<Assignment> assignment = assignments_.findById(assignmentId);
    if (!assignment) {
        throw std::invalid_argument("Assignment not found.");
    }
    if (!courses_.isLecturerAssigned(lecturer.id(), assignment->courseId())) {
        throw AccessDeniedError("You do not teach this course.");
    }

    std::vector<LecturerSubmissionRow> result;
    for (const StudentInfo& student : courses_.findStudents(assignment->courseId())) {
        LecturerSubmissionRow row;
        row.studentId = student.id;
        row.studentName = student.name;
        row.studentNumber = student.number;

        const std::optional<Submission> submission =
            submissions_.findByStudentAndAssignment(student.id, assignmentId);
        if (submission) {
            row.submitted = true;
            row.submissionId = submission->id();
            row.status = submission->status();
            row.lateDays = submission->lateDays();
            const auto versions = submissions_.findVersions(submission->id());
            row.versionCount = static_cast<int>(versions.size());
            if (!versions.empty()) {
                row.latestFile = versions.back().filePath;
            }
            row.rawMark = submission->rawMark();
            row.finalMark = submission->finalMark();
            row.feedback = submission->feedback();
        }
        result.push_back(row);
    }
    return result;
}

}  // namespace lms::application
