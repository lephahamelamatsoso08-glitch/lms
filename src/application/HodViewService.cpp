#include "HodViewService.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>

#include "Action.h"
#include "StudentInfo.h"
#include "Submission.h"

namespace lms::application {

using lms::domain::Action;
using lms::domain::Assignment;
using lms::domain::Course;
using lms::domain::HoD;
using lms::domain::StudentInfo;
using lms::domain::Submission;
using lms::domain::SubmissionStatus;
using lms::domain::User;

HodViewService::HodViewService(const lms::domain::ISubmissionRepository& submissions,
                               const lms::domain::IAssignmentRepository& assignments,
                               const lms::domain::ICourseRepository& courses,
                               const lms::domain::IUserRepository& users, const AuditService& audit)
    : submissions_(submissions),
      assignments_(assignments),
      courses_(courses),
      users_(users),
      audit_(audit) {}

const HoD& HodViewService::requireHoD(const User& user) {
    const HoD* hod = dynamic_cast<const HoD*>(&user);
    if (hod == nullptr || !user.canPerform(Action::ModerateMarks)) {
        throw AccessDeniedError("Only the head of department can use this page.");
    }
    return *hod;
}

void HodViewService::requireOwnCourse(const User& user, int courseId) const {
    const HoD& hod = requireHoD(user);
    const std::optional<Course> course = courses_.findById(courseId);
    if (!course || course->departmentId() != hod.departmentId()) {
        throw AccessDeniedError("This course is not in your department.");
    }
}

std::vector<Course> HodViewService::courses(const User& user) const {
    return courses_.findByDepartment(requireHoD(user).departmentId());
}

std::vector<Assignment> HodViewService::assignments(const User& user, int courseId) const {
    requireOwnCourse(user, courseId);
    return assignments_.findByCourse(courseId);
}

std::vector<HodSubmissionRow> HodViewService::rows(const User& user, int assignmentId) const {
    requireHoD(user);
    const std::optional<Assignment> assignment = assignments_.findById(assignmentId);
    if (!assignment) {
        throw std::invalid_argument("Assignment not found.");
    }
    requireOwnCourse(user, assignment->courseId());

    std::unordered_map<int, StudentInfo> students;
    for (const StudentInfo& student : courses_.findStudents(assignment->courseId())) {
        students[student.id] = student;
    }

    std::vector<HodSubmissionRow> result;
    for (const Submission& submission : submissions_.findByAssignment(assignmentId)) {
        const SubmissionStatus status = submission.status();
        if (status != SubmissionStatus::Marked && status != SubmissionStatus::Moderated &&
            status != SubmissionStatus::Released) {
            continue;
        }
        HodSubmissionRow row;
        row.submissionId = submission.id();
        row.studentId = submission.studentId();
        const auto found = students.find(submission.studentId());
        if (found != students.end()) {
            row.studentName = found->second.name;
            row.studentNumber = found->second.number;
        }
        row.status = status;
        row.lateDays = submission.lateDays();
        row.rawMark = submission.rawMark();
        row.finalMark = submission.finalMark();
        row.feedback = submission.feedback();
        result.push_back(row);
    }
    std::sort(result.begin(), result.end(),
              [](const HodSubmissionRow& a, const HodSubmissionRow& b) {
                  return a.studentNumber != b.studentNumber ? a.studentNumber < b.studentNumber
                                                             : a.submissionId < b.submissionId;
              });
    return result;
}

std::vector<HodAuditRow> HodViewService::history(const User& user, int submissionId) const {
    requireHoD(user);
    const std::optional<Submission> submission = submissions_.findById(submissionId);
    if (!submission) {
        throw std::invalid_argument("Submission not found.");
    }
    const std::optional<Assignment> assignment = assignments_.findById(submission->assignmentId());
    if (!assignment) {
        throw std::invalid_argument("Assignment not found.");
    }
    requireOwnCourse(user, assignment->courseId());

    std::vector<HodAuditRow> result;
    for (const auto& entry : audit_.history(user, submissionId)) {
        HodAuditRow row;
        row.changedAt = entry.changedAt();
        const std::unique_ptr<User> who = users_.findById(entry.changedBy());
        row.changedBy = who ? who->fullName() : "Unknown user";
        row.oldValue = entry.oldValue();
        row.newValue = entry.newValue();
        row.reason = entry.reason();
        result.push_back(row);
    }
    return result;
}

}  // namespace lms::application
