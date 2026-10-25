# Building, testing and running

## 1. What you need

You need a C++17 compiler, CMake (3.20 or newer), Ninja, Git, SQLite, libsodium, Qt 6
(Widgets) and GoogleTest. Qt is only needed for the windows. The business logic and its
tests build without it.

### Windows (MSYS2)

1. Install MSYS2 from msys2.org into `C:\msys64`.
2. Open the **MSYS2 UCRT64** terminal (not the plain MSYS one) and run:

       pacman -Syu
       pacman -S --needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake \
         mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-sqlite3 \
         mingw-w64-ucrt-x86_64-libsodium mingw-w64-ucrt-x86_64-qt6-base \
         mingw-w64-ucrt-x86_64-gtest git

### Ubuntu or Debian

    sudo apt update
    sudo apt install build-essential cmake ninja-build git pkg-config \
      libsqlite3-dev libsodium-dev qt6-base-dev libgtest-dev

## 2. Build

    git clone <repository link> lms
    cd lms
    cmake -S . -B build -G Ninja
    cmake --build build

CMake options:

| Option | Default | Meaning |
|---|---|---|
| `LMS_BUILD_UI` | ON | Build the Qt windows and the program. Use `-DLMS_BUILD_UI=OFF` to build only the logic and its tests |
| `LMS_BUILD_TESTS` | ON | Build the automated tests |
| `LMS_COVERAGE` | OFF | Build with coverage measurement (used by CI) |

## 3. Test

    ctest --test-dir build --output-on-failure

The window tests need no screen (Qt runs in "offscreen" mode). They create users with real
password hashing, so a full run takes a few minutes. Use `ctest --test-dir build -j4` to
run tests in parallel, or build with `-DLMS_BUILD_UI=OFF` for a fast run of the logic tests.

## 4. Run

    ./build/src/ui/lms --db demo.db --demo      (demonstration data, all passwords Passw0rd1)
    ./build/src/ui/lms                          (real use)

On Windows the program is `lms.exe`. On the first start of an empty database the program asks
you to create the first administrator. Without `--db`, the database is stored in the
application data folder of the user (on Windows `%APPDATA%\LMS\lms.db`, on Linux
`~/.local/share/LMS/lms.db`).

## 5. Make a package for Windows

The program needs the DLL files that came with the tools. This script copies them into one
folder, so the program starts by double-click on a computer without MSYS2 or Qt:

    ./tools/package_windows.sh

The result is in `dist/lms`. Copy that folder to the school computer and start `lms.exe`.

## 6. Reports for quality evidence

    ./tools/collect_evidence.sh          (coverage, cppcheck, clang-tidy, metrics)
    ./tools/collect_evidence.sh --all    (also runs all tests)

This needs `gcovr`, `cppcheck`, `clang-tidy` and `lizard`. The results are in `docs/evidence`.

## 7. Problems and answers

| Problem | Answer |
|---|---|
| `libgcc_s_seh-1.dll was not found` when you double-click the program | Start it from the MSYS2 UCRT64 terminal, or use the package from section 5 |
| Windows says "An Application Control policy has blocked this file" | Smart App Control blocks the tools. Develop in a Linux machine, or switch Smart App Control off in Windows Security |
| CMake cannot download GoogleTest (SSL certificate error) | Install GoogleTest with the package manager (`mingw-w64-ucrt-x86_64-gtest` or `libgtest-dev`) |
| The window tests are slow | Each test creates users with real password hashing. Use `-j4`, or `-DLMS_BUILD_UI=OFF` |
