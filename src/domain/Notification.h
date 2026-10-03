#pragma once

#include <string>

#include "LateDays.h"

namespace lms::domain {

// An in-app notice for one user (FR18).
class Notification {
public:
    Notification(int id, int userId, std::string message, TimePoint createdAt, bool isRead);

    int id() const { return id_; }  // 0 means "not saved yet"
    int userId() const { return userId_; }
    const std::string& message() const { return message_; }
    TimePoint createdAt() const { return createdAt_; }
    bool isRead() const { return isRead_; }

private:
    int id_;
    int userId_;
    std::string message_;
    TimePoint createdAt_;
    bool isRead_;
};

}  // namespace lms::domain
