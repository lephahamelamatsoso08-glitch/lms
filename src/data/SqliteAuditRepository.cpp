#include "SqliteAuditRepository.h"

#include <optional>
#include <stdexcept>

#include "Statement.h"
#include "TimeFormat.h"

namespace lms::data {

using lms::domain::MarkAuditEntry;

SqliteAuditRepository::SqliteAuditRepository(DatabaseConnection& db) : db_(db) {}

int SqliteAuditRepository::add(const MarkAuditEntry& entry) {
    if (entry.id() != 0) {
        throw std::invalid_argument("audit entry is already saved");
    }
    Statement s = db_.prepare(
        "INSERT INTO mark_audit(submission_id, changed_by, changed_at, old_value, new_value, "
        "reason) VALUES (?, ?, ?, ?, ?, ?)");
    s.bindInt(1, entry.submissionId());
    s.bindInt(2, entry.changedBy());
    s.bindText(3, lms::domain::toIso8601(entry.changedAt()));
    if (entry.oldValue()) {
        s.bindDouble(4, *entry.oldValue());
    } else {
        s.bindNull(4);
    }
    s.bindDouble(5, entry.newValue());
    s.bindText(6, entry.reason());
    s.step();
    return static_cast<int>(db_.lastInsertId());
}

MarkAuditEntry SqliteAuditRepository::readEntry(Statement& row) {
    std::optional<double> oldValue;
    if (!row.isNull(4)) {
        oldValue = row.columnDouble(4);
    }
    return MarkAuditEntry(static_cast<int>(row.columnInt(0)), static_cast<int>(row.columnInt(1)),
                          static_cast<int>(row.columnInt(2)),
                          lms::domain::fromIso8601(row.columnText(3)), oldValue,
                          row.columnDouble(5), row.columnText(6));
}

std::vector<MarkAuditEntry> SqliteAuditRepository::findBySubmission(int submissionId) const {
    Statement s = db_.prepare(
        "SELECT id, submission_id, changed_by, changed_at, old_value, new_value, reason "
        "FROM mark_audit WHERE submission_id = ? ORDER BY id");
    s.bindInt(1, submissionId);
    std::vector<MarkAuditEntry> result;
    while (s.step()) {
        result.push_back(readEntry(s));
    }
    return result;
}

std::vector<MarkAuditEntry> SqliteAuditRepository::findAll() const {
    Statement s = db_.prepare(
        "SELECT id, submission_id, changed_by, changed_at, old_value, new_value, reason "
        "FROM mark_audit ORDER BY id");
    std::vector<MarkAuditEntry> result;
    while (s.step()) {
        result.push_back(readEntry(s));
    }
    return result;
}

}  // namespace lms::data
