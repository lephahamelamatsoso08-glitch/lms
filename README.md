# Moderated Learning Management System (LMS)

A desktop application for a secondary school. It manages courses, assignments and
assessment for four kinds of user, and it makes sure that students see a mark only after
the Head of Department has checked and released it. This is the individual project for
CS4433 Software Engineering at the National University of Lesotho.

## What it does

| Role | Main functions |
|---|---|
| Administrator | Creates users, deactivates and reactivates accounts, resets passwords, creates departments and courses, assigns lecturers, enrols students, reads the audit log |
| Lecturer | Creates assignments, grants extensions, marks and re-marks work with feedback, sees every enrolled student, opens the latest file |
| Head of Department | Reviews the marked work of the department, approves marks, changes a mark with a reason, releases marks, reads the history of a mark |
| Student | Sees courses and assignments with their own deadline, submits and resubmits files, sees released marks and feedback, reads notices |

Main rules:
- Late work is always accepted. Each late day removes 10% of the maximum mark, for up to
  three days. After that the mark is 0.
- A lecturer can give one student a later deadline with a reason.
- A student can resubmit before their deadline. Old versions are kept.
- Marks stay hidden from students until the Head of Department releases them.
- Every change of a mark is saved in an audit log (who, when, old value, new value, reason).
- The system works offline. Data is stored in one SQLite file.

## Quick start

1. Install the tools and build the program. See [BUILDING.md](BUILDING.md).
2. Start it with demonstration data:

       ./build/src/ui/lms --db demo.db --demo

   The demo accounts are `admin`, `lecturer1`, `hod1`, `student1` and `student2`.
   They all have the password `Passw0rd1`. Demo data is only for trying the program.

## Technology

C++17, CMake and Ninja, Qt 6 Widgets, SQLite, libsodium (Argon2id password hashing),
GoogleTest, GitHub Actions, cppcheck, clang-tidy and gcovr.

## Project layout

| Folder | Content |
|---|---|
| `src/domain` | Entities, business rules and the interfaces of the lower layers. No Qt, no SQL |
| `src/application` | Services that implement the use cases |
| `src/data` | SQLite repositories, transaction runner and password hasher |
| `src/app` | Composition root (`AppContext`) and demonstration data |
| `src/ui` | Qt windows and dialogs |
| `tests` | Automated tests (GoogleTest) |
| `db` | The database schema (`schema.sql`) |
| `docs` | Branching strategy and evidence reports |
| `tools` | Scripts for the evidence reports and for the Windows package |

Dependencies point downwards only: `ui` calls `application`, `application` depends only
on `domain`, and `data` implements interfaces owned by `domain`.

## Tests and quality

Run all tests with `ctest --test-dir build`. Every push runs the build and the tests on
Linux and on Windows (GitHub Actions). The script `tools/collect_evidence.sh` creates the
coverage report, the static analysis reports and the code metrics in `docs/evidence`.

## More documents

- [BUILDING.md](BUILDING.md): installation, build, test and packaging
- [CODING_STANDARD.md](CODING_STANDARD.md): rules for the code
- [docs/BRANCHING.md](docs/BRANCHING.md): branches, pull requests and commit messages
