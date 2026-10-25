#pragma once

#include <optional>
#include <vector>

#include "DatabaseConnection.h"
#include "IAssignmentRepository.h"

namespace lms::data {

class SqliteAssignmentRepository : public lms::domain::IAssignmentRepository {
public:
    explicit SqliteAssignmentRepository(DatabaseConnection& db);

    int add(const lms::domain::Assignment& assignment) override;
    std::optional<lms::domain::Assignment> findById(int id) const override;
    std::vector<lms::domain::Assignment> findByCourse(int courseId) const override;
    int saveExtension(const lms::domain::Extension& extension) override;
    std::optional<lms::domain::Extension> findExtension(int assignmentId,
                                                        int studentId) const override;
    lms::domain::TimePoint getDeadlineFor(int studentId, int assignmentId) const override;

private:
    static lms::domain::Assignment readAssignment(const Statement& row);

    DatabaseConnection& db_;
};

}  // namespace lms::data
