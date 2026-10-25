# Quality assurance evidence (Phase 4 inputs to Phases 5 and 6)

Measured on Ubuntu 24.04, GCC 13, CMake 4.4, SQLite 3.45, libsodium 1.0.18, GoogleTest 1.14.

## 1. Tests
138 automated tests, all passing, about 1 second in total (`ctest --test-dir build`).

| Level | Tests | Files |
|---|---|---|
| Unit (domain, no database, no files) | 31 | `test_domain.cpp` |
| Integration (real SQLite, services, file store) | 85 | `test_persistence.cpp`, `test_users.cpp`, `test_submission.cpp`, `test_marking_moderation.cpp`, `test_admin_extra.cpp` |
| Security and non-functional | 10 | `test_security_nfr.cpp`, `test_nfr_load.cpp` |
| Console / end-to-end scripted sessions | 12 | `test_ui.cpp` |

(Parameterised boundary cases are counted individually by GoogleTest; the table groups by file.)
Test technique: **boundary-value analysis** for deadlines/penalties (on the second, +1 s, 24 h, 24 h + 1 s, 72 h, 72 h + 1 s),
marks (-0.01, 0, max, max + 0.01), usernames (2/3/32/33 characters), passwords (7/8 characters), reasons (9/10 characters), file size;
**equivalence partitions** for roles x operations (the `Rbac` and `Admin` matrix tests), file types and statuses;
**state-transition testing** for every allowed and every forbidden life-cycle move (`StateMachine.*`); **fault injection**
(a trigger that makes the version insert fail; a dropped table) and **randomised robustness** (1,500 hostile inputs).

## 2. Coverage of the business-logic layer
`scripts/coverage.sh` (gcov + gcovr; fails below 70 %):

| Layer | Lines | Covered | % |
|---|---|---|---|
| `src/domain` + `src/application` | 937 | 927 | **98.9 %** |
| functions | 180 | 178 | 98.9 % |
| branches (includes exception paths inside the STL) | 1,429 | 938 | 65.6 % |

The 10 uncovered lines are unreachable defensive `return`s after exhaustive `switch` statements.
The infrastructure and presentation layers are exercised by the same integration and scripted-session tests.

## 3. Static analysis
`scripts/static-analysis.sh` runs cppcheck 2.13 and clang-tidy 18 (checks in `.clang-tidy`: `bugprone-*`, `clang-analyzer-*`,
`performance-*`, selected `cppcoreguidelines`/`modernize`/`readability`). **Current result: 0 findings.**
Deliberately suppressed (documented in `.cppcheck-suppressions` / `.clang-tidy`):
* `useStlAlgorithm`: raw loops that build console table rows are clearer than `std::transform` with a lambda.
* `performance-inefficient-vector-operation`: those table-row vectors hold at most a few dozen rows.
* `bugprone-easily-swappable-parameters`: domain functions take several `Id`s by design; wrong order is caught by tests and types.

## 4. Metrics (lizard 1.24)
| Metric | Value |
|---|---|
| Source files / lines | 62 files / 4,515 lines (3,773 non-blank, non-comment) |
| Test code | 10 files / 2,029 lines |
| Functions | 336, average 7.3 lines each |
| Average cyclomatic complexity | 2.4 |
| Highest complexity | `printTable` 13, `main` 12 (option parsing + wiring), `StudentMenu::results` 12, `SqliteSubmissionRepository::find` 12, `AdminService::createUser` 10, `NotificationObserver::onEvent` 10 |
| Functions above CCN 15 | 0 |

## 5. Self code review: checklist and what changed
Checklist: layering respected; no raw owning pointers; every DB value bound as a parameter; every input validated; every
error path is an exception that the UI turns into a message; multi-step changes in a transaction; no secrets in logs/messages;
const-correctness; no duplicated rules; each requirement has a test.

| # | Finding (how found) | Change made |
|---|---|---|
| 1 | cppcheck `passedByValue` on three `User` subclass constructors | take `const UserData&` |
| 2 | cppcheck `shadowFunction`: locals named `title` hid `RoleMenu::title()` | renamed `courseTitle`, `assignmentTitle` |
| 3 | clang-tidy `unchecked-optional-access` (10 places on marking, moderation and repository insert paths); a bad dereference would be a crash instead of a handled error | `departmentOf()` helper that throws `AuthorizationError`; `requireProfile()` that throws `PersistenceError`; explicit `has_value()` checks beside each dereference |
| 4 | clang-tidy `bugprone-empty-catch` in the menu header | extracted `unreadBadge()` which returns `""` on failure (intentional, commented) |
| 5 | clang-tidy widening multiplication on `24*60*60` and `5*60` | `24LL`, `5LL` |
| 6 | clang-tidy `enum-size` | enums use `std::uint8_t` |
| 7 | Review of `SqliteUserRepository::insert`: e-mail was bound as an integer NULL then patched by a second `UPDATE` | bind text or NULL in the single `INSERT` |
| 8 | Test review: a placeholder test that asserted nothing, and one garbled assertion | replaced by real tests (`PlainPasswordAppearsInNoTextColumn...`, `UnmarkedWorkCannotBeModerated`) |
| 9 | Test run after a bulk edit: `ApproveWithoutAdjustment...` failed (an audit row was written on every approval) | fixed the misplaced guard; the test now guards the regression |
| 10 | Building the NFR tests: database `CHECK` rejected my 1-2 character test usernames | confirmed the constraint works; fixed the test data (not a product defect) |

## 6. Not yet verified
* Built and tested only on Linux (Ubuntu 24.04). Windows/macOS were not tried (`termios` password hiding is POSIX-only).
* The GitHub Actions workflow in `.github/workflows/ci.yml` has not run on GitHub (no remote exists yet); its commands were run locally.
* Timings are from a development PC, not the school's old computers; re-measure on the target machine for the acceptance test.
