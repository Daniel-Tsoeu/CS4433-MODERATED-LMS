#!/usr/bin/env bash
# Runs cppcheck and clang-tidy over src/. Exit status is non-zero on any finding.
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug >/dev/null
cppcheck --enable=warning,style,performance,portability --std=c++17 \
         --suppressions-list=.cppcheck-suppressions -DLMS_VERSION='"1.0.0"' -I src \
         --error-exitcode=1 --quiet src
out=$(clang-tidy -p build --quiet $(find src -name '*.cpp') 2>&1 | grep -E 'warning:|error:' || true)
if [ -n "$out" ]; then echo "$out"; exit 1; fi
echo "static analysis clean"
