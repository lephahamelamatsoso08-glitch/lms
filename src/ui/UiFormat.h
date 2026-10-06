#pragma once

#include <chrono>

#include <QDateTime>
#include <QString>

#include "LateDays.h"

namespace lms::ui {

// Times are stored in UTC and shown in the computer's local time.
inline QString formatTime(lms::domain::TimePoint time) {
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
    return QDateTime::fromSecsSinceEpoch(seconds).toString("ddd d MMM yyyy, HH:mm");
}

// 85 -> "85", 62.5 -> "62.5", 62.25 -> "62.25"
inline QString formatMark(double value) {
    QString text = QString::number(value, 'f', 2);
    if (text.contains('.')) {
        while (text.endsWith('0')) {
            text.chop(1);
        }
        if (text.endsWith('.')) {
            text.chop(1);
        }
    }
    return text;
}

}  // namespace lms::ui
