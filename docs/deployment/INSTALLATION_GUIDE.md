# Moderated LMS — School Technician Installation Guide (v1.0)

## Scope and prerequisites
This guide describes deployment of the Windows desktop build of the CS4433 Moderated LMS. It is an offline application using a local SQLite database. The packaged release is produced on Windows with MSYS2 UCRT64; school users do not need a compiler or MSYS2 to run the packaged build, but this must be confirmed by testing on a separate machine. Use a supported Windows computer with sufficient free disk space, a writable application-data directory and permission to run locally installed applications.

## Package preparation (developer)
From the repository root in MSYS2 UCRT64:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./tools/package_windows.sh
```

The script builds `dist/lms/lms.exe`, copies its required runtime DLLs and Qt plugins, and recreates `dist/lms`. **It removes and replaces that package folder**; do not store user data there.

## School installation
1. Transfer the complete `dist/lms` directory to the target computer, using an approved school transfer method. Do not transfer demonstration databases or test credentials as production data.
2. Copy it to a writeable installation folder (for example, `C:\SchoolApps\ModeratedLMS`). Preserve the DLLs and the `platforms` directory beside `lms.exe`.
3. Launch `lms.exe` normally. Do **not** add `--demo` for real use.
4. On the first start, create the administrator account with a unique strong password. Store credentials according to school policy.
5. Log in as Administrator; create the school's departments, courses and user accounts; assign lecturers and enrol students.
6. Test role-based access with school-approved test accounts, including that students cannot see unreleased marks.

## Database and backups
Without `--db`, Windows stores the default database at `%APPDATA%\LMS\lms.db`, as documented by the project. Keep backups of this file in access-restricted storage; close the application before making a simple file-copy backup. Restore only from a trusted backup after closing the application. Confirm permissions and test restoration on a non-production copy.

## Verification checklist
- [ ] `lms.exe` opens without a missing-DLL message
- [ ] Administrator can log in and view users/courses
- [ ] Lecturer can view assigned courses and create an assignment
- [ ] Student can see enrolled courses but not unreleased marks
- [ ] HoD can moderate and release a test mark
- [ ] Restart preserves a test record in SQLite
- [ ] App works without internet connectivity

## Troubleshooting
If a DLL or Qt platform plugin is missing, confirm the **entire** distribution folder was copied and rerun `tools/package_windows.sh` on the build computer. For permission errors, use a writable application-data directory and check that school policies permit the application. Do not disable Windows security protections as a routine workaround; ask the school's IT administrator to review application restrictions. For database issues, preserve the error message and a safe backup before making changes.

## Known limits
A single SQLite file is intended for one local application instance; simultaneous editing from several machines is not supported by this design. Submitted work is stored by file path rather than copied, so original file paths must stay accessible. The release's portability to computers without MSYS2 is **not yet independently verified**.
