// Unit tests of the domain layer: no database, no files, no I/O.
#include <cmath>

#include "TestSupport.hpp"
#include "domain/UserFactory.hpp"
#include "domain/Validation.hpp"

using namespace lms::test;

// ------------------------------------------------ late penalty (Strategy)
struct PenaltyCase {
  const char* label;
  TimePoint secondsAfterDeadline;
  int days;
  double percent;
  double finalMarkFromRaw42;
};

class PenaltyBoundary : public ::testing::TestWithParam<PenaltyCase> {};

// Boundary table from the Software Design Document: deadline Fri 17:00, max 50, raw 42.
TEST_P(PenaltyBoundary, MatchesDesignWorkedExample) {
  const auto c = GetParam();
  const TenPercentPerDayPolicy policy;
  const TimePoint deadline = 1'000'000;
  const auto r = policy.assess(deadline, deadline + c.secondsAfterDeadline);
  EXPECT_EQ(r.daysLate, c.days) << c.label;
  EXPECT_DOUBLE_EQ(r.penaltyPercent, c.percent) << c.label;
  EXPECT_DOUBLE_EQ(Submission::computeFinalMark(42, 50, r.penaltyPercent), c.finalMarkFromRaw42) << c.label;
}

INSTANTIATE_TEST_SUITE_P(DesignTable, PenaltyBoundary,
                         ::testing::Values(PenaltyCase{"exactly on the deadline", 0, 0, 0, 42},
                                           PenaltyCase{"one second late", 1, 1, 10, 37},
                                           PenaltyCase{"exactly 24h late", kDay, 1, 10, 37},
                                           PenaltyCase{"24h + 1s", kDay + 1, 2, 20, 32},
                                           PenaltyCase{"exactly 72h late", 3 * kDay, 3, 30, 27},
                                           PenaltyCase{"72h + 1s", 3 * kDay + 1, 4, 100, 0},
                                           PenaltyCase{"ten days late", 10 * kDay, 10, 100, 0}));

TEST(LatePenalty, EarlySubmissionHasNoPenalty) {
  const auto r = TenPercentPerDayPolicy().assess(1000, 10);
  EXPECT_EQ(r.daysLate, 0);
  EXPECT_DOUBLE_EQ(r.penaltyPercent, 0);
}
TEST(LatePenalty, NoPenaltyPolicyIsInterchangeable) {
  const NoPenaltyPolicy p;
  const ILatePenaltyPolicy& base = p;  // Open/Closed: another rule, same interface
  EXPECT_DOUBLE_EQ(base.assess(0, 5 * kDay).penaltyPercent, 0);
  EXPECT_EQ(base.assess(0, 5 * kDay).daysLate, 5);
}
TEST(LatePenalty, ConfigurableRule) {  // hypothetical rule from the design text: 5 days, 5% per day
  const TenPercentPerDayPolicy p(5, 5.0);
  EXPECT_DOUBLE_EQ(p.assess(0, 5 * kDay).penaltyPercent, 25);
  EXPECT_DOUBLE_EQ(p.assess(0, 5 * kDay + 1).penaltyPercent, 100);
}
TEST(FinalMark, NeverNegativeAndRounded) {
  EXPECT_DOUBLE_EQ(Submission::computeFinalMark(3, 50, 30), 0);  // 3 - 15 < 0
  EXPECT_DOUBLE_EQ(Submission::computeFinalMark(50, 50, 100), 0);
  EXPECT_DOUBLE_EQ(Submission::computeFinalMark(33.33, 70, 10), 26.33);
}

// ---------------------------------------------------------- state machine
TEST(StateMachine, ExactlyTheDiagramTransitionsAreAllowed) {
  using S = SubmissionStatus;
  using A = SubmissionAction;
  EXPECT_EQ(SubmissionStateMachine::table().size(), 9u);  // + 2 initial = 11 in the diagram
  EXPECT_EQ(SubmissionStateMachine::initialState(false), S::Submitted);
  EXPECT_EQ(SubmissionStateMachine::initialState(true), S::Late);
  EXPECT_EQ(SubmissionStateMachine::apply(S::Submitted, A::Resubmit), S::Submitted);
  EXPECT_EQ(SubmissionStateMachine::apply(S::Submitted, A::Mark), S::Marked);
  EXPECT_EQ(SubmissionStateMachine::apply(S::Late, A::Mark), S::Marked);
  EXPECT_EQ(SubmissionStateMachine::apply(S::Returned, A::Mark), S::Marked);
  EXPECT_EQ(SubmissionStateMachine::apply(S::Marked, A::Remark), S::Marked);
  EXPECT_EQ(SubmissionStateMachine::apply(S::Marked, A::ReturnForRemark), S::Returned);
  EXPECT_EQ(SubmissionStateMachine::apply(S::Marked, A::Approve), S::Moderated);
  EXPECT_EQ(SubmissionStateMachine::apply(S::Moderated, A::Release), S::Released);
  EXPECT_EQ(SubmissionStateMachine::apply(S::Released, A::Reopen), S::Moderated);
}
TEST(StateMachine, EveryOtherCombinationIsRejected) {
  const std::vector<SubmissionStatus> states{SubmissionStatus::Submitted, SubmissionStatus::Late,
                                             SubmissionStatus::Marked,    SubmissionStatus::Returned,
                                             SubmissionStatus::Moderated, SubmissionStatus::Released};
  const std::vector<SubmissionAction> actions{SubmissionAction::Resubmit, SubmissionAction::Mark,
                                              SubmissionAction::Remark,   SubmissionAction::ReturnForRemark,
                                              SubmissionAction::Approve,  SubmissionAction::Release,
                                              SubmissionAction::Reopen};
  int allowed = 0, rejected = 0;
  for (auto s : states)
    for (auto a : actions) {
      if (SubmissionStateMachine::canApply(s, a)) {
        ++allowed;
      } else {
        ++rejected;
        EXPECT_THROW(SubmissionStateMachine::apply(s, a), InvalidStateTransition);
      }
    }
  EXPECT_EQ(allowed, 9);
  EXPECT_EQ(rejected, 6 * 7 - 9);
}

// ------------------------------------------------------ Submission entity
namespace {
Submission fresh(TimePoint at = 100, PenaltyResult p = {}) {
  return Submission::createNew(1, 2, at, p);
}
}  // namespace

TEST(SubmissionEntity, NewOnTimeIsSubmittedNewLateIsLate) {
  EXPECT_EQ(fresh().status(), SubmissionStatus::Submitted);
  EXPECT_EQ(fresh(100, {2, 20}).status(), SubmissionStatus::Late);
}
TEST(SubmissionEntity, ResubmitOnlyWhileSubmittedAndBeforeDeadline) {
  auto s = fresh(100);
  EXPECT_TRUE(s.canResubmit(150, 200));
  EXPECT_TRUE(s.canResubmit(200, 200));  // exactly at the deadline is still allowed
  EXPECT_FALSE(s.canResubmit(201, 200));
  EXPECT_THROW(s.resubmit(201, 200), InvalidStateTransition);
  s.applyMark(10, 50, "ok", 160);
  EXPECT_THROW(s.resubmit(170, 200), InvalidStateTransition);
  auto late = fresh(300, {1, 10});
  EXPECT_THROW(late.resubmit(310, 1000), InvalidStateTransition);  // LATE can never resubmit
}
TEST(SubmissionEntity, MarkValidationBoundaries) {
  auto s = fresh();
  EXPECT_THROW(s.applyMark(-0.01, 50, "x", 1), ValidationError);
  EXPECT_THROW(s.applyMark(50.01, 50, "x", 1), ValidationError);
  EXPECT_THROW(s.applyMark(std::nan(""), 50, "x", 1), ValidationError);
  EXPECT_THROW(s.applyMark(10, 50, "   ", 1), ValidationError);  // feedback required
  EXPECT_EQ(s.status(), SubmissionStatus::Submitted);            // nothing changed by failures
  EXPECT_NO_THROW(s.applyMark(0, 50, "zero", 1));
  EXPECT_NO_THROW(s.applyMark(50, 50, "full", 2));
}
TEST(SubmissionEntity, MarkAppliesPenaltyToFinalMark) {
  auto s = fresh(100, {2, 20});
  const auto c = s.applyMark(40, 50, "fine", 200);
  EXPECT_DOUBLE_EQ(*s.rawMark(), 40);
  EXPECT_DOUBLE_EQ(*s.finalMark(), 30);  // 40 - 20% of 50
  EXPECT_FALSE(c.oldRaw.has_value());
  EXPECT_TRUE(c.rawChanged());
  EXPECT_TRUE(c.finalChanged());
  EXPECT_EQ(s.status(), SubmissionStatus::Marked);
}
TEST(SubmissionEntity, ApproveRequiresMarkAndFeedbackAndValidAdjustment) {
  auto s = fresh();
  EXPECT_THROW(s.approve(std::nullopt, 50, 5), InvalidStateTransition);  // not marked yet
  s.applyMark(20, 50, "ok", 10);
  EXPECT_THROW(s.approve(60.0, 50, 11), ValidationError);  // above the maximum
  EXPECT_EQ(s.status(), SubmissionStatus::Marked);
  const auto c = s.approve(25.0, 50, 12);
  EXPECT_TRUE(c.finalChanged());
  EXPECT_DOUBLE_EQ(*s.finalMark(), 25);
  EXPECT_EQ(s.status(), SubmissionStatus::Moderated);
}
TEST(SubmissionEntity, ZeroedForLateness) {
  EXPECT_TRUE(fresh(0, {4, 100}).zeroedForLateness());
  EXPECT_FALSE(fresh(0, {3, 30}).zeroedForLateness());
}

// ------------------------------------------------ users: polymorphism
namespace {
UserData data(const std::string& name) {
  UserData d;
  d.id = 1;
  d.username = name;
  d.fullName = name;
  d.passwordHash = "x";
  return d;
}
}  // namespace

TEST(Users, FactoryBuildsTheRightSubclassForEveryRole) {
  auto d = data("a");
  d.departmentId = 7;
  d.studentNumber = "S1";
  EXPECT_EQ(UserFactory::create(Role::Admin, d)->role(), Role::Admin);
  EXPECT_EQ(UserFactory::create(Role::Lecturer, d)->role(), Role::Lecturer);
  EXPECT_EQ(UserFactory::create(Role::Hod, d)->role(), Role::Hod);
  EXPECT_EQ(UserFactory::create(Role::Student, d)->role(), Role::Student);
}
TEST(Users, RolesWithoutRequiredProfileDataAreRejected) {
  EXPECT_THROW(UserFactory::create(Role::Student, data("s")), ValidationError);
  EXPECT_THROW(UserFactory::create(Role::Lecturer, data("l")), ValidationError);
  EXPECT_THROW(UserFactory::create(Role::Hod, data("h")), ValidationError);
}
TEST(Users, PermissionsDifferByRolePolymorphically) {
  auto d = data("a");
  d.departmentId = 1;
  d.studentNumber = "S1";
  const auto admin = UserFactory::create(Role::Admin, d);
  const auto lect = UserFactory::create(Role::Lecturer, d);
  const auto hod = UserFactory::create(Role::Hod, d);
  const auto stud = UserFactory::create(Role::Student, d);
  EXPECT_TRUE(admin->can(Permission::ManageUsers));
  EXPECT_FALSE(lect->can(Permission::ManageUsers));
  EXPECT_TRUE(lect->can(Permission::MarkSubmission));
  EXPECT_FALSE(hod->can(Permission::MarkSubmission));  // an HoD is not a drop-in lecturer
  EXPECT_FALSE(hod->can(Permission::CreateAssignment));
  EXPECT_TRUE(hod->can(Permission::ReleaseMarks));  // BR8: only HoDs release
  EXPECT_FALSE(admin->can(Permission::ReleaseMarks));
  EXPECT_FALSE(lect->can(Permission::ReleaseMarks));
  EXPECT_FALSE(stud->can(Permission::ReleaseMarks));
  EXPECT_TRUE(stud->can(Permission::SubmitWork));
  EXPECT_FALSE(admin->can(Permission::SubmitWork));
  EXPECT_FALSE(lect->can(Permission::SubmitWork));
}
TEST(Users, LockoutAfterFiveFailuresAndClearedOnSuccess) {
  auto d = data("a");
  auto u = UserFactory::create(Role::Admin, d);
  for (int i = 0; i < kMaxFailedLogins - 1; ++i) u->recordFailedLogin(1000);
  EXPECT_FALSE(u->isLocked(1000));
  u->recordFailedLogin(1000);
  EXPECT_TRUE(u->isLocked(1000));
  EXPECT_TRUE(u->isLocked(1000 + kLockoutSeconds - 1));
  EXPECT_FALSE(u->isLocked(1000 + kLockoutSeconds));
  u->recordSuccessfulLogin();
  EXPECT_EQ(u->failedAttempts(), 0);
}

// -------------------------------------------------------------- validation
TEST(Validation, Username) {
  EXPECT_NO_THROW(validation::username("ab.c-d_1"));
  EXPECT_THROW(validation::username("ab"), ValidationError);                  // 2 chars
  EXPECT_NO_THROW(validation::username("abc"));                               // 3 chars
  EXPECT_NO_THROW(validation::username(std::string(32, 'a')));                // 32 chars
  EXPECT_THROW(validation::username(std::string(33, 'a')), ValidationError);  // 33 chars
  EXPECT_THROW(validation::username("a b c"), ValidationError);
  EXPECT_THROW(validation::username("x'; DROP TABLE users;--"), ValidationError);
}
TEST(Validation, Password) {
  EXPECT_THROW(validation::password("Abcdef1", "u"), ValidationError);   // 7 chars
  EXPECT_NO_THROW(validation::password("Abcdefg1", "u"));                // 8 chars
  EXPECT_THROW(validation::password("abcdefgh", "u"), ValidationError);  // no digit
  EXPECT_THROW(validation::password("12345678", "u"), ValidationError);  // no letter
  EXPECT_THROW(validation::password(std::string(129, 'a') + "1", "u"), ValidationError);
}
TEST(Validation, EmailAndStudentNumber) {
  EXPECT_EQ(validation::email(""), "");
  EXPECT_NO_THROW(validation::email("a@b.co"));
  EXPECT_THROW(validation::email("a@b"), ValidationError);
  EXPECT_THROW(validation::email("a b@c.d"), ValidationError);
  EXPECT_NO_THROW(validation::studentNumber("202322798"));
  EXPECT_THROW(validation::studentNumber("12"), ValidationError);
  EXPECT_THROW(validation::studentNumber("12 34"), ValidationError);
}
TEST(Validation, TextRejectsControlCharactersAndLength) {
  EXPECT_THROW(validation::text(std::string("a\0b", 3), "F", 10), ValidationError);
  EXPECT_THROW(validation::text("a\nb", "F", 10), ValidationError);
  EXPECT_NO_THROW(validation::text("a\nb", "F", 10, 1, true));
  EXPECT_THROW(validation::text("", "F", 10), ValidationError);
  EXPECT_THROW(validation::text("abcdef", "F", 5), ValidationError);
  EXPECT_EQ(validation::text("  hi  ", "F", 5), "hi");
}
TEST(Validation, SubmissionFileTypeAndSize) {
  EXPECT_NO_THROW(validation::submissionFile({"essay.PDF", 10}));
  EXPECT_THROW(validation::submissionFile({"virus.exe", 10}), ValidationError);
  EXPECT_THROW(validation::submissionFile({"noextension", 10}), ValidationError);
  EXPECT_THROW(validation::submissionFile({"a.pdf", 0}), ValidationError);
  EXPECT_NO_THROW(validation::submissionFile({"a.pdf", validation::kMaxUploadBytes}));
  EXPECT_THROW(validation::submissionFile({"a.pdf", validation::kMaxUploadBytes + 1}), ValidationError);
}
TEST(Validation, AssignmentRules) {
  Assignment a;
  a.title = "T";
  a.deadline = 1000;
  a.maxMark = 50;
  EXPECT_NO_THROW(a.validateNew(999));
  EXPECT_THROW(a.validateNew(1000), ValidationError);  // deadline must be in the future
  a.maxMark = 0;
  EXPECT_THROW(a.validateNew(1), ValidationError);
  a.maxMark = 1000.5;
  EXPECT_THROW(a.validateNew(1), ValidationError);
}
TEST(Assignment, EffectiveDeadlineUsesTheLaterOfOriginalAndExtension) {
  Assignment a;
  a.deadline = 500;
  EXPECT_EQ(a.effectiveDeadline(std::nullopt), 500);
  Extension e;
  e.newDeadline = 900;
  EXPECT_EQ(a.effectiveDeadline(e), 900);
}

// ---------------------------------------------------------------- events
TEST(Events, ObserverFailureDoesNotPropagate) {
  struct Boom : ISubmissionObserver {
    void onEvent(const SubmissionEvent&) override { throw std::runtime_error("boom"); }
  } boom;
  struct Count : ISubmissionObserver {
    int n = 0;
    void onEvent(const SubmissionEvent&) override { ++n; }
  } count;
  SubmissionEventPublisher p;
  std::string logged;
  p.setErrorHandler([&](const std::string& m) { logged = m; });
  p.subscribe(boom);
  p.subscribe(count);
  EXPECT_NO_THROW(p.publish({}));
  EXPECT_EQ(count.n, 1);  // later observers still run
  EXPECT_EQ(logged, "boom");
}
