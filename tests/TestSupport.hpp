#pragma once
// Shared fixture: a complete in-memory system (real SQLite repositories, real
// services, Argon2id at minimum cost, a controllable clock, real file store in
// a temp folder) so integration tests exercise the same code as production.
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "application/AdminService.hpp"
#include "application/AssignmentService.hpp"
#include "application/AuthService.hpp"
#include "application/MarkingService.hpp"
#include "application/ModerationService.hpp"
#include "application/NotificationObserver.hpp"
#include "application/NotificationService.hpp"
#include "application/SubmissionService.hpp"
#include "domain/AccessPolicy.hpp"
#include "domain/Errors.hpp"
#include "infrastructure/Adapters.hpp"
#include "infrastructure/SqliteDatabase.hpp"
#include "infrastructure/SqliteRepositories.hpp"

namespace lms::test {

using namespace lms::domain;
using namespace lms::application;
using namespace lms::infrastructure;

inline constexpr const char* kPassword = "Passw0rd1";
inline constexpr TimePoint kDay = kSecondsPerDay;

struct FakeClock : IClock {
  TimePoint t = 1'800'000'000;
  TimePoint now() const override { return t; }
  void advance(TimePoint seconds) { t += seconds; }
};

struct TempDir {
  std::filesystem::path path;
  TempDir() {
    static int n = 0;
    path = std::filesystem::temp_directory_path() /
           ("lms_test_" + std::to_string(::getpid()) + "_" + std::to_string(n++));
    std::filesystem::create_directories(path);
  }
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};

struct World {
  SqliteDatabase db{":memory:"};
  FakeClock clock;
  TempDir tmp;
  SqliteUnitOfWork uow;
  SqliteUserRepository users;
  SqliteDepartmentRepository departments;
  SqliteCourseRepository courses;
  SqliteAssignmentRepository assignments;
  SqliteExtensionRepository extensions;
  SqliteSubmissionRepository submissions;
  SqliteAuditRepository audit;
  SqliteNotificationRepository notifications;
  SodiumPasswordHasher hasher{SodiumPasswordHasher::Cost::Test};
  FileStore files;
  TenPercentPerDayPolicy penalty;
  AccessPolicy access;
  SubmissionEventPublisher publisher;
  NotificationObserver observer;
  AuthService auth;
  AdminService admin;
  AssignmentService assignmentSvc;
  SubmissionService submissionSvc;
  MarkingService marking;
  ModerationService moderation;
  NotificationService notificationSvc;

  Id dept = 0, course = 0, hodId = 0, lecturerId = 0, studentId = 0;

  World()
      : uow((db.initialiseSchema(), db)),
        users(db),
        departments(db),
        courses(db),
        assignments(db),
        extensions(db),
        submissions(db),
        audit(db),
        notifications(db),
        files(tmp.path / "uploads"),
        access(courses),
        observer(users, courses, notifications, clock),
        auth(users, hasher, clock),
        admin(access, users, departments, courses, audit, hasher, clock, uow),
        assignmentSvc(access, courses, assignments, extensions, submissions, users, clock),
        submissionSvc(access, assignments, courses, extensions, submissions, penalty, files, clock, uow,
                      publisher),
        marking(access, assignments, submissions, audit, clock, uow, publisher),
        moderation(access, submissions, audit, clock, uow, publisher),
        notificationSvc(access, notifications) {
    publisher.subscribe(observer);
  }

  Session login(const std::string& user, const std::string& pw = kPassword) { return auth.login(user, pw); }

  static CreateUserRequest request(Role role, const std::string& username, const std::string& number = "",
                                   std::optional<Id> dept = std::nullopt) {
    CreateUserRequest r;
    r.role = role;
    r.username = username;
    r.fullName = "Name " + username;
    r.password = kPassword;
    r.studentNumber = number;
    r.departmentId = dept;
    return r;
  }

  /// admin1, department MCS, hod1, lect1, stud1 (S0001), course CS4433 with
  /// lect1 assigned and stud1 enrolled.
  void seed() {
    auth.createInitialAdministrator("admin1", "Admin One", kPassword);
    const Session a = login("admin1");
    dept = admin.createDepartment(a, "MCS", "Mathematics and Computer Science");
    hodId = admin.createUser(a, request(Role::Hod, "hod1", "", dept));
    lecturerId = admin.createUser(a, request(Role::Lecturer, "lect1", "", dept));
    studentId = admin.createUser(a, request(Role::Student, "stud1", "S0001"));
    course = admin.createCourse(a, "CS4433", "Software Engineering", dept, 2026);
    admin.assignLecturer(a, course, lecturerId);
    admin.enrolStudent(a, course, studentId);
  }

  Id addStudent(const std::string& username, const std::string& number, bool enrol = true) {
    const Session a = login("admin1");
    const Id id = admin.createUser(a, request(Role::Student, username, number));
    if (enrol) admin.enrolStudent(a, course, id);
    return id;
  }

  Id newAssignment(TimePoint deadlineFromNow = 3 * kDay, double maxMark = 50) {
    const Session l = login("lect1");
    return assignmentSvc.createAssignment(l, course, "Assignment", "desc", clock.now() + deadlineFromNow,
                                          maxMark);
  }

  std::string makeFile(const std::string& name = "work.pdf", const std::string& content = "data") {
    const auto p = tmp.path / name;
    std::ofstream(p) << content;
    return p.string();
  }

  /// Student submits on time and the lecturer marks it; returns submission id.
  Id submitAndMark(Id assignment, double raw = 42, const std::string& student = "stud1") {
    const auto r = submissionSvc.submit(login(student), assignment, makeFile());
    marking.mark(login("lect1"), r.submissionId, raw, "Good work", "");
    return r.submissionId;
  }
};

}  // namespace lms::test
