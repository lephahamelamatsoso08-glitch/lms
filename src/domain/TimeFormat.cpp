#include "TimeFormat.h"

#include <chrono>
#include <cstdio>
#include <stdexcept>

namespace lms::domain {

namespace {

// Calendar arithmetic (days since 1970-01-01), so the code does not depend
// on platform-specific time functions.
long long daysFromCivil(long long y, int m, int d) {
    y -= (m <= 2) ? 1 : 0;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const long long yoe = y - era * 400;
    const long long mp = (m + 9) % 12;  // March = 0
    const long long doy = (153 * mp + 2) / 5 + d - 1;
    const long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

void civilFromDays(long long z, long long& y, int& m, int& d) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const long long doe = z - era * 146097;
    const long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = yoe + era * 400;
    const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long long mp = (5 * doy + 2) / 153;
    d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    if (m <= 2) {
        ++y;
    }
}

}  // namespace

std::string toIso8601(TimePoint time) {
    const long long secs =
        std::chrono::floor<std::chrono::seconds>(time.time_since_epoch()).count();
    long long days = secs / 86400;
    long long rem = secs % 86400;
    if (rem < 0) {
        rem += 86400;
        --days;
    }
    long long year = 0;
    int month = 0;
    int day = 0;
    civilFromDays(days, year, month, day);

    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%04lld-%02d-%02dT%02d:%02d:%02dZ", year, month, day,
                  static_cast<int>(rem / 3600), static_cast<int>((rem % 3600) / 60),
                  static_cast<int>(rem % 60));
    return std::string(buffer);
}

TimePoint fromIso8601(const std::string& text) {
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0, used = 0;
    if (text.size() != 20 ||
        std::sscanf(text.c_str(), "%4d-%2d-%2dT%2d:%2d:%2dZ%n", &y, &mo, &d, &h, &mi, &s,
                    &used) != 6 ||
        used != 20) {
        throw std::invalid_argument("time must look like 2026-11-07T10:30:15Z");
    }
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 ||
        s > 59) {
        throw std::invalid_argument("time has a value out of range");
    }
    const long long days = daysFromCivil(y, mo, d);
    long long y2 = 0;
    int m2 = 0;
    int d2 = 0;
    civilFromDays(days, y2, m2, d2);
    if (y2 != y || m2 != mo || d2 != d) {
        throw std::invalid_argument("that date does not exist");
    }
    return TimePoint{} + std::chrono::seconds(days * 86400 + h * 3600 + mi * 60 + s);
}

}  // namespace lms::domain
