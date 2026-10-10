#pragma once

#include <optional>
#include <string>
#include <vector>

#include "AccessDeniedError.h"
#include "Assignment.h"
#include "AuditService.h"
#include "Course.h"
#include "IAssignmentRepository.h"
#include "ICourseRepository.h"
#include "ISubmissionRepository.h"
#include "IUserRepository.h"
#include "LateDays.h"
#include "SubmissionStatus.h"
#include "User.h"
#include "UserRoles.h"

namespace lms::application {

// One line of the head of department's list: a submission that already has a mark.
struct HodSubmissionRow {
    int submissionId = 0;
    int studentId = 0;
    std::string studentName;
    std::string studentNumber;
    lms::domain::SubmissionStatus status = lms::domain::SubmissionStatus::Marked;
    int lateDays = 0;
    std::optional<double> rawMark;
    std::optional<double> finalMark;
    std::string feedback;
};

// One line of the audit history of a mark (FR15, FR19), with the name of the person.
struct HodAuditRow {
    lms::domain::TimePoint changedAt{};
    std::string changedBy;
    std::optional<double> oldValue;
    double newValue = 0.0;
    std::string reason;
};

// What a head of department sees (FR13, FR19). Only their own department (rule C7).
class HodViewService {
public:
    HodViewService(const lms::domain::ISubmissionRepository& submissions,
                   const lms::domain::IAssignmentRepository& assignments,
                   const lms::domain::ICourseRepository& courses,
                   const lms::domain::IUserRepository& users, const AuditService& audit);

    // All of these throw AccessDeniedError for anyone who is not an active head of
    // department, and for a course or assignment of another department.
    std::vector<lms::domain::Course> courses(const lms::domain::User& hod) const;
    std::vector<lms::domain::Assignment> assignments(const lms::domain::User& hod,
                                                     int courseId) const;

    // Submissions that have a mark (Marked, Moderated, Released), ordered by student number.
    // std::invalid_argument if the assignment does not exist.
    std::vector<HodSubmissionRow> rows(const lms::domain::User& hod, int assignmentId) const;

    // Oldest first. std::invalid_argument if the submission does not exist.
    std::vector<HodAuditRow> history(const lms::domain::User& hod, int submissionId) const;

private:
    static const lms::domain::HoD& requireHoD(const lms::domain::User& user);
    void requireOwnCourse(const lms::domain::User& user, int courseId) const;

    const lms::domain::ISubmissionRepository& submissions_;
    const lms::domain::IAssignmentRepository& assignments_;
    const lms::domain::ICourseRepository& courses_;
    const lms::domain::IUserRepository& users_;
    const AuditService& audit_;
};

}  // namespace lms::application
