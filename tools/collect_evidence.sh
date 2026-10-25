#!/usr/bin/env bash
# Collects the evidence for Phase 5 and Phase 6 into docs/evidence/.
# Usage:  tools/collect_evidence.sh          (without the slow window tests)
#         tools/collect_evidence.sh --all    (also runs ALL tests, takes several minutes)
set -u
cd "$(dirname "$0")/.."
OUT=docs/evidence
mkdir -p "$OUT"

echo "== 1. Coverage of the business-logic layers (domain and application) =="
cmake -S . -B build/cov -G Ninja -DLMS_COVERAGE=ON -DLMS_BUILD_UI=OFF > /dev/null
cmake --build build/cov > "$OUT/coverage-build.txt" 2>&1
ctest --test-dir build/cov --output-on-failure > "$OUT/coverage-tests.txt" 2>&1
tail -3 "$OUT/coverage-tests.txt"
gcovr --root . --object-directory build/cov --filter 'src/domain/' --filter 'src/application/' \
      --txt > "$OUT/coverage.txt" 2>&1
gcovr --root . --object-directory build/cov --filter 'src/domain/' --filter 'src/application/' \
      --print-summary 2>&1 | tail -4

echo "== 2. Static analysis with cppcheck =="
cppcheck --enable=warning,style,performance,portability --std=c++17 --library=qt \
  --suppress=missingInclude --suppress=missingIncludeSystem --inline-suppr \
  --template='{file}:{line}: ({severity}) {message} [{id}]' \
  -I src/domain -I src/data -I src/application -I src/app -I src/ui \
  src 2> "$OUT/cppcheck.txt" > /dev/null
echo "cppcheck findings: $(grep -c -E '\((error|warning|style|performance|portability)\)' "$OUT/cppcheck.txt")"

echo "== 3. Static analysis with clang-tidy (core layers) =="
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > /dev/null
FILES=$(find src/domain src/application src/data src/app -name '*.cpp')
clang-tidy -p build --quiet \
  -checks='clang-analyzer-*,bugprone-*,-bugprone-easily-swappable-parameters,performance-*,-performance-enum-size,modernize-use-nullptr,modernize-use-override' \
  $FILES > "$OUT/clang-tidy.txt" 2>&1
echo "clang-tidy warnings: $(grep -c ' warning: ' "$OUT/clang-tidy.txt")  errors: $(grep -c ' error: ' "$OUT/clang-tidy.txt")"

echo "== 4. Code metrics =="
{
  echo "Lines of code (all lines, including comments and blank lines)"
  for dir in src/domain src/data src/application src/app src/ui tests; do
    printf '%-18s %6s lines in %s files\n' "$dir" \
      "$(cat $(find $dir -name '*.cpp' -o -name '*.h') 2>/dev/null | wc -l)" \
      "$(find $dir -name '*.cpp' -o -name '*.h' | wc -l)"
  done
  echo
  echo "Cyclomatic complexity (lizard). Functions with complexity above 8 are listed."
  lizard src -l cpp -C 8 -w
  echo
  lizard src -l cpp | tail -12
} > "$OUT/metrics.txt" 2>&1
tail -8 "$OUT/metrics.txt"

echo "== 5. List of all tests =="
ctest --test-dir build -N > "$OUT/test-list.txt" 2>&1
tail -1 "$OUT/test-list.txt"

if [ "${1:-}" = "--all" ]; then
  echo "== 6. Running ALL tests (this takes several minutes) =="
  cmake -S . -B build -G Ninja -DLMS_BUILD_UI=ON > /dev/null
  cmake --build build > /dev/null 2>&1
  ctest --test-dir build --output-on-failure > "$OUT/test-results.txt" 2>&1
  tail -4 "$OUT/test-results.txt"
fi
echo "Done. The files are in $OUT"
