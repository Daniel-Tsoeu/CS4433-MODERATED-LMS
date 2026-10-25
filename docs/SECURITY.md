# Security design and justification (Phase 4: persistence and security)

## 1. Password storage (NFR 4)
Passwords are **never stored, logged or compared in plain text**. `SodiumPasswordHasher` calls libsodium's
`crypto_pwhash_str_alg(... crypto_pwhash_ALG_ARGON2ID13)` and stores the returned self-describing string, e.g.
`$argon2id$v=19$m=65536,t=2,p=1$<random salt>$<hash>`. Verification uses `crypto_pwhash_str_verify` (constant-time compare).

**Why Argon2id through libsodium** (the brief asks to justify the choice):

| Option | Verdict |
|---|---|
| **Argon2id (libsodium)** | **Chosen.** Winner of the Password Hashing Competition and the first choice in the OWASP Password Storage Cheat Sheet. Memory-hard (64 MiB per guess), so GPU/ASIC cracking is expensive. libsodium generates a unique random salt, encodes salt and parameters in the string (so costs can be raised later without a schema change), is widely audited, and is one small dependency. |
| bcrypt | Not memory-hard; silently truncates passwords at 72 bytes; Argon2id is the stronger modern choice. |
| PBKDF2 | Acceptable but not memory-hard; needs a hand-managed salt and iteration count. |
| scrypt | Good, but Argon2id is the current recommendation and has the better-defined libsodium API. |
| SHA-256/MD5 + salt | **Rejected**: fast hashes can be brute-forced at billions of guesses per second. |
| Own algorithm | **Rejected**: never write your own cryptography. |

Cost: libsodium's *interactive* profile (about 64 MiB, 2 passes), above the OWASP minimum, about 73 ms per login measured on the development PC (limit: 2000 ms; NFR 1, see
`Performance.LoginWithEightHundredStudents...`). The school's old computers will be slower; if a login ever approaches
the limit there, lower the memory cost in `SodiumPasswordHasher` (existing hashes keep working because the cost is stored inside each hash).
Automated tests use the minimum profile (`Cost::Test`) only to keep the suite fast; production wiring in `main.cpp`
uses the default (interactive) cost, and `PasswordSecurity.ProductionCostProfile...` asserts `m=65536,t=2`.

## 2. Authentication hardening
* Wrong user, wrong password, and empty fields give the **same message** (no username enumeration); for unknown users a
  dummy hash is verified so response time does not reveal whether the account exists.
* 5 consecutive failures lock the account for 5 minutes (`User::recordFailedLogin`); success resets the counter;
  an administrator password reset also clears the lock.
* Deactivated accounts cannot log in, and `AccessPolicy::require` re-checks `isActive()` on every operation.
* Password policy (`validation::password`): 8-128 characters, at least one letter and one digit, not equal to the username.
* The first administrator is created interactively on first run: **there is no default password** in the program.

## 3. Authorisation (FR 2, BR 2, BR 8)
Two layers, both enforced in the application services (so no UI can bypass them):
1. **Role permission** - each `User` subclass returns its own permission set (polymorphism); `AccessPolicy::require`
   checks it. A head of department is *not* a lecturer, so cannot mark or create assignments.
2. **Scope** - lecturers must teach the course (`requireTeaches`), students must be enrolled (`requireEnrolled`),
   heads of department must head the submission's department (`requireHeadOf`).
`Rbac.*`, `Admin.OnlyAdministrators...`, `Assessment.OnlyHodsMayRelease` and `...HodOfAnotherDepartmentIsRefused...`
test the matrix, including that a refused action changes nothing.

## 4. Data integrity and confidentiality
* **Parameterised queries only.** Values reach SQL through `Statement::bind*`; the only dynamic SQL
  (`SqliteSubmissionRepository::find`) appends constant fragments and binds every value.
  `ParameterisedQueries.InjectionStrings...` stores `x'); DROP TABLE users; --` as plain text and the tables survive.
* **Database enforces the rules too**: `CHECK`, `UNIQUE`, `FOREIGN KEY` (enabled on every connection), a trigger so only a user
  whose role is HOD can head a department, and triggers that make `mark_audit` **append-only** (`UPDATE`/`DELETE` abort).
* **Atomicity.** A mark plus its audit rows, a submission plus its file version, and a bulk release each run in one
  `BEGIN IMMEDIATE ... COMMIT`; any exception rolls back. If the database fails after the upload was copied, the stored file is deleted.
* **Students cannot see marks early** (BR 4): `myResults` leaves mark/feedback empty unless the status is RELEASED, so even a
  buggy menu cannot leak them.
* **Files.** Uploaded files are copied under generated names (`a<assignment>_s<student>_v<n>.<ext>`); user-supplied names are only
  stored as text, so path traversal is impossible (`FileStore.RejectsPathTraversal...`). Type whitelist and 20 MB limit.
* **File permissions.** A newly created database file is `0600`, the uploads folder `0700`.
* **Errors.** Menus show friendly text; SQL and file-system details go to `lms-error.log` only
  (`ConsoleApp.PersistenceFailureIsHidden...`). `Robustness.RandomInput...` fires 1,500 hostile inputs at the services and
  only domain errors ever escape.

## 5. Privacy (learners' personal data)
Only the data the case study needs is stored (name, username, optional e-mail, student number). Deactivation keeps assessment
records but blocks access. No personal data is sent anywhere: the program has no network code (`ldd` shows only libsqlite3,
libsodium and libc).

## 6. Known limitations (honest list)
* No encryption at rest: protection relies on operating-system accounts and the `0600` file mode. Use full-disk encryption on school PCs.
* The console has no idle-session timeout; a user must log out. (A lock screen policy is a deployment control.)
* Uploaded files are not scanned for malware; they are stored, never executed.
* A local administrator with OS access can replace the database file. The audit log is tamper-evident to the application,
  not to someone who controls the machine.
