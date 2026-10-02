#pragma once

#include <string>

#include "Statement.h"

struct sqlite3;

namespace lms::data {

// One SQLite database connection. Foreign keys are always switched on.
class DatabaseConnection {
public:
    // Use ":memory:" for a temporary database (tests).
    explicit DatabaseConnection(const std::string& path);
    ~DatabaseConnection();
    DatabaseConnection(const DatabaseConnection&) = delete;
    DatabaseConnection& operator=(const DatabaseConnection&) = delete;

    // For fixed SQL only (the schema, test set-up). Never pass user input here.
    void runScript(const std::string& sql);

    // All queries with values go through prepare() and bind parameters.
    Statement prepare(const std::string& sql);

    long long lastInsertId() const;

    void beginTransaction();
    void commit();
    void rollback();

private:
    sqlite3* db_ = nullptr;
};

}  // namespace lms::data
