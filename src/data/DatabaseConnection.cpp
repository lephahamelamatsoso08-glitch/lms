#include "DatabaseConnection.h"

#include <sqlite3.h>

#include "DatabaseError.h"

namespace lms::data {

DatabaseConnection::DatabaseConnection(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        const std::string message = db_ ? sqlite3_errmsg(db_) : "out of memory";
        sqlite3_close(db_);
        throw DatabaseError("cannot open database: " + message);
    }
    try {
        runScript("PRAGMA foreign_keys = ON;");
    } catch (...) {
        sqlite3_close(db_);
        throw;
    }
}

DatabaseConnection::~DatabaseConnection() { sqlite3_close(db_); }

void DatabaseConnection::runScript(const std::string& sql) {
    char* error = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
        const std::string message = error ? error : "unknown error";
        sqlite3_free(error);
        throw DatabaseError("SQL script failed: " + message);
    }
}

Statement DatabaseConnection::prepare(const std::string& sql) { return Statement(db_, sql); }

long long DatabaseConnection::lastInsertId() const { return sqlite3_last_insert_rowid(db_); }

void DatabaseConnection::beginTransaction() { runScript("BEGIN"); }
void DatabaseConnection::commit() { runScript("COMMIT"); }
void DatabaseConnection::rollback() { runScript("ROLLBACK"); }

}  // namespace lms::data
