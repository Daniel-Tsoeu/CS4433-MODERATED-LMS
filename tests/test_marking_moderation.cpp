// UC-02 Mark submission and UC-03 Moderate and release marks, plus the audit
// trail (FR13-FR16, FR19, FR20, BR4, BR7, BR8).
#include "TestSupport.hpp"

using namespace lms::test;

class Assessment : public ::testing::Test {
 protected:
  World w;
  Id assignment = 0;
  void SetUp() override {
    w.seed();
    assignment = w.newAssignment(3 * kDay, 50);
  }
  Session lect() { return w.login("lect1"); }
  Session hod() { return w.login("hod1"); }
  Session stud() { return w.login("stud1"); }
  SubmissionStatus statusOf(Id sub) { return w.submissions.findSummary(sub)->submission.status(); }
};

// ------------------------------------------------------------ marking
TEST_F(Assessment, FirstMarkingStoresMarksAndWritesTwoAuditRows) {
  const Id sub = w.submitAndMark(assignment, 42);
  const auto s = w.submissions.findSummary(sub)->submission;
  EXPECT_EQ(s.status(), SubmissionStatus::Marked);
  EXPECT_DOUBLE_EQ(*s.rawMark(), 42);
  EXPECT_DOUBLE_EQ(*s.finalMark(), 42);
  EXPECT_EQ(s.feedback(), "Good work");
  const auto log = w.audit.listForSubmission(sub);
  ASSERT_EQ(log.size(), 2u);  // RAW_MARK and FINAL_MARK (design: one row per changed field)
  EXPECT_EQ(log[0].field, AuditField::RawMark);
  EXPECT_FALSE(log[0].oldValue.has_value());  // null on first mark
  EXPECT_DOUBLE_EQ(log[0].newValue, 42);
  EXPECT_EQ(log[0].changedBy, w.lecturerId);   // who
  EXPECT_EQ(log[0].changedAt, w.clock.now());  // when
  EXPECT_FALSE(log[0].reason.empty());         // why
  EXPECT_EQ(log[0].changedByName, "lect1");
}

TEST_F(Assessment, LatePenaltyIsAppliedAtMarkingAndLoggedAsFinalMarkChange) {
  w.clock.advance(3 * kDay + 5);  // 1 day late (deadline was +3 days, now +3d+5s)
  const auto r = w.submissionSvc.submit(stud(), assignment, w.makeFile());
  ASSERT_EQ(r.status, SubmissionStatus::Late);
  w.marking.mark(lect(), r.submissionId, 42, "Late work", "");
  const auto s = w.submissions.findSummary(r.submissionId)->submission;
  EXPECT_DOUBLE_EQ(*s.rawMark(), 42);
  EXPECT_DOUBLE_EQ(*s.finalMark(), 37);  // design worked example: 42 - 10% of 50
  const auto log = w.audit.listForSubmission(r.submissionId);
  ASSERT_EQ(log.size(), 2u);
  EXPECT_DOUBLE_EQ(log[1].newValue, 37);
}

TEST_F(Assessment, MarkBoundariesAreValidated) {
  const auto r = w.submissionSvc.submit(stud(), assignment, w.makeFile());
  EXPECT_THROW(w.marking.mark(lect(), r.submissionId, 50.01, "x", ""), ValidationError);
  EXPECT_THROW(w.marking.mark(lect(), r.submissionId, -1, "x", ""), ValidationError);
  EXPECT_THROW(w.marking.mark(lect(), r.submissionId, 10, "", ""), ValidationError);  // feedback required
  EXPECT_EQ(statusOf(r.submissionId), SubmissionStatus::Submitted);
  EXPECT_TRUE(w.audit.listForSubmission(r.submissionId).empty());  // failures leave no audit noise
  EXPECT_NO_THROW(w.marking.mark(lect(), r.submissionId, 50, "Full marks", ""));
}

TEST_F(Assessment, RemarkRequiresAReasonAndIsAudited) {
  const Id sub = w.submitAndMark(assignment, 30);
  EXPECT_THROW(w.marking.mark(lect(), sub, 35, "Updated", ""), ValidationError);  // reason needed
  w.clock.advance(60);
  w.marking.mark(lect(), sub, 35, "Updated", "Missed a section");
  const auto log = w.audit.listForSubmission(sub);
  ASSERT_EQ(log.size(), 4u);
  EXPECT_DOUBLE_EQ(*log[2].oldValue, 30);
  EXPECT_DOUBLE_EQ(log[2].newValue, 35);
  EXPECT_EQ(log[2].reason, "Missed a section");
}

TEST_F(Assessment, RemarkWithTheSameMarkWritesNoNewAuditRows) {
  const Id sub = w.submitAndMark(assignment, 30);
  w.marking.mark(lect(), sub, 30, "Better feedback", "Clarified feedback");
  EXPECT_EQ(w.audit.listForSubmission(sub).size(), 2u);
  EXPECT_EQ(w.submissions.findSummary(sub)->submission.feedback(), "Better feedback");
}

TEST_F(Assessment, OnlyTheCourseLecturerMayMark) {
  const Id sub = w.submissionSvc.submit(stud(), assignment, w.makeFile()).submissionId;
  w.admin.createUser(w.login("admin1"), World::request(Role::Lecturer, "lect2", "", w.dept));
  EXPECT_THROW(w.marking.mark(w.login("lect2"), sub, 10, "x", ""), AuthorizationError);
  EXPECT_THROW(w.marking.listSubmissions(w.login("lect2"), assignment), AuthorizationError);
  EXPECT_THROW(w.marking.detail(w.login("lect2"), sub), AuthorizationError);
  EXPECT_THROW(w.marking.mark(lect(), 9999, 10, "x", ""), NotFoundError);
}

TEST_F(Assessment, LecturerSeesSubmissionDetailsAndVersions) {
  const Id sub = w.submissionSvc.submit(stud(), assignment, w.makeFile("essay.pdf")).submissionId;
  const auto d = w.marking.detail(lect(), sub);
  EXPECT_EQ(d.summary.studentNumber, "S0001");
  ASSERT_EQ(d.versions.size(), 1u);
  EXPECT_EQ(d.versions[0].originalName, "essay.pdf");
  EXPECT_EQ(w.marking.listSubmissions(lect(), assignment).size(), 1u);
}

TEST_F(Assessment, MarkingNotifiesTheHodNotTheStudent) {
  w.submitAndMark(assignment);
  EXPECT_EQ(w.notificationSvc.unreadCount(w.login("hod1")), 1);
  EXPECT_EQ(w.notificationSvc.unreadCount(stud()), 0);  // student not told until release
}

// ------------------------------------------- BR4: nothing visible early
TEST_F(Assessment, StudentCannotSeeMarksOrFeedbackBeforeRelease) {
  auto expectHidden = [&](const char* when) {
    const auto results = w.submissionSvc.myResults(stud());
    ASSERT_EQ(results.size(), 1u) << when;
    EXPECT_FALSE(results[0].finalMark.has_value()) << when;
    EXPECT_FALSE(results[0].rawMark.has_value()) << when;
    EXPECT_FALSE(results[0].feedback.has_value()) << when;
  };
  const auto r = w.submissionSvc.submit(stud(), assignment, w.makeFile());
  expectHidden("submitted");
  w.marking.mark(lect(), r.submissionId, 42, "Good work", "");
  expectHidden("marked");
  w.moderation.returnForRemark(hod(), r.submissionId, "Please recheck");
  expectHidden("returned");
  w.marking.mark(lect(), r.submissionId, 40, "Rechecked", "Rechecked rubric");
  w.moderation.approve(hod(), r.submissionId, std::nullopt, "");
  expectHidden("moderated");
}

TEST_F(Assessment, StudentSeesMarkAndFeedbackOnlyAfterRelease) {
  const Id sub = w.submitAndMark(assignment, 42);
  w.moderation.approve(hod(), sub, std::nullopt, "");
  EXPECT_FALSE(w.submissionSvc.myResults(stud())[0].finalMark.has_value());
  EXPECT_EQ(w.moderation.release(hod(), std::nullopt), 1);
  const auto r = w.submissionSvc.myResults(stud());
  ASSERT_EQ(r.size(), 1u);
  EXPECT_DOUBLE_EQ(*r[0].finalMark, 42);
  EXPECT_EQ(*r[0].feedback, "Good work");
}

TEST_F(Assessment, OtherStudentsNeverSeeSomeoneElsesResult) {
  w.submitAndMark(assignment);
  w.addStudent("stud2", "S0002");
  EXPECT_TRUE(w.submissionSvc.myResults(w.login("stud2")).empty());
}

// ---------------------------------------------------------- moderation
TEST_F(Assessment, ModerationQueueListsOnlyMarkedWorkOfOwnDepartment) {
  w.submitAndMark(assignment);
  EXPECT_EQ(w.moderation.listForModeration(hod()).size(), 1u);
  // second department with its own HoD and course must not leak into the first queue
  const Session a = w.login("admin1");
  const Id d2 = w.admin.createDepartment(a, "PHY", "Physics");
  w.admin.createUser(a, World::request(Role::Hod, "hod2", "", d2));
  w.admin.createUser(a, World::request(Role::Lecturer, "lect2", "", d2));
  EXPECT_TRUE(w.moderation.listForModeration(w.login("hod2")).empty());
  EXPECT_TRUE(w.moderation.listDepartment(w.login("hod2"), std::nullopt).empty());
}

TEST_F(Assessment,
       HodOfAnotherDepartmentIsRefusedAndNothingChanges) {  // sequence diagram: AuthorizationError
  const Id sub = w.submitAndMark(assignment);
  const Session a = w.login("admin1");
  const Id d2 = w.admin.createDepartment(a, "PHY", "Physics");
  w.admin.createUser(a, World::request(Role::Hod, "hod2", "", d2));
  const Session other = w.login("hod2");
  EXPECT_THROW(w.moderation.approve(other, sub, std::nullopt, ""), AuthorizationError);
  EXPECT_THROW(w.moderation.returnForRemark(other, sub, "Not yours at all"), AuthorizationError);
  EXPECT_THROW(w.moderation.auditTrail(other, sub), AuthorizationError);
  EXPECT_EQ(statusOf(sub), SubmissionStatus::Marked);
}

TEST_F(Assessment, ApproveWithoutAdjustmentKeepsTheMarkAndLogsNothingNew) {
  const Id sub = w.submitAndMark(assignment, 42);
  w.moderation.approve(hod(), sub, std::nullopt, "");
  EXPECT_EQ(statusOf(sub), SubmissionStatus::Moderated);
  EXPECT_EQ(w.audit.listForSubmission(sub).size(), 2u);
}

TEST_F(Assessment, AdjustmentNeedsATenCharacterReasonAndIsAudited) {
  const Id sub = w.submitAndMark(assignment, 42);
  EXPECT_THROW(w.moderation.approve(hod(), sub, 45.0, ""), ValidationError);
  EXPECT_THROW(w.moderation.approve(hod(), sub, 45.0, "too short"), ValidationError);           // 9 chars
  EXPECT_THROW(w.moderation.approve(hod(), sub, 51.0, "Reason long enough"), ValidationError);  // > max
  EXPECT_EQ(statusOf(sub), SubmissionStatus::Marked);
  w.clock.advance(100);
  w.moderation.approve(hod(), sub, 45.0, "Consistent with rubric");  // exactly 22 chars
  const auto s = w.submissions.findSummary(sub)->submission;
  EXPECT_DOUBLE_EQ(*s.finalMark(), 45);
  EXPECT_DOUBLE_EQ(*s.rawMark(), 42);  // the lecturer's raw mark is preserved
  const auto log = w.audit.listForSubmission(sub);
  ASSERT_EQ(log.size(), 3u);
  EXPECT_EQ(log[2].changedBy, w.hodId);
  EXPECT_DOUBLE_EQ(*log[2].oldValue, 42);
  EXPECT_DOUBLE_EQ(log[2].newValue, 45);
  EXPECT_NE(log[2].reason.find("Consistent with rubric"), std::string::npos);
}

TEST_F(Assessment, ReasonOfExactlyTenCharactersIsAccepted) {
  const Id sub = w.submitAndMark(assignment, 42);
  EXPECT_NO_THROW(w.moderation.approve(hod(), sub, 44.0, "0123456789"));
}

TEST_F(Assessment, UnmarkedWorkCannotBeModerated) {  // UC-03 EF2
  const auto r = w.submissionSvc.submit(stud(), assignment, w.makeFile());
  EXPECT_THROW(w.moderation.approve(hod(), r.submissionId, std::nullopt, ""), InvalidStateTransition);
  EXPECT_EQ(w.moderation.release(hod(), std::nullopt), 0);  // nothing moderated, nothing released
  EXPECT_EQ(statusOf(r.submissionId), SubmissionStatus::Submitted);
}

TEST_F(Assessment, ReturnForRemarkNotifiesTheLecturerAndLetsThemMarkAgain) {
  const Id sub = w.submitAndMark(assignment, 42);
  EXPECT_THROW(w.moderation.returnForRemark(hod(), sub, "x"), ValidationError);  // reason too short
  w.moderation.returnForRemark(hod(), sub, "Marks look inconsistent");
  EXPECT_EQ(statusOf(sub), SubmissionStatus::Returned);
  const auto inbox = w.notificationSvc.inbox(lect());
  ASSERT_FALSE(inbox.empty());
  EXPECT_NE(inbox.front().message.find("Marks look inconsistent"), std::string::npos);
  EXPECT_TRUE(w.moderation.listForModeration(hod()).empty());  // out of the HoD queue
  w.marking.mark(lect(), sub, 38, "Re-assessed", "Corrected after HoD review");
  EXPECT_EQ(statusOf(sub), SubmissionStatus::Marked);
  EXPECT_EQ(w.moderation.listForModeration(hod()).size(), 1u);
}

TEST_F(Assessment, ReturnedWorkCannotBeApprovedOrReleased) {
  const Id sub = w.submitAndMark(assignment);
  w.moderation.returnForRemark(hod(), sub, "Please recheck");
  EXPECT_THROW(w.moderation.approve(hod(), sub, std::nullopt, ""), InvalidStateTransition);
  EXPECT_EQ(w.moderation.release(hod(), std::nullopt), 0);
}

TEST_F(Assessment, MissingFeedbackBlocksApproval) {  // UC-03 EF1 (defence in depth)
  const Id sub = w.submitAndMark(assignment);
  w.db.exec("PRAGMA foreign_keys=ON");
  w.db.prepare("UPDATE submissions SET feedback='' WHERE id=?").bindInt(1, sub).run();
  EXPECT_THROW(w.moderation.approve(hod(), sub, std::nullopt, ""), ValidationError);
  EXPECT_EQ(statusOf(sub), SubmissionStatus::Marked);
}

// ------------------------------------------------------------- release
TEST_F(Assessment, ReleaseOnlyAffectsModeratedWorkAndNotifiesStudents) {
  const Id s1 = w.submitAndMark(assignment, 42);
  w.addStudent("stud2", "S0002");
  const Id s2 = w.submitAndMark(assignment, 30, "stud2");
  w.moderation.approve(hod(), s1, std::nullopt, "");  // s2 stays MARKED
  EXPECT_EQ(w.moderation.release(hod(), std::nullopt), 1);
  EXPECT_EQ(statusOf(s1), SubmissionStatus::Released);
  EXPECT_EQ(statusOf(s2), SubmissionStatus::Marked);
  EXPECT_EQ(w.notificationSvc.unreadCount(stud()), 1);
  EXPECT_EQ(w.notificationSvc.unreadCount(w.login("stud2")), 0);
  EXPECT_EQ(w.moderation.release(hod(), std::nullopt), 0);  // nothing left to release
}

TEST_F(Assessment, ReleaseCanBeLimitedToOneCourse) {
  const Id sub = w.submitAndMark(assignment);
  w.moderation.approve(hod(), sub, std::nullopt, "");
  EXPECT_EQ(w.moderation.release(hod(), w.course + 100), 0);  // other course: nothing
  EXPECT_EQ(w.moderation.release(hod(), w.course), 1);
}

TEST_F(Assessment, OnlyHodsMayRelease) {
  const Id sub = w.submitAndMark(assignment);
  w.moderation.approve(hod(), sub, std::nullopt, "");
  for (const char* who : {"admin1", "lect1", "stud1"})
    EXPECT_THROW(w.moderation.release(w.login(who), std::nullopt), AuthorizationError) << who;
  EXPECT_EQ(statusOf(sub), SubmissionStatus::Moderated);
}

TEST_F(Assessment, ReopenWithdrawsAReleasedMarkAndIsAuditedEvenWithoutAChange) {
  const Id sub = w.submitAndMark(assignment, 42);
  w.moderation.approve(hod(), sub, std::nullopt, "");
  w.moderation.release(hod(), std::nullopt);
  EXPECT_THROW(w.moderation.reopen(hod(), sub, std::nullopt, "short"), ValidationError);
  const auto before = w.audit.listForSubmission(sub).size();
  w.moderation.reopen(hod(), sub, 40.0, "Appeal upheld after review");
  EXPECT_EQ(statusOf(sub), SubmissionStatus::Moderated);
  EXPECT_FALSE(w.submissionSvc.myResults(stud())[0].finalMark.has_value());  // hidden again
  const auto log = w.audit.listForSubmission(sub);
  ASSERT_EQ(log.size(), before + 1);
  EXPECT_DOUBLE_EQ(*log.back().oldValue, 42);
  EXPECT_DOUBLE_EQ(log.back().newValue, 40);
  w.moderation.release(hod(), std::nullopt);
  EXPECT_DOUBLE_EQ(*w.submissionSvc.myResults(stud())[0].finalMark, 40);
}

TEST_F(Assessment, ReopenOnlyFromReleased) {
  const Id sub = w.submitAndMark(assignment);
  EXPECT_THROW(w.moderation.reopen(hod(), sub, std::nullopt, "Long enough reason"), InvalidStateTransition);
}

// -------------------------------------------------- reports and audit
TEST_F(Assessment, DepartmentReportCountsModeratedAndReleased) {
  const Id s1 = w.submitAndMark(assignment, 40);
  w.addStudent("stud2", "S0002");
  const Id s2 = w.submitAndMark(assignment, 20, "stud2");
  w.moderation.approve(hod(), s1, std::nullopt, "");
  w.moderation.approve(hod(), s2, std::nullopt, "");
  w.moderation.release(hod(), std::nullopt);
  w.addStudent("stud3", "S0003");
  const Id s3 = w.submitAndMark(assignment, 10, "stud3");
  w.moderation.approve(hod(), s3, std::nullopt, "");  // moderated, not released
  const auto rows = w.moderation.report(hod());
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].released, 2);
  EXPECT_EQ(rows[0].moderated, 1);
  EXPECT_NEAR(*rows[0].averageFinalMark, (40 + 20 + 10) / 3.0, 1e-9);
  EXPECT_THROW(w.moderation.report(lect()), AuthorizationError);
}

TEST_F(Assessment, AuditTrailVisibleToHodAndAdminButNotToLecturersOrStudents) {
  const Id sub = w.submitAndMark(assignment);
  EXPECT_EQ(w.moderation.auditTrail(hod(), sub).size(), 2u);
  EXPECT_EQ(w.moderation.auditTrail(w.login("admin1"), sub).size(), 2u);
  EXPECT_THROW(w.moderation.auditTrail(lect(), sub), AuthorizationError);
  EXPECT_THROW(w.moderation.auditTrail(stud(), sub), AuthorizationError);
  EXPECT_EQ(w.admin.recentAudit(w.login("admin1"), 10).size(), 2u);
  EXPECT_THROW(w.admin.recentAudit(hod(), 10), AuthorizationError);
}

TEST_F(Assessment, EveryMarkChangeInTheWholeLifecycleHasWhoWhenOldNewWhy) {
  const Id sub = w.submitAndMark(assignment, 30);
  w.marking.mark(lect(), sub, 34, "Regraded", "Rubric clarified");
  w.moderation.approve(hod(), sub, 36.0, "Aligned with moderation sample");
  w.moderation.release(hod(), std::nullopt);
  w.moderation.reopen(hod(), sub, 35.0, "Student appeal accepted");
  for (const auto& e : w.audit.listForSubmission(sub)) {
    EXPECT_GT(e.changedBy, 0);
    EXPECT_GT(e.changedAt, 0);
    EXPECT_FALSE(e.reason.empty());
    EXPECT_FALSE(e.changedByName.empty());
  }
  // old value of each change equals the new value of the previous change of the same field
  std::optional<double> lastFinal;
  for (const auto& e : w.audit.listForSubmission(sub)) {
    if (e.field != AuditField::FinalMark) continue;
    EXPECT_EQ(e.oldValue, lastFinal);
    lastFinal = e.newValue;
  }
  EXPECT_DOUBLE_EQ(*lastFinal, 35);
}

TEST_F(Assessment, NotificationsCanBeReadAndMarkedRead) {
  w.submitAndMark(assignment);
  const Session h = hod();
  EXPECT_EQ(w.notificationSvc.unreadCount(h), 1);
  EXPECT_EQ(w.notificationSvc.inbox(h).size(), 1u);
  w.notificationSvc.markAllRead(h);
  EXPECT_EQ(w.notificationSvc.unreadCount(h), 0);
  EXPECT_THROW(w.notificationSvc.inbox(w.login("admin1")), AuthorizationError);  // admins have no inbox
}
