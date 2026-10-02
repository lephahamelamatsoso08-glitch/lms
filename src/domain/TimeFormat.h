#pragma once

#include <string>

#include "LateDays.h"

namespace lms::domain {

// All times in the database are ISO-8601 UTC text, for example
// "2026-11-07T10:30:15Z". Whole seconds only.
std::string toIso8601(TimePoint time);

// Throws std::invalid_argument if the text is not a real date and time.
TimePoint fromIso8601(const std::string& text);

}  // namespace lms::domain
