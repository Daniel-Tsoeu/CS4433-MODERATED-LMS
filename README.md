# Moderated Learning Management System (LMS) v1.0

CS4433 Software Engineering, final project, Phase 4 (Implementation).
C++17 console application with SQLite persistence for a school with four roles:
**Administrator, Lecturer, Head of Department (HoD), Student**. Marks pass through a
moderation gate: a student sees a mark only after the HoD releases it.

## 1. Build guide

Tested on Ubuntu 24.04 with GCC 13 and CMake 3.28/4.x. Any C++17 compiler on Linux should work
(the console code uses POSIX `termios` to hide password typing).

```bash
# 1. Dependencies (Debian/Ubuntu)
sudo apt-get install -y g++ cmake pkg-config libsqlite3-dev libsodium-dev libgtest-dev

# 2. Configure and build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

# 3. Run the automated tests (138 tests, about 1 second)
ctest --test-dir build --output-on-failure

# 4. Run the program
./build/lms                                   # uses ./lms.db and ./uploads
./build/lms --db /var/lms/lms.db --uploads /var/lms/uploads --log /var/lms/lms-error.log
```

| Library | Why | Package |
|---|---|---|
| SQLite 3 | embedded database, works offline | `libsqlite3-dev` |
| libsodium | Argon2id password hashing | `libsodium-dev` |
| GoogleTest | unit and integration tests | `libgtest-dev` (otherwise CMake downloads v1.14) |

CMake options: `-DLMS_BUILD_TESTS=OFF` (skip tests), `-DLMS_COVERAGE=ON` (gcov),
`-DLMS_WARNINGS_AS_ERRORS=ON` (used by CI).

## 2. First run

1. Start `./build/lms`. On an empty database it asks you to create the **administrator**
   (password: 8+ characters with a letter and a digit; it is hashed, never stored).
2. Log in as the administrator and, in this order: create a department, create users
   (lecturer, HoD, students), create a course, assign the lecturer, enrol the students.
3. The lecturer creates an assignment (deadline format `YYYY-MM-DD HH:MM`, local time).
4. A student submits a file (path of a pdf/docx/txt/zip/source file up to 20 MB).
5. The lecturer marks it (raw mark + written feedback); the HoD approves/adjusts it and
   releases it; the student then sees the mark and feedback.

## 3. Rules implemented

| Rule | Where |
|---|---|
| Late penalty: 10 % of the *maximum* mark per started day for 1-3 days; zero after that. `daysLate = ceil((submitted - effectiveDeadline)/24h)`; exactly on the deadline is on time. `final = max(0, raw - penalty% x max)` | `domain/LatePenaltyPolicy` (Strategy), `Submission::computeFinalMark` |
| Extension: per student, replaces the deadline for that student only | `AssignmentService::grantExtension`, `Assignment::effectiveDeadline` |
| Resubmission: only while status is SUBMITTED and now <= effective deadline; each upload is a new version | `Submission::resubmit`, `SubmissionService::submit` |
| Life cycle SUBMITTED/LATE -> MARKED -> MODERATED -> RELEASED (+ RETURNED, reopen) | `domain/SubmissionStateMachine` (one transition table) |
| Moderation: HoD of the same department only; adjusted mark needs a reason of 10+ characters | `ModerationService` |
| Students see marks and feedback only when RELEASED | `SubmissionService::myResults` |
| Only HoDs release marks; only administrators manage users/courses | `User::permissions()` per role + `AccessPolicy` |
| Every mark change is audited (who, when, old, new, why); log is append-only (DB triggers) | `MarkingService`, `ModerationService`, `sql/schema.sql` |

## 4. Architecture (matches the Software Design Document)

```
presentation  -> application -> domain <- infrastructure
 (menus)         (services)     (rules,     (SQLite, Argon2id,
                                 ports)      file store, clock)
```
The dependency rule is enforced by CMake: `lms_domain` links nothing, `lms_application` links only
the domain, `lms_infrastructure` links the domain plus SQLite/libsodium. `src/main.cpp` is the only
place concrete classes are wired together. See `docs/DESIGN_FIDELITY.md` for the design-to-code map.

Patterns: **Strategy** (`ILatePenaltyPolicy`), **Repository** (`I*Repository` / `Sqlite*Repository`),
**Observer** (`SubmissionEventPublisher`, `NotificationObserver`), **Factory** (`UserFactory`).
Also: table-driven state machine, RAII `Transaction` (unit of work).

## 5. Quality evidence

| Item | Result |
|---|---|
| Tests | 138 automated (GoogleTest): unit, integration, security, NFR, UI scripts |
| Line coverage, business-logic layer (`domain` + `application`) | **98.9 %** (927/937); `scripts/coverage.sh` |
| cppcheck / clang-tidy | 0 findings (`scripts/static-analysis.sh`) |
| Code size | 4,515 lines in 62 source files; 2,029 lines of tests |
| Complexity | average cyclomatic complexity 2.4; maximum 13 (a table printer) |

Details in `docs/QUALITY.md`. Security justification (Argon2id etc.) in `docs/SECURITY.md`.
Requirements traceability in `docs/TRACEABILITY.md`. Coding standard in `CODING_STANDARD.md`.

## 6. Repository layout

```
src/domain/          entities, rules, state machine, penalty strategy, ports (no I/O)
src/application/     use-case services, observer, session
src/infrastructure/  SQLite wrapper + repositories, Argon2id hasher, file store, clock
src/presentation/    console, role menus, input validation
src/main.cpp         composition root
sql/schema.sql       the database schema (embedded into the binary at build time)
tests/               GoogleTest suite
docs/                design fidelity, traceability, security, quality
scripts/             coverage.sh, static-analysis.sh
.github/workflows/   CI: build, test, static analysis, coverage
```

## 7. Troubleshooting

* *"The system could not start: Cannot open database"* - the folder of `--db` must exist and be writable.
* *Database has schema version N* - the file was created by a different program version.
* Technical error details are written to `lms-error.log`; users only see friendly messages.
* To start again from scratch, delete `lms.db` and the `uploads/` folder.
