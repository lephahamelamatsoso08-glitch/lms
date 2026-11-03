# Software Quality Dashboard

**Project:** Moderated Learning Management System  
**Course:** CS4433 Software Engineering  
**Release:** v1.1

[![CI](https://github.com/lephahamelamatsoso08-glitch/lms/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/lephahamelamatsoso08-glitch/lms/actions/workflows/ci.yml)

> This page reports recorded project evidence. Historical measurements are distinguished from current CI status.

## Quality Metrics

| Quality indicator | Recorded result | Evidence |
|---|---|---|
| Automated regression tests | **489/489 passed** | [Test results](evidence/test-results.txt) |
| Test failures | **0** | [Test results](evidence/test-results.txt) |
| Test execution time | **139.96 seconds** | [Test results](evidence/test-results.txt) |
| Domain/application line coverage | **98% (960/980 lines)** | [Coverage report](evidence/coverage.txt) |
| C++ functions analysed | **415** | [Complexity metrics](evidence/metrics.txt) |
| Average cyclomatic complexity | **2.3** | [Complexity metrics](evidence/metrics.txt) |
| Functions exceeding configured complexity threshold | **0** | [Complexity metrics](evidence/metrics.txt) |

**Coverage limitation:** The recorded 98% line-coverage result predates the final v1.1 maintenance change. It is not a complete v1.1 coverage measurement.

**Metrics limitation:** Complexity measurements are from the saved analysis report and may not include subsequent code changes.

## Verification and Validation

### Verification

Verification evaluates whether implementation behaviour conforms to specified requirements.

The project uses:

- **Unit tests:** Domain entities and business rules.
- **Integration tests:** Services, SQLite repositories and database operations.
- **UI tests:** Selected graphical interface behaviours.
- **Regression testing:** Checks for unintended effects after changes.

The complete recorded regression run passed **489 of 489 tests**.

### Validation

Validation evaluates whether the software supports its intended use.

Recorded manual checks include:

- Administrator course, lecturer and student administration.
- Opening the packaged Windows application.
- Lecturer access to the class-wide extension feature.
- Confirmation that an updated deadline appeared in a student's assignment list.

These checks provide selected acceptance evidence; they are not a comprehensive school deployment acceptance test.

## Static Analysis and Maintainability

| Tool | Purpose |
|---|---|
| GoogleTest | Automated functional testing |
| CTest | Test execution and aggregation |
| gcovr | Line-coverage measurement |
| cppcheck | C++ static analysis |
| clang-tidy | Additional source-code analysis |
| Lizard | Complexity and code-size metrics |
| GitHub Actions | Automated builds and tests |

### Recorded Evidence

- [cppcheck report](evidence/cppcheck.txt)
- [clang-tidy report](evidence/clang-tidy.txt)
- [complexity metrics](evidence/metrics.txt)
- [coverage report](evidence/coverage.txt)
- [complete test results](evidence/test-results.txt)

Passing tests, high coverage and low measured complexity are useful quality indicators, but they do not guarantee defect-free software.

## Continuous Integration

GitHub Actions executes automated build and test workflows.

[View live CI workflow results](https://github.com/lephahamelamatsoso08-glitch/lms/actions/workflows/ci.yml)

The status badge above displays the current workflow result. Historical CI runs should not be treated as proof that every future commit will pass.

## Requirements Traceability and Maintenance

Change request **CR1** introduced class-wide assignment deadline extensions.

Impact areas included:

1. Assignment application services.
2. Course enrolment and deadline rules.
3. Database transaction integration.
4. Lecturer interface controls.
5. Regression tests.

The feature was developed on `feature/class-wide-extensions`, reviewed through automated and manual checks, merged into `main`, and released as `v1.1`.

[Read the CR1 impact analysis](maintenance/CR1_CLASS_WIDE_EXTENSIONS.md)

## Known Limitations and Future Quality Work

- Portability to a separate Windows computer without MSYS2 has not been independently confirmed.
- The application is designed primarily for local desktop use, not concurrent multi-computer access.
- Submitted file paths must remain accessible.
- More extensive security, recovery and user-acceptance testing is recommended.
- Coverage should be regenerated for the final v1.1 source baseline.

## Project Resources

- [Final report](Final_Report.pdf)
- [UML diagrams](DIAGRAM_INDEX.md)
- [Project roadmap](ROADMAP.md)
- [Windows release v1.1](https://github.com/lephahamelamatsoso08-glitch/lms/releases/tag/v1.1)
