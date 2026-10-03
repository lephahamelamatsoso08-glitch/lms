#pragma once

#include <functional>

namespace lms::domain {

// Runs several saves as one unit: all of them happen, or none of them.
// If the work throws, everything it saved is undone and the exception continues.
class ITransactionRunner {
public:
    virtual ~ITransactionRunner() = default;
    virtual void runInTransaction(const std::function<void()>& work) = 0;
};

}  // namespace lms::domain
