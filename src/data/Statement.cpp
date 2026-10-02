#include "Statement.h"

#include <sqlite3.h>

#include "DatabaseError.h"

namespace lms::data {

Statement::Statement(sqlite3* db, const std::string& sql) : db_(db) {
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK) {
        throw DatabaseError(std::string("cannot prepare statement: ") + sqlite3_errmsg(db_));
    }
}

Statement::~Statement() { sqlite3_finalize(stmt_); }

Statement::Statement(Statement&& other) noexcept : db_(other.db_), stmt_(other.stmt_) {
    other.stmt_ = nullptr;
}

void Statement::check(int resultCode) const {
    if (resultCode != SQLITE_OK) {
        throw DatabaseError(std::string("database error: ") + sqlite3_errmsg(db_));
    }
}

void Statement::bindInt(int index, long long value) {
    check(sqlite3_bind_int64(stmt_, index, value));
}

void Statement::bindDouble(int index, double value) {
    check(sqlite3_bind_double(stmt_, index, value));
}

void Statement::bindText(int index, const std::string& value) {
    check(sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()),
                            SQLITE_TRANSIENT));
}

void Statement::bindNull(int index) { check(sqlite3_bind_null(stmt_, index)); }

bool Statement::step() {
    const int result = sqlite3_step(stmt_);
    if (result == SQLITE_ROW) {
        return true;
    }
    if (result == SQLITE_DONE) {
        return false;
    }
    throw DatabaseError(std::string("statement failed: ") + sqlite3_errmsg(db_));
}

void Statement::reset() {
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
}

long long Statement::columnInt(int column) const { return sqlite3_column_int64(stmt_, column); }

double Statement::columnDouble(int column) const { return sqlite3_column_double(stmt_, column); }

std::string Statement::columnText(int column) const {
    const unsigned char* text = sqlite3_column_text(stmt_, column);
    return text ? std::string(reinterpret_cast<const char*>(text)) : std::string();
}

bool Statement::isNull(int column) const {
    return sqlite3_column_type(stmt_, column) == SQLITE_NULL;
}

}  // namespace lms::data
