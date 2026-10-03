#include "SqliteTransactionRunner.h"

namespace lms::data {

SqliteTransactionRunner::SqliteTransactionRunner(DatabaseConnection& db) : db_(db) {}

void SqliteTransactionRunner::runInTransaction(const std::function<void()>& work) {
    db_.beginTransaction();
    try {
        work();
        db_.commit();
    } catch (...) {
        db_.rollback();
        throw;
    }
}

}  // namespace lms::data
