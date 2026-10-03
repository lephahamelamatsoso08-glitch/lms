#pragma once

#include <functional>

#include "DatabaseConnection.h"
#include "ITransactionRunner.h"

namespace lms::data {

class SqliteTransactionRunner : public lms::domain::ITransactionRunner {
public:
    explicit SqliteTransactionRunner(DatabaseConnection& db);
    void runInTransaction(const std::function<void()>& work) override;

private:
    DatabaseConnection& db_;
};

}  // namespace lms::data
