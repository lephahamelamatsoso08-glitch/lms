#pragma once

namespace lms::domain {

// Things a user may try to do. Each value maps to a requirement in the SRS.
enum class Action {
    ManageUsers,        // FR3
    ManageCourses,      // FR4
    AssignAndEnrol,     // FR5
    ResetPassword,      // FR20
    CreateAssignment,   // FR6
    GrantExtension,     // FR7
    MarkSubmission,     // FR12
    ModerateMarks,      // FR13
    ReleaseMarks,       // FR14
    ViewAuditLog,       // FR19
    ViewOwnCourses,     // FR8
    SubmitWork,         // FR9, FR10
    ViewReleasedMarks   // FR17
};

}  // namespace lms::domain
