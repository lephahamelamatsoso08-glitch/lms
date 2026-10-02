#pragma once

#include <vector>

#include "DatabaseConnection.h"
#include "IAuditRepository.h"

namespace lms::data {

class SqliteAuditRepository : public lms::domain::IAuditRepository {
public:
    explicit SqliteAuditRepository(DatabaseConnection& db);

    int add(const lms::domain::MarkAuditEntry& entry) override;
    std::vector<lms::domain::MarkAuditEntry> findBySubmission(int submissionId) const override;
    std::vector<lms::domain::MarkAuditEntry> findAll() const override;

private:
    static lms::domain::MarkAuditEntry readEntry(Statement& row);

    DatabaseConnection& db_;
};

}  // namespace lms::data
