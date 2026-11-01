# Release Notes — Moderated LMS v1.0

**Status:** Draft release notes. Fill in the exact Git commit, tag date and packaged ZIP checksum once the release is created.

## Included capabilities
- C++17 / Qt 6 Windows desktop application with SQLite persistence and four user roles.
- Administrator account/course/department management, lecturer assignment and student enrolment.
- Lecturer assignment creation, individual extensions, submission marking and written feedback.
- Student submission and on-time resubmission, late-day/penalty rules.
- HoD review, moderation and release of approved marks; audit history and student notices.
- Password hashing using Argon2id/libsodium, role and ownership checks, parameterised SQLite queries.

## Verification evidence
User-reported MSYS2 UCRT64 result on 1 November 2026: 485/485 CTest cases passed after the optional-safety fix (148.27 seconds). Business-logic line coverage was reported as 960/980 = 98% using gcovr. Lizard reported 6,150 NLOC, 415 functions and average cyclomatic complexity 2.3. The Windows folder package was generated at `dist/lms` (31 items, approximately 79 MB) and successfully launched and logged in on the development computer.

## Known limitations
- SQLite database is designed for one local instance; simultaneous multi-computer editing is unsupported.
- Submitted files are referenced by path, not copied into the database.
- A released mark cannot currently be edited within the application.
- Whole-class extension functionality is reserved for maintenance release v1.1.
- A clean-machine portability test without MSYS2 has not yet been recorded.

## Deployment
Follow `INSTALLATION_GUIDE.md` and `BUILDING.md`. Do not use `--demo` or the shared sample credentials for production use.

## Release record (complete after verification)
- Git tag: `v1.0`
- Commit SHA: **Pending**
- Artifact path / checksum: **Pending**
- Release date: **Pending**
