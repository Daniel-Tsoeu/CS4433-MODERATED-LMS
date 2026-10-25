# Requirements traceability matrix (implementation part)

Links each SRS requirement to its use case, design elements, source files and automated tests
(`Suite.TestName`; run one with `./build/tests/lms_tests --gtest_filter='Suite.*'`).
UC-01 Submit assignment, UC-02 Mark submission, UC-03 Moderate and release marks.

## Functional requirements
| ID | Pri | UC | Design elements | Source | Tests |
|---|---|---|---|---|---|
| FR 1 Authenticate | Must | all | `AuthService`, `IPasswordHasher`, table `users` | `application/AuthService.cpp`, `infrastructure/Adapters.cpp` | `Auth.*`, `PasswordSecurity.*`, `ConsoleApp.WrongPassword...` |
| FR 2 Role-based access | Must | all | `User::permissions`, `AccessPolicy` | `domain/User.cpp`, `domain/AccessPolicy.cpp` | `Users.PermissionsDifferByRolePolymorphically`, `Rbac.*`, `Admin.OnlyAdministrators...` |
| FR 3 Manage user accounts | Must | - | `AdminService`, `UserFactory`, `SqliteUserRepository` | `application/AdminService.cpp` | `Admin.*`, `AdminExtra.*`, `Auth.DeactivatedAccount...` |
| FR 4 Departments | Must | - | `AdminService`, table `departments` | `AdminService::createDepartment/updateDepartment` | `AdminExtra.DepartmentsCanBeRenamed...`, `Admin.CourseDepartmentAndEnrolmentRules` |
| FR 5 Courses, assign lecturers | Must | - | `AdminService`, tables `courses`, `course_lecturers` | `AdminService::createCourse/assignLecturer` | `Admin.CourseDepartmentAndEnrolmentRules`, `Constraints.*` |
| FR 6 Enrol students | Must | - | `AdminService`, table `enrolments` | `AdminService::enrolStudent` | `Admin.CourseDepartmentAndEnrolmentRules`, `SubmissionFlow.UnenrolledStudent...` |
| FR 7 Create assignments | Must | - | `AssignmentService`, `Assignment::validateNew` | `application/AssignmentService.cpp`, `domain/Validation.cpp` | `SubmissionFlow.AssignmentCreationRules`, `Validation.AssignmentRules`, `AssignmentExtra.*` |
| FR 8 Individual extensions | Should | UC-01 AF1 | `Extension`, `Assignment::effectiveDeadline`, `IExtensionRepository` | `AssignmentService::grantExtension` | `SubmissionFlow.ExtensionRules`, `...ExtensionMoves...`, `...LateAfterTheExtension...`, `Assignment.EffectiveDeadline...` |
| FR 9 View courses and assignments | Must | UC-01 | `AssignmentService::myCourses/assignmentsForStudent` | `application/AssignmentService.cpp` | `SubmissionFlow.StudentSeesOwnAssignments...`, `AssignmentExtra.*`, `ConsoleApp.StudentCompletes...` |
| FR 10 Submit (before/after deadline) | Must | UC-01 | `SubmissionService::submit`, `IFileStore`, tables `submissions`, `submission_versions` | `application/SubmissionService.cpp`, `infrastructure/Adapters.cpp` | `SubmissionFlow.*` (on time, exactly on deadline, +1 s, invalid file, rollback) |
| FR 11 Resubmit before deadline | Must | UC-01 AF2 | `Submission::resubmit`, versions | `domain/Submission.cpp` | `SubmissionFlow.Resubmission*`, `SubmissionEntity.ResubmitOnlyWhile...` |
| FR 12 Submission status | Must | all | `SubmissionStateMachine`, `Submission` | `domain/SubmissionStateMachine.cpp`, `domain/Submission.cpp` | `StateMachine.*`, `SubmissionEntity.*`, `Types*.*` |
| FR 13 Marks and written feedback | Must | UC-02 | `MarkingService::mark`, `Submission::applyMark` | `application/MarkingService.cpp` | `Assessment.FirstMarking...`, `...MarkBoundariesAreValidated`, `...RemarkRequiresAReason...`, `...OnlyTheCourseLecturerMayMark` |
| FR 14 HoD review and moderate | Must | UC-03 | `ModerationService::listForModeration/approve/returnForRemark` | `application/ModerationService.cpp` | `Assessment.ModerationQueue...`, `...Adjustment...`, `...ReturnForRemark...`, `...HodOfAnotherDepartment...` |
| FR 15 HoD releases | Must | UC-03 | `ModerationService::release/reopen` | `ModerationService.cpp` | `Assessment.ReleaseOnlyAffects...`, `...OnlyHodsMayRelease`, `...Reopen*`, `Performance.ReleasingEightHundred...` |
| FR 16 Students see marks only after release | Must | UC-01 | `SubmissionService::myResults` | `SubmissionService.cpp` | `Assessment.StudentCannotSeeMarks...`, `...StudentSeesMarkAndFeedbackOnlyAfterRelease`, `...OtherStudentsNever...`, `ConsoleApp.FullRolePipeline...` |
| FR 17 Late penalty 10 %/day, 3 days | Must | UC-01 | `ILatePenaltyPolicy` (Strategy), `TenPercentPerDayPolicy` | `domain/LatePenaltyPolicy.cpp` | `DesignTable/PenaltyBoundary.*` (7 boundary cases), `LatePenalty.*`, `SubmissionFlow.ThreeDaysLate...`, `Assessment.LatePenaltyIsApplied...` |
| FR 18 Zero after 3 days | Must | UC-01 EF2 | same + `Submission::zeroedForLateness` | `LatePenaltyPolicy.cpp`, `Submission.cpp` | `PenaltyBoundary` (72 h + 1 s, 10 days), `FinalMark.*`, `SubmissionFlow.ThreeDaysLate...` |
| FR 19 Audit trail | Must | UC-02, UC-03 | `IAuditRepository`, table `mark_audit` + triggers, `Transaction` | `MarkingService.cpp`, `ModerationService.cpp`, `sql/schema.sql` | `Assessment.EveryMarkChangeInTheWholeLifecycle...`, `AuditLog.IsAppendOnly...`, `...AuditTrailVisibleTo...` |
| FR 20 Reports of moderated/released | Could | UC-03 | `ModerationService::report`, `SqliteSubmissionRepository::report` | `ModerationService.cpp` | `Assessment.DepartmentReportCounts...` |

## Non-functional requirements
| ID | Requirement | Mechanism | Evidence |
|---|---|---|---|
| NFR 1 | Login <= 2 s with 800 students stored | Argon2id interactive cost, indexed lookups | `Performance.LoginWithEightHundredStudents...` measured **73 ms** |
| NFR 2 | Search/retrieval <= 2 s | indexes `idx_sub_*`, joined summary query | `Performance.ListingAndModerationQueues...` 800 rows in **4 ms** |
| NFR 3 | 800 students, 50 lecturers, 20 HoDs, 10 admins <= 2 s | same | `Nfr3.FullPopulation...` (31 ms) |
| NFR 4 | Salted cryptographic password hashes | `SodiumPasswordHasher` (Argon2id) | `PasswordSecurity.*`, `docs/SECURITY.md` |
| NFR 5 | Works offline | no network code; embedded SQLite | `ldd build/lms` = libsqlite3, libsodium, libc only; no socket symbols |
| NFR 6 | >= 99 % of 1,000 consecutive operations succeed | transactions, validation, exceptions | `Nfr6.ThousandConsecutiveOperations...` **1000/1000**; `Robustness.RandomInput...` |

## Business rules
| ID | Rule | Enforced in | Tests |
|---|---|---|---|
| BR 1 | One role per user | `users.role` CHECK, `UserFactory`, `updateUser` cannot change role | `Admin.UpdateUserChangesNameAndEmailOnly`, `Constraints.*`, `Users.Factory...` |
| BR 2 | Only administrators manage users/courses | `Permission::Manage*` only in `Administrator` | `Admin.OnlyAdministrators...` |
| BR 3 | Resubmit only before the effective deadline | `Submission::canResubmit` | `SubmissionFlow.ResubmissionAfterTheDeadlineIsRefused...` |
| BR 4 | Marks hidden until released | `SubmissionService::myResults` | `Assessment.StudentCannotSeeMarks...` |
| BR 5, BR 6 | 10 %/day up to 3 days, then zero | `TenPercentPerDayPolicy` | `PenaltyBoundary.*` |
| BR 7 | Log user, date, old, new, reason | `mark_audit` | `Assessment.EveryMarkChange...` |
| BR 8 | Only HoDs release | `Permission::ReleaseMarks` only in `HeadOfDepartment` | `Assessment.OnlyHodsMayRelease`, `Users.Permissions...` |

## Use-case exception flows
| Flow | Behaviour | Test |
|---|---|---|
| UC-01 EF1 invalid file | rejected, nothing stored | `SubmissionFlow.InvalidFilesAreRejected...` |
| UC-01 EF2 more than 3 days late | accepted, zero mark | `SubmissionFlow.ThreeDaysLate...` |
| UC-01 EF3 storage failure | rollback, stored file deleted | `SubmissionFlow.DatabaseFailureRollsBack...` |
| UC-02 invalid mark | `ValidationError`, nothing logged | `Assessment.MarkBoundariesAreValidated` |
| UC-03 EF1 missing feedback | approval blocked | `Assessment.MissingFeedbackBlocksApproval` |
| UC-03 EF2 unmarked work | `InvalidStateTransition` | `Assessment.UnmarkedWorkCannotBeModerated` |
| Other-department HoD | `AuthorizationError`, nothing changes | `Assessment.HodOfAnotherDepartment...` |

*Requirement coverage: every Must/Should/Could functional requirement and all six NFRs have at least one automated test.*
