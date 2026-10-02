# Coding Standard (LMS)

Language: C++17. Based on the C++ Core Guidelines.

## Naming and files
- Classes and enums: `PascalCase`. Functions and variables: `camelCase`.
- Private data members end with an underscore (`ratePerDay_`).
- One class per header and source file, named after the class. Use `#pragma once`.
- Namespaces follow the layers: `lms::domain`, `lms::application`, `lms::data`, `lms::ui`.

## Architecture rules
- Dependencies point down only: ui -> application -> domain <- data.
- The domain layer must not include Qt or SQLite headers.
- SQL appears only in the data layer, and only as parameterised statements.

## Safety
- No raw `new` or `delete`. Use `std::unique_ptr` and RAII.
- Mark everything `const` that can be `const`.
- Never put `using namespace` in a header.
- Passwords are never stored or logged in plain text.

## Errors
- Report invalid input and failures with exceptions (`std::invalid_argument` and project exception types).
- Never swallow an exception silently. The UI layer catches at the boundary and shows a message.
- Destructors never throw.

## Format
- 4 spaces, no tabs. Maximum 100 characters per line.
- Opening brace on the same line. Always use braces for `if`, `for` and `while`.

## Warnings and analysis
- Build with `-Wall -Wextra -Wpedantic`. The target is zero warnings.
- Run cppcheck before each merge and address its findings.

## Tests
- Every public function in the domain and application layers has unit tests.
- Test names describe the behaviour (`DayFourGivesZero`).

## Commits
- Format: `type(scope): short message`.
- Types: feat, fix, test, docs, refactor, build, ci.
- One logical change per commit. Mention requirement IDs (for example FR11) in the message body when useful.
