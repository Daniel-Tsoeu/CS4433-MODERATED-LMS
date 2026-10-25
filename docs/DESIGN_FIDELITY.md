# Design fidelity: Software Design Document -> code

Marks are given for "fidelity to the design". This table maps each design element to code, then lists every difference
and why, so the viva can start from here.

## 1. Architecture (design section 1)
| Design | Code |
|---|---|
| Presentation layer: ConsoleApp, Admin/Lecturer/HoD/Student menus, InputReader | `presentation/ConsoleApp`, `RoleMenu` + `AdminMenu`, `LecturerMenu`, `HodMenu`, `StudentMenu`, `Console` (the "InputReader") |
| Application layer: Auth, Admin, Assignment, Submission, Marking, Moderation, Notification services | `application/*Service` (same seven names) |
| Domain: entities, AccessPolicy, ILatePenaltyPolicy + implementations, event publisher, ports, UserFactory | `domain/*` |
| Infrastructure: Sqlite repositories, SqliteDatabase, Argon2id hasher, SystemClock, FileStore | `infrastructure/*` |
| Domain owns repository interfaces; infrastructure implements them | `domain/Repositories.hpp` / `infrastructure/SqliteRepositories.*`; enforced by CMake link graph |
| `SqliteDatabase` created once in `main`, passed in (no Singleton) | `src/main.cpp` |

## 2. Class diagram
| Design | Code |
|---|---|
| Abstract `User`; `Student`, `Lecturer`, `HeadOfDepartment`, `Administrator`; HoD does not inherit Lecturer | `domain/User.hpp`; `role()`, `permissions()` are pure virtual |
| `UserFactory::create(role, data)` returns `unique_ptr<User>` | `domain/UserFactory` |
| `Course`, `Assignment` (effective deadline), `Extension`, `Submission`, `SubmissionVersion`, `AuditEntry`, `Notification`, `Department` | `domain/Entities.hpp`, `domain/Submission.hpp` |
| `ILatePenaltyPolicy`, `TenPercentPerDayPolicy`, `NoPenaltyPolicy` | `domain/LatePenaltyPolicy` |
| `SubmissionStateMachine` (table-driven, rejected State pattern) | `domain/SubmissionStateMachine` |
| `ISubmissionObserver`, `SubmissionEventPublisher`, `NotificationObserver`; audit log deliberately *not* an observer | `domain/SubmissionEvents.hpp`, `application/NotificationObserver`; audit rows written in the same `Transaction` |
| `IClock`, `IPasswordHasher`, `IFileStore`, `IUnitOfWork`, `AccessPolicy` | `domain/Ports.hpp`, `domain/AccessPolicy` |
| `SubmissionService`, `MarkingService`, `ModerationService` and their methods (`listForModeration`, `approve`, `returnForRemark`, `release`) | same names in `application/` |

## 3. Behavioural models
| Design | Code / test |
|---|---|
| State machine: 11 transitions | the table in `SubmissionStateMachine.cpp` (9 from-state rows + 2 initial states); `StateMachine.*` tests |
| Sequence "Submit assignment" incl. late, resubmission, closed resubmission, rollback + delete file on `PersistenceError` | `SubmissionService::submit`; `SubmissionFlow.*` (incl. `DatabaseFailureRollsBack...`) |
| Sequence "Moderate and release marks": reason >= 10 characters for an adjustment, other-department `AuthorizationError`, release loop in one transaction, notification after commit | `ModerationService`; `Assessment.*` |
| Activity diagram swimlanes | menus: `LecturerMenu` (mark), `HodMenu` (approve/release), `StudentMenu` (submit/results); system steps are the services |

## 4. Database
`sql/schema.sql` is the schema from design section 3 iii **verbatim** (tables, CHECKs, UNIQUEs, indexes, the 2 audit triggers
and 2 HoD triggers); CMake embeds it so the file and the program cannot drift. Class-table inheritance for users,
`currentVersion` derived from `MAX(version_no)`, `TimePoint` stored as epoch seconds, one audit row per changed field: all as designed.
`PRAGMA user_version = 1` was added to version the schema.

## 5. Differences and why
| # | Difference | Reason |
|---|---|---|
| 1 | Added a `Session` class holding the signed-in `User` | The sequence diagrams pass a `session`; it keeps role checks polymorphic. |
| 2 | Added `IExtensionRepository` and `ICourseRepository` also covers enrolments and course-lecturer links | Link records are tables only; no separate classes needed (mapping table iv, "link records"). |
| 3 | Return-for-remark **reason is delivered in the lecturer's notification**, not stored in a column | The schema has no column for it, and the design says "reason, notify lecturer". Changing the schema would diverge from the approved ERD. |
| 4 | **Reopen** always writes a `FINAL_MARK` audit row (old = new if no adjustment) | State diagram says `reopen [HoD], reason, audit`; `mark_audit.new_value` is NOT NULL, so an unchanged mark is recorded as old = new. |
| 5 | Written feedback is **required when marking** | Student representative asked for written feedback. UC-03 EF1 (missing feedback blocks approval) is kept as defence in depth (`Submission::approve`, test `MissingFeedbackBlocksApproval`). |
| 6 | Service tests run against **in-memory SQLite** (`:memory:`) instead of hand-written fake repositories | Gives real integration coverage of SQL, constraints and triggers; pure domain rules are still tested with no database. |
| 7 | An extension cannot be granted once the student's work is LATE or later | Design is silent. Otherwise a granted extension would contradict an already-applied penalty. Relevant for CR1 (class-wide extensions). |
| 8 | Lockout (5 failures / 5 min), file whitelist and 20 MB limit | SRS exception flows needed concrete numbers; documented in `SECURITY.md`. |
| 9 | Menus are flat per role (one list of actions), at most 3 levels deep | Principal: "teachers are not IT experts, the system must be simple". |

## 6. Issues found in the design document itself (fix before submission)
1. Sections "Behavioral models" and "Database design" are both numbered 3, and the mapping table cites a non-existent "section 5.2".
2. The ERD omits columns that appear in the data dictionary (e.g. `email`, `full_name`, `reason` on extensions); add one note saying the ERD is abridged.
3. The design text says `submitted_at` is the time of the *first* submission; the implementation follows that, and later versions use `submission_versions.uploaded_at`.
