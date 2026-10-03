#pragma once

#include <vector>

#include "Notification.h"

namespace lms::domain {

class INotificationRepository {
public:
    virtual ~INotificationRepository() = default;

    // Saves a new notification (its id must be 0) and returns the new id.
    virtual int add(const Notification& notification) = 0;

    virtual std::vector<Notification> findForUser(int userId) const = 0;  // newest first
    virtual int countUnread(int userId) const = 0;

    // Marks one of this user's notifications as read. Returns false if that
    // notification does not exist or belongs to someone else.
    virtual bool markRead(int userId, int notificationId) = 0;
};

}  // namespace lms::domain
