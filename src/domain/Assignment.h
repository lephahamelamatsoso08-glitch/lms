#pragma once

#include <optional>
#include <string>

#include "Extension.h"
#include "LateDays.h"

namespace lms::domain {

// An assignment with a deadline and a maximum mark (FR6).
class Assignment {
public:
    Assignment(int id, int courseId, std::string title, TimePoint deadline, double maxMark);

    int id() const { return id_; }  // 0 means "not saved yet"
    int courseId() const { return courseId_; }
    const std::string& title() const { return title_; }
    TimePoint deadline() const { return deadline_; }
    double maxMark() const { return maxMark_; }

    // The deadline that applies to one student: the extension if there is
    // one, otherwise the normal deadline (rule C2).
    TimePoint deadlineFor(const std::optional<Extension>& extension) const;

    // FR7: creates an extension for one student. The new deadline must be
    // later than the normal deadline and the reason must not be empty.
    Extension grantExtension(int studentId, TimePoint newDeadline,
                             const std::string& reason) const;

private:
    int id_;
    int courseId_;
    std::string title_;
    TimePoint deadline_;
    double maxMark_;
};

}  // namespace lms::domain
