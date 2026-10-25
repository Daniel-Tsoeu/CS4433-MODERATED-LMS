#!/usr/bin/env bash
# Builds with gcov instrumentation, runs the tests and prints line coverage of
# the business-logic layer (src/domain + src/application). Needs: gcovr.
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build-cov -DCMAKE_BUILD_TYPE=Debug -DLMS_COVERAGE=ON >/dev/null
cmake --build build-cov -j"$(nproc)" >/dev/null
find build-cov -name '*.gcda' -delete
./build-cov/tests/lms_tests >/dev/null
gcovr -r . build-cov --filter 'src/domain/' --filter 'src/application/' \
      --txt --print-summary --html-details build-cov/coverage.html --fail-under-line 70
