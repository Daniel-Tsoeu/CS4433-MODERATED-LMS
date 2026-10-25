#include "infrastructure/SqliteRepositories.hpp"

#include "domain/Errors.hpp"
#include "domain/UserFactory.hpp"

namespace lms::infrastructure {

using namespace lms::domain;

// ---------------------------------------------------------------- users
namespace {
template <typename T>
const T& requireProfile(const std::optional<T>& v, const char* what) {
  if (!v.has_value()) throw PersistenceError(std::string("Missing profile data: ") + what);
  return *v;
}

const char* kUserSelect =
    "SELECT u.id,u.username,u.full_name,u.email,u.password_hash,u.role,u.is_active,"
    "u.failed_attempts,u.locked_until,u.created_at,s.student_number,"
    "COALESCE(l.department_id,h.department_id) "
    "FROM users u LEFT JOIN students s ON s.user_id=u.id "
    "LEFT JOIN lecturers l ON l.user_id=u.id "
    "LEFT JOIN heads_of_department h ON h.user_id=u.id ";

std::unique_ptr<User> readUser(const Statement& st) {
  UserData d;
  d.id = st.intAt(0);
  d.username = st.textAt(1);
  d.fullName = st.textAt(2);
  d.email = st.isNull(3) ? "" : st.textAt(3);
  d.passwordHash = st.textAt(4);
  d.active = st.intAt(6) != 0;
  d.failedAttempts = static_cast<int>(st.intAt(7));
  d.lockedUntil = st.optIntAt(8);
  d.createdAt = st.intAt(9);
  if (!st.isNull(10)) d.studentNumber = st.textAt(10);
  d.departmentId = st.optIntAt(11);
  return UserFactory::create(roleFromDb(st.textAt(5)), std::move(d));
}
}  // namespace

std::size_t SqliteUserRepository::count() const {
  auto st = db_.prepare("SELECT COUNT(*) FROM users");
  st.step();
  return static_cast<std::size_t>(st.intAt(0));
}

Id SqliteUserRepository::insert(const User& u) {
  auto ins = db_.prepare(
      "INSERT INTO users(username,full_name,email,password_hash,role,is_active,"
      "failed_attempts,locked_until,created_at) VALUES(?,?,?,?,?,?,?,?,?)");
  ins.bindText(1, u.username()).bindText(2, u.fullName());
  if (u.email().empty())
    ins.bindNull(3);
  else
    ins.bindText(3, u.email());
  ins.bindText(4, u.passwordHash())
      .bindText(5, toDb(u.role()))
      .bindInt(6, u.isActive() ? 1 : 0)
      .bindInt(7, u.failedAttempts())
      .bindOptInt(8, u.lockedUntil())
      .bindInt(9, u.createdAt())
      .run();
  const Id id = db_.lastInsertId();
  switch (u.role()) {
    case Role::Student:
      db_.prepare("INSERT INTO students(user_id,student_number) VALUES(?,?)")
          .bindInt(1, id)
          .bindText(2, requireProfile(u.studentNumber(), "student number"))
          .run();
      break;
    case Role::Lecturer:
      db_.prepare("INSERT INTO lecturers(user_id,department_id) VALUES(?,?)")
          .bindInt(1, id)
          .bindInt(2, requireProfile(u.departmentId(), "department"))
          .run();
      break;
    case Role::Hod:
      db_.prepare("INSERT INTO heads_of_department(user_id,department_id) VALUES(?,?)")
          .bindInt(1, id)
          .bindInt(2, requireProfile(u.departmentId(), "department"))
          .run();
      break;
    case Role::Admin:
      break;
  }
  return id;
}

void SqliteUserRepository::update(const User& u) {
  auto st = db_.prepare(
      "UPDATE users SET full_name=?,email=?,password_hash=?,is_active=?,"
      "failed_attempts=?,locked_until=? WHERE id=?");
  st.bindText(1, u.fullName());
  if (u.email().empty())
    st.bindNull(2);
  else
    st.bindText(2, u.email());
  st.bindText(3, u.passwordHash())
      .bindInt(4, u.isActive() ? 1 : 0)
      .bindInt(5, u.failedAttempts())
      .bindOptInt(6, u.lockedUntil())
      .bindInt(7, u.id())
      .run();
  if (db_.changes() != 1) throw NotFoundError("User not found.");
}

std::unique_ptr<User> SqliteUserRepository::findById(Id id) const {
  auto st = db_.prepare(std::string(kUserSelect) + "WHERE u.id=?");
  st.bindInt(1, id);
  return st.step() ? readUser(st) : nullptr;
}
std::unique_ptr<User> SqliteUserRepository::findByUsername(const std::string& name) const {
  auto st = db_.prepare(std::string(kUserSelect) + "WHERE u.username=? COLLATE NOCASE");
  st.bindText(1, name);
  return st.step() ? readUser(st) : nullptr;
}
std::unique_ptr<User> SqliteUserRepository::findHeadOfDepartment(Id departmentId) const {
  auto st = db_.prepare(std::string(kUserSelect) + "WHERE h.department_id=?");
  st.bindInt(1, departmentId);
  return st.step() ? readUser(st) : nullptr;
}
std::vector<std::unique_ptr<User>> SqliteUserRepository::listAll() const {
  auto st = db_.prepare(std::string(kUserSelect) + "ORDER BY u.id");
  std::vector<std::unique_ptr<User>> out;
  while (st.step()) out.push_back(readUser(st));
  return out;
}

// ---------------------------------------------------------- departments
Id SqliteDepartmentRepository::insert(const Department& d) {
  db_.prepare("INSERT INTO departments(code,name) VALUES(?,?)").bindText(1, d.code).bindText(2, d.name).run();
  return db_.lastInsertId();
}
void SqliteDepartmentRepository::update(const Department& d) {
  db_.prepare("UPDATE departments SET code=?,name=? WHERE id=?")
      .bindText(1, d.code)
      .bindText(2, d.name)
      .bindInt(3, d.id)
      .run();
  if (db_.changes() != 1) throw NotFoundError("Department not found.");
}
std::optional<Department> SqliteDepartmentRepository::findById(Id id) const {
  auto st = db_.prepare("SELECT id,code,name FROM departments WHERE id=?");
  st.bindInt(1, id);
  if (!st.step()) return std::nullopt;
  return Department{st.intAt(0), st.textAt(1), st.textAt(2)};
}
std::vector<Department> SqliteDepartmentRepository::listAll() const {
  auto st = db_.prepare("SELECT id,code,name FROM departments ORDER BY code");
  std::vector<Department> out;
  while (st.step()) out.push_back({st.intAt(0), st.textAt(1), st.textAt(2)});
  return out;
}

// -------------------------------------------------------------- courses
namespace {
const char* kCourseSelect = "SELECT c.id,c.code,c.title,c.department_id,c.academic_year FROM courses c ";
Course readCourse(const Statement& st) {
  return Course{st.intAt(0), st.textAt(1), st.textAt(2), st.intAt(3), static_cast<int>(st.intAt(4))};
}
}  // namespace

Id SqliteCourseRepository::insert(const Course& c) {
  db_.prepare("INSERT INTO courses(code,title,department_id,academic_year) VALUES(?,?,?,?)")
      .bindText(1, c.code)
      .bindText(2, c.title)
      .bindInt(3, c.departmentId)
      .bindInt(4, c.academicYear)
      .run();
  return db_.lastInsertId();
}
std::optional<Course> SqliteCourseRepository::findById(Id id) const {
  auto st = db_.prepare(std::string(kCourseSelect) + "WHERE c.id=?");
  st.bindInt(1, id);
  return st.step() ? std::optional<Course>(readCourse(st)) : std::nullopt;
}
std::vector<Course> SqliteCourseRepository::listAll() const {
  auto st = db_.prepare(std::string(kCourseSelect) + "ORDER BY c.academic_year DESC, c.code");
  std::vector<Course> out;
  while (st.step()) out.push_back(readCourse(st));
  return out;
}
std::vector<Course> SqliteCourseRepository::listForLecturer(Id lecturerId) const {
  auto st = db_.prepare(std::string(kCourseSelect) +
                        "JOIN course_lecturers cl ON cl.course_id=c.id WHERE cl.lecturer_id=? "
                        "ORDER BY c.academic_year DESC, c.code");
  st.bindInt(1, lecturerId);
  std::vector<Course> out;
  while (st.step()) out.push_back(readCourse(st));
  return out;
}
std::vector<Course> SqliteCourseRepository::listForStudent(Id studentId) const {
  auto st = db_.prepare(std::string(kCourseSelect) +
                        "JOIN enrolments e ON e.course_id=c.id WHERE e.student_id=? "
                        "ORDER BY c.academic_year DESC, c.code");
  st.bindInt(1, studentId);
  std::vector<Course> out;
  while (st.step()) out.push_back(readCourse(st));
  return out;
}
void SqliteCourseRepository::assignLecturer(Id courseId, Id lecturerId) {
  db_.prepare("INSERT INTO course_lecturers(course_id,lecturer_id) VALUES(?,?)")
      .bindInt(1, courseId)
      .bindInt(2, lecturerId)
      .run();
}
bool SqliteCourseRepository::isLecturerAssigned(Id courseId, Id lecturerId) const {
  auto st = db_.prepare("SELECT 1 FROM course_lecturers WHERE course_id=? AND lecturer_id=?");
  st.bindInt(1, courseId).bindInt(2, lecturerId);
  return st.step();
}
std::vector<Id> SqliteCourseRepository::lecturerIds(Id courseId) const {
  auto st = db_.prepare("SELECT lecturer_id FROM course_lecturers WHERE course_id=? ORDER BY lecturer_id");
  st.bindInt(1, courseId);
  std::vector<Id> out;
  while (st.step()) out.push_back(st.intAt(0));
  return out;
}
void SqliteCourseRepository::enrol(Id courseId, Id studentId, TimePoint at) {
  db_.prepare("INSERT INTO enrolments(course_id,student_id,enrolled_at) VALUES(?,?,?)")
      .bindInt(1, courseId)
      .bindInt(2, studentId)
      .bindInt(3, at)
      .run();
}
bool SqliteCourseRepository::isEnrolled(Id courseId, Id studentId) const {
  auto st = db_.prepare("SELECT 1 FROM enrolments WHERE course_id=? AND student_id=?");
  st.bindInt(1, courseId).bindInt(2, studentId);
  return st.step();
}

// ---------------------------------------------------------- assignments
namespace {
const char* kAssignmentSelect =
    "SELECT id,course_id,title,description,deadline,max_mark,created_by,created_at FROM assignments ";
Assignment readAssignment(const Statement& st) {
  Assignment a;
  a.id = st.intAt(0);
  a.courseId = st.intAt(1);
  a.title = st.textAt(2);
  a.description = st.isNull(3) ? "" : st.textAt(3);
  a.deadline = st.intAt(4);
  a.maxMark = st.doubleAt(5);
  a.createdBy = st.intAt(6);
  a.createdAt = st.intAt(7);
  return a;
}
}  // namespace

Id SqliteAssignmentRepository::insert(const Assignment& a) {
  db_.prepare(
         "INSERT INTO assignments(course_id,title,description,deadline,max_mark,created_by,"
         "created_at) VALUES(?,?,?,?,?,?,?)")
      .bindInt(1, a.courseId)
      .bindText(2, a.title)
      .bindText(3, a.description)
      .bindInt(4, a.deadline)
      .bindDouble(5, a.maxMark)
      .bindInt(6, a.createdBy)
      .bindInt(7, a.createdAt)
      .run();
  return db_.lastInsertId();
}
std::optional<Assignment> SqliteAssignmentRepository::findById(Id id) const {
  auto st = db_.prepare(std::string(kAssignmentSelect) + "WHERE id=?");
  st.bindInt(1, id);
  return st.step() ? std::optional<Assignment>(readAssignment(st)) : std::nullopt;
}
std::vector<Assignment> SqliteAssignmentRepository::listByCourse(Id courseId) const {
  auto st = db_.prepare(std::string(kAssignmentSelect) + "WHERE course_id=? ORDER BY deadline, id");
  st.bindInt(1, courseId);
  std::vector<Assignment> out;
  while (st.step()) out.push_back(readAssignment(st));
  return out;
}

// ----------------------------------------------------------- extensions
void SqliteExtensionRepository::upsert(const Extension& e) {
  db_.prepare(
         "INSERT INTO extensions(assignment_id,student_id,new_deadline,reason,granted_by,"
         "granted_at) VALUES(?,?,?,?,?,?) "
         "ON CONFLICT(assignment_id,student_id) DO UPDATE SET new_deadline=excluded.new_deadline,"
         "reason=excluded.reason,granted_by=excluded.granted_by,granted_at=excluded.granted_at")
      .bindInt(1, e.assignmentId)
      .bindInt(2, e.studentId)
      .bindInt(3, e.newDeadline)
      .bindText(4, e.reason)
      .bindInt(5, e.grantedBy)
      .bindInt(6, e.grantedAt)
      .run();
}
std::optional<Extension> SqliteExtensionRepository::find(Id assignmentId, Id studentId) const {
  auto st = db_.prepare(
      "SELECT id,assignment_id,student_id,new_deadline,reason,granted_by,granted_at "
      "FROM extensions WHERE assignment_id=? AND student_id=?");
  st.bindInt(1, assignmentId).bindInt(2, studentId);
  if (!st.step()) return std::nullopt;
  return Extension{st.intAt(0),  st.intAt(1), st.intAt(2), st.intAt(3),
                   st.textAt(4), st.intAt(5), st.intAt(6)};
}

// ---------------------------------------------------------- submissions
namespace {
const char* kSubmissionCols =
    "s.id,s.assignment_id,s.student_id,s.status,s.submitted_at,s.days_late,s.penalty_percent,"
    "s.raw_mark,s.final_mark,s.feedback,s.updated_at";

Submission readSubmission(const Statement& st) {
  return Submission(st.intAt(0), st.intAt(1), st.intAt(2), statusFromDb(st.textAt(3)), st.intAt(4),
                    static_cast<int>(st.intAt(5)), st.doubleAt(6), st.optDoubleAt(7), st.optDoubleAt(8),
                    st.isNull(9) ? "" : st.textAt(9), st.intAt(10));
}
std::string summarySelect() {
  return std::string("SELECT ") + kSubmissionCols +
         ",u.full_name,st.student_number,a.title,a.course_id,c.code,c.department_id,a.max_mark "
         "FROM submissions s JOIN assignments a ON a.id=s.assignment_id "
         "JOIN courses c ON c.id=a.course_id JOIN users u ON u.id=s.student_id "
         "JOIN students st ON st.user_id=s.student_id ";
}
SubmissionSummary readSummary(const Statement& st) {
  return SubmissionSummary{readSubmission(st), st.textAt(11), st.textAt(12), st.textAt(13),
                           st.intAt(14),       st.textAt(15), st.intAt(16),  st.doubleAt(17)};
}
}  // namespace

Id SqliteSubmissionRepository::insert(const Submission& s) {
  auto st = db_.prepare(
      "INSERT INTO submissions(assignment_id,student_id,status,submitted_at,"
      "days_late,penalty_percent,raw_mark,final_mark,feedback,updated_at) "
      "VALUES(?,?,?,?,?,?,?,?,?,?)");
  st.bindInt(1, s.assignmentId())
      .bindInt(2, s.studentId())
      .bindText(3, toDb(s.status()))
      .bindInt(4, s.submittedAt())
      .bindInt(5, s.daysLate())
      .bindDouble(6, s.penaltyPercent())
      .bindOptDouble(7, s.rawMark())
      .bindOptDouble(8, s.finalMark())
      .bindText(9, s.feedback())
      .bindInt(10, s.updatedAt())
      .run();
  return db_.lastInsertId();
}
void SqliteSubmissionRepository::update(const Submission& s) {
  auto st = db_.prepare(
      "UPDATE submissions SET status=?,days_late=?,penalty_percent=?,raw_mark=?,"
      "final_mark=?,feedback=?,updated_at=? WHERE id=?");
  st.bindText(1, toDb(s.status()))
      .bindInt(2, s.daysLate())
      .bindDouble(3, s.penaltyPercent())
      .bindOptDouble(4, s.rawMark())
      .bindOptDouble(5, s.finalMark())
      .bindText(6, s.feedback())
      .bindInt(7, s.updatedAt())
      .bindInt(8, s.id())
      .run();
  if (db_.changes() != 1) throw NotFoundError("Submission not found.");
}
std::optional<Submission> SqliteSubmissionRepository::findByAssignmentAndStudent(Id a, Id stu) const {
  auto st = db_.prepare(std::string("SELECT ") + kSubmissionCols +
                        " FROM submissions s WHERE s.assignment_id=? AND s.student_id=?");
  st.bindInt(1, a).bindInt(2, stu);
  return st.step() ? std::optional<Submission>(readSubmission(st)) : std::nullopt;
}
std::optional<SubmissionSummary> SqliteSubmissionRepository::findSummary(Id id) const {
  auto st = db_.prepare(summarySelect() + "WHERE s.id=?");
  st.bindInt(1, id);
  if (!st.step()) return std::nullopt;
  return readSummary(st);
}
std::vector<SubmissionSummary> SqliteSubmissionRepository::find(const SubmissionFilter& f) const {
  // Only fixed SQL fragments are appended; every value is bound as a parameter.
  std::string sql = summarySelect() + "WHERE 1=1";
  if (f.assignmentId) sql += " AND s.assignment_id=?";
  if (f.courseId) sql += " AND a.course_id=?";
  if (f.departmentId) sql += " AND c.department_id=?";
  if (f.studentId) sql += " AND s.student_id=?";
  if (f.status) sql += " AND s.status=?";
  sql += " ORDER BY st.student_number, s.id";
  auto st = db_.prepare(sql);
  int i = 1;
  if (f.assignmentId) st.bindInt(i++, *f.assignmentId);
  if (f.courseId) st.bindInt(i++, *f.courseId);
  if (f.departmentId) st.bindInt(i++, *f.departmentId);
  if (f.studentId) st.bindInt(i++, *f.studentId);
  if (f.status) st.bindText(i++, toDb(*f.status));
  std::vector<SubmissionSummary> out;
  while (st.step()) out.push_back(readSummary(st));
  return out;
}
void SqliteSubmissionRepository::addVersion(const SubmissionVersion& v) {
  db_.prepare(
         "INSERT INTO submission_versions(submission_id,version_no,file_path,original_name,"
         "size_bytes,uploaded_at) VALUES(?,?,?,?,?,?)")
      .bindInt(1, v.submissionId)
      .bindInt(2, v.versionNo)
      .bindText(3, v.filePath)
      .bindText(4, v.originalName)
      .bindInt(5, v.sizeBytes)
      .bindInt(6, v.uploadedAt)
      .run();
}
std::vector<SubmissionVersion> SqliteSubmissionRepository::versions(Id submissionId) const {
  auto st = db_.prepare(
      "SELECT submission_id,version_no,file_path,original_name,size_bytes,uploaded_at "
      "FROM submission_versions WHERE submission_id=? ORDER BY version_no");
  st.bindInt(1, submissionId);
  std::vector<SubmissionVersion> out;
  while (st.step())
    out.push_back(
        {st.intAt(0), static_cast<int>(st.intAt(1)), st.textAt(2), st.textAt(3), st.intAt(4), st.intAt(5)});
  return out;
}
int SqliteSubmissionRepository::latestVersionNo(Id submissionId) const {
  auto st = db_.prepare("SELECT COALESCE(MAX(version_no),0) FROM submission_versions WHERE submission_id=?");
  st.bindInt(1, submissionId);
  st.step();
  return static_cast<int>(st.intAt(0));
}
std::vector<ReportRow> SqliteSubmissionRepository::report(Id departmentId) const {
  auto st = db_.prepare(
      "SELECT c.code,a.title,SUM(s.status='MODERATED'),SUM(s.status='RELEASED'),AVG(s.final_mark) "
      "FROM submissions s JOIN assignments a ON a.id=s.assignment_id "
      "JOIN courses c ON c.id=a.course_id "
      "WHERE c.department_id=? AND s.status IN ('MODERATED','RELEASED') "
      "GROUP BY a.id ORDER BY c.code,a.title");
  st.bindInt(1, departmentId);
  std::vector<ReportRow> out;
  while (st.step())
    out.push_back({st.textAt(0), st.textAt(1), static_cast<int>(st.intAt(2)), static_cast<int>(st.intAt(3)),
                   st.optDoubleAt(4)});
  return out;
}

// --------------------------------------------------------------- audit
namespace {
AuditEntry readAudit(const Statement& st) {
  AuditEntry e;
  e.id = st.intAt(0);
  e.submissionId = st.intAt(1);
  e.changedBy = st.intAt(2);
  e.changedAt = st.intAt(3);
  e.field = auditFieldFromDb(st.textAt(4));
  e.oldValue = st.optDoubleAt(5);
  e.newValue = st.doubleAt(6);
  e.reason = st.textAt(7);
  e.changedByName = st.textAt(8);
  return e;
}
const char* kAuditSelect =
    "SELECT m.id,m.submission_id,m.changed_by,m.changed_at,m.field,m.old_value,m.new_value,m.reason,"
    "u.username FROM mark_audit m JOIN users u ON u.id=m.changed_by ";
}  // namespace

void SqliteAuditRepository::append(const AuditEntry& e) {
  db_.prepare(
         "INSERT INTO mark_audit(submission_id,changed_by,changed_at,field,old_value,new_value,"
         "reason) VALUES(?,?,?,?,?,?,?)")
      .bindInt(1, e.submissionId)
      .bindInt(2, e.changedBy)
      .bindInt(3, e.changedAt)
      .bindText(4, toDb(e.field))
      .bindOptDouble(5, e.oldValue)
      .bindDouble(6, e.newValue)
      .bindText(7, e.reason)
      .run();
}
std::vector<AuditEntry> SqliteAuditRepository::listForSubmission(Id submissionId) const {
  auto st = db_.prepare(std::string(kAuditSelect) + "WHERE m.submission_id=? ORDER BY m.id");
  st.bindInt(1, submissionId);
  std::vector<AuditEntry> out;
  while (st.step()) out.push_back(readAudit(st));
  return out;
}
std::vector<AuditEntry> SqliteAuditRepository::listRecent(int limit) const {
  auto st = db_.prepare(std::string(kAuditSelect) + "ORDER BY m.id DESC LIMIT ?");
  st.bindInt(1, limit);
  std::vector<AuditEntry> out;
  while (st.step()) out.push_back(readAudit(st));
  return out;
}

// -------------------------------------------------------- notifications
void SqliteNotificationRepository::add(const Notification& n) {
  db_.prepare("INSERT INTO notifications(recipient_id,message,created_at,is_read) VALUES(?,?,?,0)")
      .bindInt(1, n.recipientId)
      .bindText(2, n.message)
      .bindInt(3, n.createdAt)
      .run();
}
std::vector<Notification> SqliteNotificationRepository::listForUser(Id userId, int limit) const {
  auto st = db_.prepare(
      "SELECT id,recipient_id,message,created_at,is_read FROM notifications "
      "WHERE recipient_id=? ORDER BY id DESC LIMIT ?");
  st.bindInt(1, userId).bindInt(2, limit);
  std::vector<Notification> out;
  while (st.step()) out.push_back({st.intAt(0), st.intAt(1), st.textAt(2), st.intAt(3), st.intAt(4) != 0});
  return out;
}
int SqliteNotificationRepository::countUnread(Id userId) const {
  auto st = db_.prepare("SELECT COUNT(*) FROM notifications WHERE recipient_id=? AND is_read=0");
  st.bindInt(1, userId);
  st.step();
  return static_cast<int>(st.intAt(0));
}
void SqliteNotificationRepository::markAllRead(Id userId) {
  db_.prepare("UPDATE notifications SET is_read=1 WHERE recipient_id=? AND is_read=0")
      .bindInt(1, userId)
      .run();
}

// ---------------------------------------------------------- transactions
void SqliteUnitOfWork::begin() {
  if (active_) throw PersistenceError("A transaction is already active.");
  db_.exec("BEGIN IMMEDIATE");  // take the write lock now, so no half-way deadlocks
  active_ = true;
}
void SqliteUnitOfWork::commit() {
  if (!active_) throw PersistenceError("No active transaction to commit.");
  db_.exec("COMMIT");
  active_ = false;
}
void SqliteUnitOfWork::rollback() noexcept {
  if (!active_) return;
  sqlite3_exec(db_.handle(), "ROLLBACK", nullptr, nullptr, nullptr);
  active_ = false;
}

}  // namespace lms::infrastructure
