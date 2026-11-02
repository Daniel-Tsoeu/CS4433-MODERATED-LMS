# Quality Assurance Evidence (Phases 4, 5 and 6)

Measured on Ubuntu 24.04, GCC 13, CMake 4.4, SQLite 3.45, libsodium 1.0.18 and GoogleTest 1.14.

## 1. Tests

138 automated tests, all passing, in about 1 second in total (`ctest --test-dir build`).

| Level | Tests | Files |
|---|---:|---|
| Unit (domain, no database, no files) | 31 | `test_domain.cpp` |
| Integration (real SQLite, services, file store) | 85 | `test_persistence.cpp`, `test_users.cpp`, `test_submission.cpp`, `test_marking_moderation.cpp`, `test_admin_extra.cpp` |
| Security and non-functional | 10 | `test_security_nfr.cpp`, `test_nfr_load.cpp` |
| Console / end-to-end scripted sessions | 12 | `test_ui.cpp` |

(Parameterised boundary cases are counted individually by GoogleTest; the table groups by file.)

Test techniques include **boundary-value analysis** for deadlines and penalties (on the second, +1 s, 24 h, 24 h + 1 s, 72 h, 72 h + 1 s), marks (-0.01, 0, max, max + 0.01), usernames (2/3/32/33 characters), passwords (7/8 characters), reasons (9/10 characters) and file size.

**Equivalence partitioning** was used for roles and operations (the `Rbac` and `Admin` matrix tests), file types and statuses.

**State-transition testing** covered every allowed and forbidden life-cycle move (`StateMachine.*`).

**Fault injection** included a trigger that makes the version insert fail and a dropped table. **Randomised robustness testing** used 1,500 hostile inputs.

## 2. Coverage of the Business-Logic Layer

`scripts/coverage.sh` uses gcov and gcovr and fails when line coverage falls below 70%.

| Layer | Lines | Covered | Coverage |
|---|---:|---:|---:|
| `src/domain` + `src/application` | 937 | 927 | **98.9%** |
| Functions | 180 | 178 | 98.9% |
| Branches (includes exception paths inside the STL) | 1,429 | 938 | 65.6% |

The reported 10 uncovered lines are identified in the existing analysis as defensive `return` statements after exhaustive `switch` statements.

The infrastructure and presentation layers are exercised by the same integration and scripted-session tests.

The latest GitHub Actions coverage run also passed the configured 70% line-coverage threshold.

## 3. Static Analysis

`scripts/static-analysis.sh` runs Cppcheck 2.13 and Clang-Tidy 18. The checks configured in `.clang-tidy` include `bugprone-*`, `clang-analyzer-*`, `performance-*` and selected `cppcoreguidelines`, `modernize` and `readability` checks.

**Latest result: 0 reported findings.**

GitHub Actions verification on **2 November 2026** confirmed:

- Cppcheck completed successfully.
- Clang-Tidy completed successfully.
- No static-analysis findings were reported.
- The script returned the output: `static analysis clean`.

Deliberately suppressed checks are documented in `.cppcheck-suppressions` and `.clang-tidy`:

- `useStlAlgorithm`: Raw loops that build console table rows are clearer than `std::transform` with a lambda.
- `performance-inefficient-vector-operation`: Those table-row vectors hold at most a few dozen rows.
- `bugprone-easily-swappable-parameters`: Domain functions take several `Id` values by design; incorrect argument order is checked through tests and types.

## 4. Code Metrics (Lizard 1.24)

| Metric | Value |
|---|---|
| Source files / lines | 62 files / 4,515 lines (3,773 non-blank, non-comment) |
| Test code | 10 files / 2,029 lines |
| Functions | 336, average 7.3 lines each |
| Average cyclomatic complexity | 2.4 |
| Highest complexity | `printTable` 13, `main` 12 (option parsing + wiring), `StudentMenu::results` 12, `SqliteSubmissionRepository::find` 12, `AdminService::createUser` 10, `NotificationObserver::onEvent` 10 |
| Functions above CCN 15 | 0 |

## 5. Self Code Review: Checklist and Changes Made

The following checklist was used during the code review:

- Layering respected.
- No raw owning pointers.
- Every database value bound as a parameter.
- Every input validated.
- Every error path is an exception that the UI turns into a message.
- Multi-step changes handled in a transaction.
- No secrets in logs or messages.
- Const-correctness maintained.
- No duplicated business rules.
- Each requirement has a corresponding test.

| # | Finding (how found) | Change made |
|---:|---|---|
| 1 | Cppcheck `passedByValue` on three `User` subclass constructors | Changed parameters to `const UserData&`. |
| 2 | Cppcheck `shadowFunction`: local variables named `title` hid `RoleMenu::title()` | Renamed them to `courseTitle` and `assignmentTitle`. |
| 3 | Clang-Tidy `unchecked-optional-access` (10 places on marking, moderation and repository insert paths); an invalid dereference could cause a crash instead of a handled error | Added `departmentOf()` helper that throws `AuthorizationError`, `requireProfile()` that throws `PersistenceError`, and explicit `has_value()` checks beside each dereference. |
| 4 | Clang-Tidy `bugprone-empty-catch` in the menu header | Extracted `unreadBadge()`, which returns `""` on failure (intentional and commented). |
| 5 | Clang-Tidy widening multiplication on `24*60*60` and `5*60` | Changed to `24LL` and `5LL`. |
| 6 | Clang-Tidy `enum-size` | Changed enums to use `std::uint8_t`. |
| 7 | Review of `SqliteUserRepository::insert`: e-mail was bound as an integer NULL and then patched by a second `UPDATE` | Changed to bind text or NULL in the single `INSERT`. |
| 8 | Test review found a placeholder test that asserted nothing and one garbled assertion | Replaced them with real tests (`PlainPasswordAppearsInNoTextColumn...`, `UnmarkedWorkCannotBeModerated`). |
| 9 | Test run after a bulk edit: `ApproveWithoutAdjustment...` failed because an audit row was written on every approval | Fixed the misplaced guard; the test now guards against regression. |
| 10 | During NFR test development, the database `CHECK` rejected 1–2 character test usernames | Confirmed that the constraint works and corrected the test data. This was not a product defect. |

## 6. Verification and Remaining Limitations

### GitHub Actions verification

The CI workflow in `.github/workflows/ci.yml` was successfully executed on GitHub Actions on **2 November 2026**.

Both workflow jobs completed successfully:

1. **Build and Test:** The project configured, compiled and completed its automated test run successfully.
2. **Quality Assurance:** Static analysis completed with zero reported findings, and the coverage script reported 98.9% business-logic line coverage.

The workflow runs on Ubuntu 24.04 and is configured to execute on pushes and pull requests.

### Remaining limitations

- The project has been built and tested on Linux (Ubuntu 24.04). Windows and macOS have not been fully tested (`termios` password hiding is POSIX-only).
- Timings are from a development PC, not the school's older computers. Performance should be measured again on the target machine during acceptance testing.
