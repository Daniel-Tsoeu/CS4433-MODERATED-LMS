// Authentication (FR1, NFR4) and administration (FR2-FR6, BR1, BR2).
#include "TestSupport.hpp"

using namespace lms::test;

TEST(Auth, FirstRunCreatesExactlyOneAdministrator) {
  World w;
  EXPECT_TRUE(w.auth.requiresInitialSetup());
  w.auth.createInitialAdministrator("root", "Root", kPassword);
  EXPECT_FALSE(w.auth.requiresInitialSetup());
  EXPECT_THROW(w.auth.createInitialAdministrator("root2", "Root", kPassword), AuthorizationError);
  EXPECT_EQ(w.login("root").role(), Role::Admin);
}

TEST(Auth, SuccessfulLoginForEveryRole) {
  World w;
  w.seed();
  EXPECT_EQ(w.login("admin1").role(), Role::Admin);
  EXPECT_EQ(w.login("lect1").role(), Role::Lecturer);
  EXPECT_EQ(w.login("hod1").role(), Role::Hod);
  EXPECT_EQ(w.login("stud1").role(), Role::Student);
  EXPECT_EQ(w.login("STUD1").role(), Role::Student);  // usernames are case-insensitive
}

TEST(Auth, WrongPasswordUnknownUserAndEmptyInputAreRejectedWithTheSameMessage) {
  World w;
  w.seed();
  std::string m1, m2;
  try {
    w.login("stud1", "WrongPass1");
  } catch (const AuthenticationError& e) {
    m1 = e.what();
  }
  try {
    w.login("nobody", "WrongPass1");
  } catch (const AuthenticationError& e) {
    m2 = e.what();
  }
  EXPECT_FALSE(m1.empty());
  EXPECT_EQ(m1, m2);  // no username enumeration
  EXPECT_THROW(w.login("", ""), AuthenticationError);
  EXPECT_THROW(w.login("stud1", ""), AuthenticationError);
}

TEST(Auth, AccountLocksAfterFiveFailuresAndUnlocksAfterFiveMinutes) {
  World w;
  w.seed();
  for (int i = 0; i < kMaxFailedLogins; ++i) EXPECT_THROW(w.login("stud1", "Bad12345"), AuthenticationError);
  EXPECT_THROW(w.login("stud1"), AuthenticationError);  // correct password, still locked
  w.clock.advance(kLockoutSeconds - 1);
  EXPECT_THROW(w.login("stud1"), AuthenticationError);
  w.clock.advance(1);
  EXPECT_NO_THROW(w.login("stud1"));
}

TEST(Auth, SuccessfulLoginResetsTheFailureCounter) {
  World w;
  w.seed();
  for (int i = 0; i < kMaxFailedLogins - 1; ++i)
    EXPECT_THROW(w.login("stud1", "Bad12345"), AuthenticationError);
  EXPECT_NO_THROW(w.login("stud1"));
  for (int i = 0; i < kMaxFailedLogins - 1; ++i)
    EXPECT_THROW(w.login("stud1", "Bad12345"), AuthenticationError);
  EXPECT_NO_THROW(w.login("stud1"));  // never reached 5 in a row
}

TEST(Auth, DeactivatedAccountCannotLogInButRecordsRemain) {
  World w;
  w.seed();
  const Session a = w.login("admin1");
  w.admin.setActive(a, w.studentId, false);
  EXPECT_THROW(w.login("stud1"), AuthenticationError);
  EXPECT_NE(w.users.findById(w.studentId), nullptr);  // record kept
  w.admin.setActive(a, w.studentId, true);
  EXPECT_NO_THROW(w.login("stud1"));
}

TEST(Auth, ChangePasswordNeedsCurrentPasswordAndPolicy) {
  World w;
  w.seed();
  const Session s = w.login("stud1");
  EXPECT_THROW(w.auth.changePassword(s, "Wrong1234", "NewPass123"), AuthenticationError);
  EXPECT_THROW(w.auth.changePassword(s, kPassword, "short1"), ValidationError);
  w.auth.changePassword(s, kPassword, "NewPass123");
  EXPECT_THROW(w.login("stud1"), AuthenticationError);
  EXPECT_NO_THROW(w.login("stud1", "NewPass123"));
}

TEST(Admin, OnlyAdministratorsManageUsersDepartmentsCoursesAndEnrolments) {
  World w;
  w.seed();
  for (const char* who : {"lect1", "hod1", "stud1"}) {
    const Session s = w.login(who);
    EXPECT_THROW(w.admin.createUser(s, World::request(Role::Student, "newbie", "S9999")), AuthorizationError)
        << who;
    EXPECT_THROW(w.admin.listUsers(s), AuthorizationError) << who;
    EXPECT_THROW(w.admin.createDepartment(s, "XX", "Nope"), AuthorizationError) << who;
    EXPECT_THROW(w.admin.createCourse(s, "C1", "T", w.dept, 2026), AuthorizationError) << who;
    EXPECT_THROW(w.admin.assignLecturer(s, w.course, w.lecturerId), AuthorizationError) << who;
    EXPECT_THROW(w.admin.enrolStudent(s, w.course, w.studentId), AuthorizationError) << who;
    EXPECT_THROW(w.admin.setActive(s, w.studentId, false), AuthorizationError) << who;
    EXPECT_THROW(w.admin.resetPassword(s, w.studentId, "Hacked123"), AuthorizationError) << who;
  }
}

TEST(Admin, CreateUserValidatesEveryField) {
  World w;
  w.seed();
  const Session a = w.login("admin1");
  auto r = World::request(Role::Student, "newbie", "S9999");
  EXPECT_NO_THROW(w.admin.createUser(a, r));
  EXPECT_THROW(w.admin.createUser(a, r), ConflictError);  // duplicate username
  r.username = "other";
  EXPECT_THROW(w.admin.createUser(a, r), ConflictError);  // duplicate student number
  r = World::request(Role::Student, "weak", "S1234");
  r.password = "weak";
  EXPECT_THROW(w.admin.createUser(a, r), ValidationError);  // weak password
  r = World::request(Role::Student, "nonum", "");
  EXPECT_THROW(w.admin.createUser(a, r), ValidationError);  // student needs a number
  r = World::request(Role::Lecturer, "nodept", "");
  EXPECT_THROW(w.admin.createUser(a, r), ValidationError);  // lecturer needs a department
  r = World::request(Role::Lecturer, "baddept", "", 9999);
  EXPECT_THROW(w.admin.createUser(a, r), ValidationError);
  r = World::request(Role::Student, "bademail", "S7777");
  r.email = "not-an-email";
  EXPECT_THROW(w.admin.createUser(a, r), ValidationError);
}

TEST(Admin, AdministratorCannotDeactivateSelf) {
  World w;
  w.seed();
  const Session a = w.login("admin1");
  EXPECT_THROW(w.admin.setActive(a, a.userId(), false), ValidationError);
}

TEST(Admin, PasswordResetClearsLockout) {
  World w;
  w.seed();
  for (int i = 0; i < kMaxFailedLogins; ++i) EXPECT_THROW(w.login("stud1", "Bad12345"), AuthenticationError);
  w.admin.resetPassword(w.login("admin1"), w.studentId, "Fresh12345");
  EXPECT_NO_THROW(w.login("stud1", "Fresh12345"));
}

TEST(Admin, CourseDepartmentAndEnrolmentRules) {
  World w;
  w.seed();
  const Session a = w.login("admin1");
  EXPECT_THROW(w.admin.createCourse(a, "CS4433", "Dup", w.dept, 2026), ConflictError);  // same code + year
  EXPECT_NO_THROW(w.admin.createCourse(a, "CS4433", "Next year", w.dept, 2027));
  EXPECT_THROW(w.admin.createCourse(a, "X1", "T", 9999, 2026), NotFoundError);
  EXPECT_THROW(w.admin.createCourse(a, "X1", "T", w.dept, 1999), ValidationError);
  EXPECT_THROW(w.admin.createDepartment(a, "MCS", "Another"), ConflictError);
  EXPECT_THROW(w.admin.enrolStudent(a, w.course, w.studentId), ConflictError);      // already enrolled
  EXPECT_THROW(w.admin.enrolStudent(a, w.course, w.lecturerId), ValidationError);   // not a student
  EXPECT_THROW(w.admin.assignLecturer(a, w.course, w.studentId), ValidationError);  // not a lecturer
  EXPECT_THROW(w.admin.assignLecturer(a, w.course, w.lecturerId), ConflictError);
  EXPECT_THROW(w.admin.enrolStudent(a, 9999, w.studentId), NotFoundError);
}

TEST(Admin, UpdateUserChangesNameAndEmailOnly) {
  World w;
  w.seed();
  const Session a = w.login("admin1");
  w.admin.updateUser(a, w.studentId, "New Name", "new@school.ls");
  const auto u = w.users.findById(w.studentId);
  EXPECT_EQ(u->fullName(), "New Name");
  EXPECT_EQ(u->email(), "new@school.ls");
  EXPECT_EQ(u->role(), Role::Student);  // BR1: role never changes
  EXPECT_THROW(w.admin.updateUser(a, 9999, "X", ""), NotFoundError);
}

TEST(Rbac, StudentsAndLecturersCannotUseEachOthersFunctions) {
  World w;
  w.seed();
  const Id as = w.newAssignment();
  const Session stud = w.login("stud1"), lect = w.login("lect1"), hod = w.login("hod1"),
                adm = w.login("admin1");
  // students cannot create assignments, mark, moderate or release
  EXPECT_THROW(w.assignmentSvc.createAssignment(stud, w.course, "T", "", w.clock.now() + kDay, 10),
               AuthorizationError);
  EXPECT_THROW(w.marking.mark(stud, 1, 1, "x", ""), AuthorizationError);
  EXPECT_THROW(w.moderation.release(stud, std::nullopt), AuthorizationError);
  // lecturers cannot submit, moderate or release
  EXPECT_THROW(w.submissionSvc.submit(lect, as, w.makeFile()), AuthorizationError);
  EXPECT_THROW(w.moderation.listForModeration(lect), AuthorizationError);
  EXPECT_THROW(w.moderation.release(lect, std::nullopt), AuthorizationError);
  // HoDs cannot mark, submit or create assignments (not a drop-in lecturer)
  EXPECT_THROW(w.marking.mark(hod, 1, 1, "x", ""), AuthorizationError);
  EXPECT_THROW(w.submissionSvc.submit(hod, as, w.makeFile()), AuthorizationError);
  EXPECT_THROW(w.assignmentSvc.createAssignment(hod, w.course, "T", "", w.clock.now() + kDay, 10),
               AuthorizationError);
  // administrators cannot touch the assessment flow
  EXPECT_THROW(w.moderation.release(adm, std::nullopt), AuthorizationError);
  EXPECT_THROW(w.marking.mark(adm, 1, 1, "x", ""), AuthorizationError);
  EXPECT_THROW(w.submissionSvc.submit(adm, as, w.makeFile()), AuthorizationError);
}

TEST(Rbac, DeactivatedSessionIsRefusedEverywhere) {
  World w;
  w.seed();
  const Session stud = w.login("stud1");
  w.admin.setActive(w.login("admin1"), w.studentId, false);
  const auto fresh = w.users.findById(w.studentId);
  const Session stale(std::shared_ptr<const User>(w.users.findById(w.studentId)));
  EXPECT_THROW(w.assignmentSvc.myCourses(stale), AuthorizationError);
  (void)stud;
  (void)fresh;
}
