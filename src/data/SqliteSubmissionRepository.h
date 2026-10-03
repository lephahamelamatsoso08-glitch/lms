#pragma once

#include <optional>
#include <vector>

#include "DatabaseConnection.h"
#include "ISubmissionRepository.h"

namespace lms::data {

class SqliteSubmissionRepository : public lms::domain::ISubmissionRepository {
public:
    explicit SqliteSubmissionRepository(DatabaseConnection& db);

    int add(const lms::domain::Submission& submission,
            const lms::domain::SubmissionVersion& firstVersion) override;
    std::optional<lms::domain::Submission> findById(int id) const override;
    std::optional<lms::domain::Submission> findByStudentAndAssignment(
        int studentId, int assignmentId) const override;
    std::vector<lms::domain::Submission> findByAssignment(int assignmentId) const override;
    void save(const lms::domain::Submission& submission) override;
    int addVersion(int submissionId, const lms::domain::SubmissionVersion& version) override;
    std::vector<lms::domain::SubmissionVersion> findVersions(int submissionId) const override;

private:
    static lms::domain::Submission readSubmission(Statement& row);
    static void checkNewVersion(const lms::domain::SubmissionVersion& version);
    int insertVersion(int submissionId, const lms::domain::SubmissionVersion& version);

    DatabaseConnection& db_;
};

}  // namespace lms::data
