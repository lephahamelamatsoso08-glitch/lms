#include "TimeFormat.h"

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace lms::domain {

namespace {

// Calendar arithmetic (days since 1970-01-01), so the code does not depend on
// platform-specific time functions.
struct CivilDate {
    long long year;
    int month;
    int day;
};

long long daysFromCivil(const CivilDate& date) {
    const long long y = date.year - (date.month <= 2 ? 1 : 0);
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const long long yoe = y - era * 400;
    const long long mp = (date.month + 9) % 12;  // March = 0
    const long long doy = (153 * mp + 2) / 5 + date.day - 1;
    const long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

CivilDate civilFromDays(long long days) {
    const long long z = days + 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const long long doe = z - era * 146097;
    const long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long long mp = (5 * doy + 2) / 153;
    CivilDate result;
    result.day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    result.month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    result.year = yoe + era * 400 + (result.month <= 2 ? 1 : 0);
    return result;
}

// The number made of `count` digits from `start`, or -1 if one of them is not a digit.
int digits(const std::string& text, std::size_t start, std::size_t count) {
    int value = 0;
    for (std::size_t i = start; i < start + count; ++i) {
        if (text[i] < '0' || text[i] > '9') {
            return -1;
        }
        value = value * 10 + (text[i] - '0');
    }
    return value;
}

bool inRange(int value, int low, int high) { return value >= low && value <= high; }

// "YYYY-MM-DDTHH:MM:SSZ" has exactly 20 characters.
bool hasIsoShape(const std::string& text) {
    return text.size() == 20 && text[4] == '-' && text[7] == '-' && text[10] == 'T' &&
           text[13] == ':' && text[16] == ':' && text[19] == 'Z';
}

CivilDate parseDate(const std::string& text) {
    const CivilDate date{digits(text, 0, 4), digits(text, 5, 2), digits(text, 8, 2)};
    if (date.year < 0 || !inRange(date.month, 1, 12) || !inRange(date.day, 1, 31)) {
        throw std::invalid_argument("time has a value out of range");
    }
    // The date must exist (for example, there is no 30 February).
    const CivilDate back = civilFromDays(daysFromCivil(date));
    if (back.year != date.year || back.month != date.month || back.day != date.day) {
        throw std::invalid_argument("that date does not exist");
    }
    return date;
}

long long parseSecondsOfDay(const std::string& text) {
    const int hour = digits(text, 11, 2);
    const int minute = digits(text, 14, 2);
    const int second = digits(text, 17, 2);
    if (!inRange(hour, 0, 23) || !inRange(minute, 0, 59) || !inRange(second, 0, 59)) {
        throw std::invalid_argument("time has a value out of range");
    }
    return hour * 3600LL + minute * 60LL + second;
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
    const CivilDate date = civilFromDays(days);

    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%04lld-%02d-%02dT%02d:%02d:%02dZ", date.year, date.month,
                  date.day, static_cast<int>(rem / 3600), static_cast<int>((rem % 3600) / 60),
                  static_cast<int>(rem % 60));
    return std::string(buffer);
}

TimePoint fromIso8601(const std::string& text) {
    if (!hasIsoShape(text)) {
        throw std::invalid_argument("time must look like 2026-11-07T10:30:15Z");
    }
    const long long days = daysFromCivil(parseDate(text));
    const long long seconds = parseSecondsOfDay(text);
    return TimePoint{} + std::chrono::seconds(days * 86400LL + seconds);
}

}  // namespace lms::domain
