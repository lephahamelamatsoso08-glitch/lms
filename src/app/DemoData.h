#pragma once

#include "AppContext.h"

namespace lms::app {

// Every demo account uses this password. Demo data is only for trying the program
// and for the demonstration, never for a real school.
inline constexpr const char* kDemoPassword = "Passw0rd1";

// Fills an EMPTY system with an administrator (admin), a lecturer (lecturer1), a head
// of department (hod1), two students (student1, student2), two courses and three
// assignments. Throws std::logic_error if the system already has users.
void seedDemoData(AppContext& context);

}  // namespace lms::app
