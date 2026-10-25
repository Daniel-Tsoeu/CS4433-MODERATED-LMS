// Remaining administrator operations and small domain helpers.
#include "TestSupport.hpp"

using namespace lms::test;

TEST(AdminExtra, DepartmentsCanBeRenamedAndListed) {
  World w;
  w.seed();
  const Session a = w.login("admin1");
  const Id phy = w.admin.createDepartment(a, "PHY", "Physics");
  w.admin.updateDepartment(a, phy, "PHY", "Physics and Astronomy");
  EXPECT_EQ(w.departments.findById(phy)->name, "Physics and Astronomy");
  EXPECT_EQ(w.admin.listDepartments(a).size(), 2u);
  EXPECT_THROW(w.admin.updateDepartment(a, phy, "MCS", "Physics"), ConflictError);  // code taken
  EXPECT_THROW(w.admin.updateDepartment(a, 9999, "ZZ", "Nothing"), NotFoundError);
  EXPECT_THROW(w.admin.updateDepartment(a, phy, "P", "Too short code"), ValidationError);
  EXPECT_THROW(w.admin.updateDepartment(w.login("lect1"), phy, "PHY", "Hack"), AuthorizationError);
  EXPECT_THROW(w.admin.listDepartments(w.login("stud1")), AuthorizationError);
}

TEST(AdminExtra, ListsUsersAndCoursesForAdministratorsOnly) {
  World w;
  w.seed();
  const Session a = w.login("admin1");
  EXPECT_EQ(w.admin.listUsers(a).size(), 4u);
  EXPECT_EQ(w.admin.listCourses(a).size(), 1u);
  EXPECT_THROW(w.admin.listCourses(w.login("hod1")), AuthorizationError);
}

TEST(AdminExtra, SetActiveAndResetPasswordRejectUnknownUsersAndWeakPasswords) {
  World w;
  w.seed();
  const Session a = w.login("admin1");
  EXPECT_THROW(w.admin.setActive(a, 9999, false), NotFoundError);
  EXPECT_THROW(w.admin.resetPassword(a, 9999, "Fresh12345"), NotFoundError);
  EXPECT_THROW(w.admin.resetPassword(a, w.studentId, "weak"), ValidationError);
}

TEST(AdminExtra, RecentAuditLimitIsClamped) {
  World w;
  w.seed();
  w.submitAndMark(w.newAssignment());
  EXPECT_EQ(w.admin.recentAudit(w.login("admin1"), 0).size(), 1u);       // clamped up to 1
  EXPECT_EQ(w.admin.recentAudit(w.login("admin1"), 100000).size(), 2u);  // clamped down to 500
}

TEST(AssignmentExtra, LecturerListsOwnCoursesAndAssignmentsOnly) {
  World w;
  w.seed();
  w.newAssignment();
  const Session l = w.login("lect1");
  EXPECT_EQ(w.assignmentSvc.coursesTaught(l).size(), 1u);
  EXPECT_EQ(w.assignmentSvc.assignmentsOfCourse(l, w.course).size(), 1u);
  w.admin.createUser(w.login("admin1"), World::request(Role::Lecturer, "lect2", "", w.dept));
  EXPECT_TRUE(w.assignmentSvc.coursesTaught(w.login("lect2")).empty());
  EXPECT_THROW(w.assignmentSvc.assignmentsOfCourse(w.login("lect2"), w.course), AuthorizationError);
  EXPECT_THROW(w.assignmentSvc.coursesTaught(w.login("stud1")), AuthorizationError);
  EXPECT_EQ(w.assignmentSvc.myCourses(w.login("stud1")).size(), 1u);
}

TEST(TypesExtra, EnumsRoundTripThroughTheirDatabaseText) {
  for (Role r : {Role::Admin, Role::Lecturer, Role::Hod, Role::Student}) {
    EXPECT_EQ(roleFromDb(toDb(r)), r);
    EXPECT_FALSE(displayName(r).empty());
  }
  for (auto s : {SubmissionStatus::Submitted, SubmissionStatus::Late, SubmissionStatus::Marked,
                 SubmissionStatus::Returned, SubmissionStatus::Moderated, SubmissionStatus::Released}) {
    EXPECT_EQ(statusFromDb(toDb(s)), s);
    EXPECT_FALSE(displayName(s).empty());
  }
  for (auto f : {AuditField::RawMark, AuditField::FinalMark}) {
    EXPECT_EQ(auditFieldFromDb(toDb(f)), f);
    EXPECT_FALSE(displayName(f).empty());
  }
  EXPECT_THROW(roleFromDb("ROOT"), PersistenceError);  // corrupt data fails loudly, not silently
  EXPECT_THROW(statusFromDb("DONE"), PersistenceError);
  EXPECT_THROW(auditFieldFromDb("MARK"), PersistenceError);
}

TEST(PolicyExtra, PoliciesDescribeThemselves) {
  EXPECT_NE(TenPercentPerDayPolicy().describe().find("10%"), std::string::npos);
  EXPECT_NE(TenPercentPerDayPolicy().describe().find("3 days"), std::string::npos);
  EXPECT_EQ(NoPenaltyPolicy().describe(), "No late penalty");
}
