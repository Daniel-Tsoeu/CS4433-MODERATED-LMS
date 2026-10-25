// UC-01 Submit assignment: on-time, late, >3 days late, extension, resubmission,
// invalid file, access checks (FR9-FR12, FR17, FR18, BR3, BR5, BR6).
#include "TestSupport.hpp"

using namespace lms::test;

class SubmissionFlow : public ::testing::Test {
 protected:
  World w;
  void SetUp() override { w.seed(); }
  SubmissionReceipt submit(Id a, const std::string& user = "stud1", const std::string& file = "") {
    return w.submissionSvc.submit(w.login(user), a, file.empty() ? w.makeFile() : file);
  }
};

TEST_F(SubmissionFlow, OnTimeSubmissionIsSubmittedWithNoPenalty) {
  const Id a = w.newAssignment(2 * kDay);
  const auto r = submit(a);
  EXPECT_EQ(r.status, SubmissionStatus::Submitted);
  EXPECT_EQ(r.versionNo, 1);
  EXPECT_EQ(r.daysLate, 0);
  EXPECT_DOUBLE_EQ(r.penaltyPercent, 0);
  EXPECT_FALSE(r.resubmission);
  EXPECT_EQ(w.submissions.versions(r.submissionId).size(), 1u);
}

TEST_F(SubmissionFlow, SubmissionExactlyAtTheDeadlineIsOnTime) {
  const Id a = w.newAssignment(kDay);
  w.clock.advance(kDay);
  EXPECT_EQ(submit(a).status, SubmissionStatus::Submitted);
}

TEST_F(SubmissionFlow, OneSecondAfterTheDeadlineIsLateWithTenPercent) {
  const Id a = w.newAssignment(kDay);
  w.clock.advance(kDay + 1);
  const auto r = submit(a);
  EXPECT_EQ(r.status, SubmissionStatus::Late);
  EXPECT_EQ(r.daysLate, 1);
  EXPECT_DOUBLE_EQ(r.penaltyPercent, 10);
}

TEST_F(SubmissionFlow, ThreeDaysLateIsThirtyPercentAndMoreThanThreeDaysIsAcceptedButZero) {
  const Id a = w.newAssignment(kDay);
  w.clock.advance(kDay + 3 * kDay);  // exactly 3 days late
  auto r = submit(a);
  EXPECT_EQ(r.daysLate, 3);
  EXPECT_DOUBLE_EQ(r.penaltyPercent, 30);

  w.addStudent("stud2", "S0002");
  w.clock.advance(1);  // now 3 days + 1 second late
  r = submit(a, "stud2");
  EXPECT_EQ(r.status, SubmissionStatus::Late);  // accepted for the record (FR/UC-01 EF2)
  EXPECT_EQ(r.daysLate, 4);
  EXPECT_DOUBLE_EQ(r.penaltyPercent, 100);
  // and the lecturer's marking then yields zero (FR18)
  w.marking.mark(w.login("lect1"), r.submissionId, 45, "Late but marked", "");
  EXPECT_DOUBLE_EQ(*w.submissions.findSummary(r.submissionId)->submission.finalMark(), 0);
}

TEST_F(SubmissionFlow, ExtensionMovesTheEffectiveDeadlineForThatStudentOnly) {
  const Id a = w.newAssignment(kDay);
  w.addStudent("stud2", "S0002");
  w.assignmentSvc.grantExtension(w.login("lect1"), a, w.studentId, w.clock.now() + 3 * kDay,
                                 "Medical certificate");
  w.clock.advance(2 * kDay);  // past the original deadline, inside the extension
  EXPECT_EQ(submit(a).status, SubmissionStatus::Submitted);      // stud1: on time thanks to the extension
  EXPECT_EQ(submit(a, "stud2").status, SubmissionStatus::Late);  // stud2: original deadline applies
}

TEST_F(SubmissionFlow, LateAfterTheExtensionIsPenalisedFromTheExtendedDeadline) {
  const Id a = w.newAssignment(kDay);
  w.assignmentSvc.grantExtension(w.login("lect1"), a, w.studentId, w.clock.now() + 2 * kDay,
                                 "Family emergency");
  w.clock.advance(2 * kDay + 10);
  const auto r = submit(a);
  EXPECT_EQ(r.daysLate, 1);
  EXPECT_DOUBLE_EQ(r.penaltyPercent, 10);
}

TEST_F(SubmissionFlow, ResubmissionBeforeTheDeadlineAddsAVersionAndKeepsOneSubmission) {
  const Id a = w.newAssignment(2 * kDay);
  const auto first = submit(a, "stud1", w.makeFile("v1.pdf", "one"));
  w.clock.advance(kDay);
  const auto second = submit(a, "stud1", w.makeFile("v2.pdf", "two"));
  EXPECT_TRUE(second.resubmission);
  EXPECT_EQ(second.submissionId, first.submissionId);
  EXPECT_EQ(second.versionNo, 2);
  const auto versions = w.submissions.versions(first.submissionId);
  ASSERT_EQ(versions.size(), 2u);
  EXPECT_EQ(versions[1].originalName, "v2.pdf");
  EXPECT_TRUE(std::filesystem::exists(w.files.root() / versions[0].filePath));  // old version kept
  EXPECT_TRUE(std::filesystem::exists(w.files.root() / versions[1].filePath));
  EXPECT_EQ(w.submissions.find({}).size(), 1u);
}

TEST_F(SubmissionFlow, ResubmissionAfterTheDeadlineIsRefusedAndNothingChanges) {
  const Id a = w.newAssignment(kDay);
  const auto first = submit(a);
  w.clock.advance(kDay + 1);
  EXPECT_THROW(submit(a), InvalidStateTransition);
  EXPECT_EQ(w.submissions.versions(first.submissionId).size(), 1u);
  EXPECT_EQ(w.submissions.findSummary(first.submissionId)->submission.status(), SubmissionStatus::Submitted);
}

TEST_F(SubmissionFlow, ResubmissionAfterMarkingIsRefused) {
  const Id a = w.newAssignment(5 * kDay);
  w.submitAndMark(a);
  EXPECT_THROW(submit(a), InvalidStateTransition);
}

TEST_F(SubmissionFlow, LateSubmissionCannotBeResubmitted) {
  const Id a = w.newAssignment(kDay);
  w.clock.advance(kDay + 5);
  submit(a);
  EXPECT_THROW(submit(a), InvalidStateTransition);
}

TEST_F(SubmissionFlow, InvalidFilesAreRejectedBeforeAnythingIsStored) {
  const Id a = w.newAssignment();
  EXPECT_THROW(submit(a, "stud1", w.makeFile("malware.exe")), ValidationError);
  EXPECT_THROW(submit(a, "stud1", w.makeFile("empty.pdf", "")), ValidationError);
  EXPECT_THROW(submit(a, "stud1", (w.tmp.path / "missing.pdf").string()), ValidationError);
  EXPECT_TRUE(w.submissions.find({}).empty());
  EXPECT_TRUE(std::filesystem::is_empty(w.files.root()));  // no orphan files
}

TEST_F(SubmissionFlow, UnenrolledStudentCannotSubmit) {
  const Id a = w.newAssignment();
  w.addStudent("outsider", "S0099", /*enrol=*/false);
  EXPECT_THROW(submit(a, "outsider"), AuthorizationError);
  EXPECT_THROW(w.assignmentSvc.assignmentsForStudent(w.login("outsider"), w.course), AuthorizationError);
}

TEST_F(SubmissionFlow, UnknownAssignmentIsReported) {
  EXPECT_THROW(submit(9999), NotFoundError);
}

TEST_F(SubmissionFlow, DatabaseFailureRollsBackAndDeletesTheStoredFile) {
  const Id a = w.newAssignment();
  // Sabotage: make the versions insert fail so the transaction must roll back.
  w.db.exec(
      "CREATE TRIGGER sabotage BEFORE INSERT ON submission_versions "
      "BEGIN SELECT RAISE(ABORT,'disk full'); END;");
  EXPECT_THROW(submit(a), ConflictError);
  EXPECT_TRUE(w.submissions.find({}).empty());             // no half-saved submission row
  EXPECT_TRUE(std::filesystem::is_empty(w.files.root()));  // stored copy removed
}

TEST_F(SubmissionFlow, SubmissionNotifiesTheCourseLecturer) {
  const Id a = w.newAssignment(kDay);
  w.clock.advance(kDay + 1);
  submit(a);
  const auto inbox = w.notificationSvc.inbox(w.login("lect1"));
  ASSERT_EQ(inbox.size(), 1u);
  EXPECT_NE(inbox[0].message.find("(late)"), std::string::npos);
}

TEST_F(SubmissionFlow, StudentSeesOwnAssignmentsWithTheirEffectiveDeadline) {
  const Id a = w.newAssignment(kDay);
  const TimePoint ext = w.clock.now() + 4 * kDay;
  w.assignmentSvc.grantExtension(w.login("lect1"), a, w.studentId, ext, "Illness");
  const auto views = w.assignmentSvc.assignmentsForStudent(w.login("stud1"), w.course);
  ASSERT_EQ(views.size(), 1u);
  EXPECT_TRUE(views[0].hasExtension);
  EXPECT_EQ(views[0].effectiveDeadline, ext);
  EXPECT_FALSE(views[0].submissionStatus.has_value());
}

TEST_F(SubmissionFlow, ExtensionRules) {
  const Id a = w.newAssignment(kDay);
  const Session l = w.login("lect1");
  const TimePoint orig = w.clock.now() + kDay;
  EXPECT_THROW(w.assignmentSvc.grantExtension(l, a, w.studentId, orig, "Reason ok"),
               ValidationError);  // not later
  EXPECT_THROW(w.assignmentSvc.grantExtension(l, a, w.studentId, orig + 1, "No"),
               ValidationError);  // reason too short
  EXPECT_THROW(w.assignmentSvc.grantExtension(l, a, w.lecturerId, orig + 1, "Reason ok"),
               ValidationError);  // not a student
  w.addStudent("outsider", "S0099", false);
  EXPECT_THROW(
      w.assignmentSvc.grantExtension(l, a, w.users.findByUsername("outsider")->id(), orig + 1, "Reason ok"),
      ValidationError);  // not enrolled
  EXPECT_NO_THROW(w.assignmentSvc.grantExtension(l, a, w.studentId, orig + 100, "Reason ok"));
  EXPECT_NO_THROW(w.assignmentSvc.grantExtension(l, a, w.studentId, orig + 200, "Second grant replaces"));
  EXPECT_EQ(w.extensions.find(a, w.studentId)->newDeadline, orig + 200);
  // a lecturer of another course may not grant it
  const Session a1 = w.login("admin1");
  const Id other = w.admin.createUser(a1, World::request(Role::Lecturer, "lect2", "", w.dept));
  (void)other;
  EXPECT_THROW(w.assignmentSvc.grantExtension(w.login("lect2"), a, w.studentId, orig + 300, "Reason ok"),
               AuthorizationError);
}

TEST_F(SubmissionFlow, ExtensionCannotBeGrantedAfterWorkIsLateOrMarked) {
  const Id a = w.newAssignment(kDay);
  w.clock.advance(kDay + 1);
  submit(a);  // LATE
  EXPECT_THROW(
      w.assignmentSvc.grantExtension(w.login("lect1"), a, w.studentId, w.clock.now() + kDay, "Too late now"),
      InvalidStateTransition);
}

TEST_F(SubmissionFlow, AssignmentCreationRules) {
  const Session l = w.login("lect1");
  EXPECT_THROW(w.assignmentSvc.createAssignment(l, w.course, "", "", w.clock.now() + kDay, 10),
               ValidationError);
  EXPECT_THROW(w.assignmentSvc.createAssignment(l, w.course, "T", "", w.clock.now() - 1, 10),
               ValidationError);
  EXPECT_THROW(w.assignmentSvc.createAssignment(l, w.course, "T", "", w.clock.now() + kDay, 0),
               ValidationError);
  EXPECT_THROW(w.assignmentSvc.createAssignment(l, 9999, "T", "", w.clock.now() + kDay, 10), NotFoundError);
  // a lecturer who does not teach the course
  const Session a = w.login("admin1");
  w.admin.createUser(a, World::request(Role::Lecturer, "lect2", "", w.dept));
  EXPECT_THROW(
      w.assignmentSvc.createAssignment(w.login("lect2"), w.course, "T", "", w.clock.now() + kDay, 10),
      AuthorizationError);
}
