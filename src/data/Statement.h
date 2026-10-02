#pragma once

#include <string>

struct sqlite3;
struct sqlite3_stmt;

namespace lms::data {

// One prepared SQL statement. Values are always bound as parameters and are
// never pasted into the SQL text (NFR4). It must not outlive its connection.
class Statement {
public:
    Statement(sqlite3* db, const std::string& sql);
    ~Statement();
    Statement(Statement&& other) noexcept;
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement& operator=(Statement&&) = delete;

    // Parameter numbers start at 1.
    void bindInt(int index, long long value);
    void bindDouble(int index, double value);
    void bindText(int index, const std::string& value);
    void bindNull(int index);

    // true = a row is ready, false = finished. Throws DatabaseError on failure.
    bool step();
    void reset();

    // Column numbers start at 0.
    long long columnInt(int column) const;
    double columnDouble(int column) const;
    std::string columnText(int column) const;
    bool isNull(int column) const;

private:
    void check(int resultCode) const;

    sqlite3* db_;
    sqlite3_stmt* stmt_ = nullptr;
};

}  // namespace lms::data
