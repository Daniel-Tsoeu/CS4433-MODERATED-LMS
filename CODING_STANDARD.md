# Coding standard

Based on the C++ Core Guidelines and the Google C++ style, adapted for this project.
Formatting is automated: `clang-format -i` with the repository's `.clang-format` (2 spaces, 110 columns).

## Language and structure
1. **C++17**, no compiler extensions. Warnings: `-Wall -Wextra -Wpedantic -Wshadow`; CI treats them as errors.
2. **Layering (dependency rule).** `domain` includes no other layer and performs no I/O. `application` includes only
   `domain`. `infrastructure` implements domain ports. `presentation` calls only `application`. CMake link
   dependencies enforce this. Only `main.cpp` constructs concrete infrastructure classes.
3. One class (or one tightly related group) per header/source pair. Headers use `#pragma once`.
   Include project headers as `"layer/Name.hpp"`.
4. Business rules live in domain entities and policies. Menus contain no rules; repositories contain no rules.

## Naming
| Item | Style | Example |
|---|---|---|
| Types, enums | `PascalCase` | `SubmissionStateMachine` |
| Interfaces | `I` prefix | `ILatePenaltyPolicy` |
| Functions, methods | `camelCase` | `listForModeration` |
| Variables | `camelCase` | `effectiveDeadline` |
| Data members | `camelCase_` (private), plain for plain structs | `penalty_`, `Assignment::maxMark` |
| Constants | `kPascalCase` | `kSecondsPerDay` |
| Namespaces | `lms::<layer>` | `lms::domain` |

## Safety and robustness
5. **RAII everywhere.** No `new`/`delete`, no raw owning pointers: `std::unique_ptr`, containers, the `Transaction`
   guard, `Statement`/`SqliteDatabase` wrappers that finalise and close handles.
6. **Exceptions for errors.** All user-correctable failures derive from `LmsError` (`ValidationError`,
   `AuthorizationError`, `NotFoundError`, `ConflictError`, `InvalidStateTransition`, `AuthenticationError`,
   `PersistenceError`). Menus catch them, show the message and continue. `PersistenceError` detail goes to the log only.
7. **Validate at the boundary and again in the domain.** The console re-prompts on bad input; services call
   `domain/Validation`; entities refuse illegal state changes; the database has `CHECK`/`UNIQUE`/`FOREIGN KEY` rules.
8. **Const-correctness.** Query methods are `const`; arguments are `const&` unless a copy is needed.
   Services receive dependencies by reference through the constructor (no globals, no singletons).
9. **SQL.** Every value is bound with `Statement::bind*`. SQL text is built only from constant fragments.
   SQL appears only in `infrastructure/Sqlite*.cpp` and `sql/schema.sql`.
10. **Passwords** are never logged, stored or compared directly: only `IPasswordHasher` touches them.
11. Multi-step changes (marks plus audit rows, submission plus file) run inside one `Transaction`.

## Tests, comments, version control
12. Every business rule gets a test that names it; boundaries are tested at, just below and just above the limit.
    Test names read as sentences: `Assessment.AdjustmentNeedsATenCharacterReasonAndIsAudited`.
13. Comments explain *why* (rule, requirement ID, design reference), not what the code already says.
14. Git: short-lived feature branches merged by pull request; commit messages use
    `type(scope): summary` with types `feat`, `fix`, `test`, `docs`, `build`, `refactor`; release tags `vMAJOR.MINOR.PATCH`.
15. Before merging: `scripts/static-analysis.sh` clean, all tests pass, line coverage of `domain`+`application` >= 70 %.
