// Console layer: input validation loops and scripted sessions through the real menus.
#include <sstream>

#include "TestSupport.hpp"
#include "presentation/ConsoleApp.hpp"
#include "presentation/TimeFormat.hpp"

using namespace lms::test;
using namespace lms::presentation;

TEST(Console, ParsesIntegersStrictly) {
  std::int64_t v = 0;
  EXPECT_TRUE(Console::parseInt(" 42 ", v));
  EXPECT_EQ(v, 42);
  EXPECT_TRUE(Console::parseInt("-7", v));
  EXPECT_FALSE(Console::parseInt("", v));
  EXPECT_FALSE(Console::parseInt("12abc", v));
  EXPECT_FALSE(Console::parseInt("1.5", v));
  EXPECT_FALSE(Console::parseInt("99999999999999999999999", v));  // overflow
}
TEST(Console, ParsesDoublesStrictly) {
  double v = 0;
  EXPECT_TRUE(Console::parseDouble("12.5", v));
  EXPECT_DOUBLE_EQ(v, 12.5);
  EXPECT_FALSE(Console::parseDouble("nan", v));
  EXPECT_FALSE(Console::parseDouble("inf", v));
  EXPECT_FALSE(Console::parseDouble("1e", v));
  EXPECT_FALSE(Console::parseDouble("abc", v));
}
TEST(Console, AskIntRepeatsUntilValidAndThrowsAtEndOfInput) {
  std::istringstream in("abc\n99\n-1\n5\n");
  std::ostringstream out;
  Console c(in, out);
  EXPECT_EQ(c.askInt("n", 0, 10), 5);
  EXPECT_NE(out.str().find("Enter a whole number"), std::string::npos);
  EXPECT_THROW(c.askInt("n", 0, 10), InputClosed);
}
TEST(Console, DateTimeParsingRejectsImpossibleDates) {
  TimePoint t = 0;
  EXPECT_TRUE(parseDateTime("2026-11-20 17:00", t));
  EXPECT_EQ(formatDateTime(t), "2026-11-20 17:00");
  EXPECT_FALSE(parseDateTime("2026-02-31 10:00", t));
  EXPECT_FALSE(parseDateTime("2026-13-01 10:00", t));
  EXPECT_FALSE(parseDateTime("2026-11-20 25:00", t));
  EXPECT_FALSE(parseDateTime("20/11/2026 10:00", t));
  EXPECT_FALSE(parseDateTime("2026-11-20", t));
  EXPECT_FALSE(parseDateTime("2026-11-20 10:00 extra", t));
  EXPECT_FALSE(parseDateTime("", t));
}
TEST(Console, YesNoRepeatsUntilClear) {
  std::istringstream in("maybe\ny\n");
  std::ostringstream out;
  Console c(in, out);
  EXPECT_TRUE(c.confirm("ok?"));
}

namespace {
struct Ui {
  World& w;
  std::istringstream in;
  std::ostringstream out;
  Console console;
  Services services;
  Ui(World& world, const std::string& script)
      : w(world), in(script), console(in, out), services{w.auth,
                                                         w.admin,
                                                         w.assignmentSvc,
                                                         w.submissionSvc,
                                                         w.marking,
                                                         w.moderation,
                                                         w.notificationSvc,
                                                         [this](const std::string& m) { logged += m + "\n"; }} {}
  std::string logged;
  int run() { return ConsoleApp(console, services).run(); }
};
}  // namespace

TEST(ConsoleApp, FirstRunSetupLoginAndAdminCreatesADepartmentDespiteBadInput) {
  World w;
  Ui ui(w,
        "root\nRoot User\nweak\nweak\n"            // weak password -> rejected, loop again
        "root\nRoot User\nPassw0rd1\nPassw0rd1\n"  // accepted
        "abc\n1\n"                                 // bad menu choice, then Log in
        "root\nPassw0rd1\n"
        "99\n6\nMCS\nMathematics and CS\n7\n0\n"  // bad choice, create dept, list, log out
        "0\n");                                   // exit
  EXPECT_EQ(ui.run(), 0);
  const std::string o = ui.out.str();
  EXPECT_NE(o.find("Password must be at least 8"), std::string::npos);
  EXPECT_NE(o.find("Administrator created"), std::string::npos);
  EXPECT_NE(o.find("Enter a whole number"), std::string::npos);
  EXPECT_NE(o.find("Department created with ID 1"), std::string::npos);
  EXPECT_NE(o.find("Mathematics and CS"), std::string::npos);
  EXPECT_NE(o.find("You have been logged out"), std::string::npos);
  EXPECT_EQ(w.departments.listAll().size(), 1u);
}

TEST(ConsoleApp, WrongPasswordShowsAFriendlyMessageAndTheProgramContinues) {
  World w;
  w.seed();
  Ui ui(w, "1\nstud1\nWrong1234\n1\nstud1\nPassw0rd1\n0\n0\n");
  EXPECT_EQ(ui.run(), 0);
  EXPECT_NE(ui.out.str().find("Invalid username or password."), std::string::npos);
  EXPECT_NE(ui.out.str().find("Welcome, Name stud1 (Student)"), std::string::npos);
}

TEST(ConsoleApp, DomainErrorInsideAMenuActionIsShownNotThrown) {
  World w;
  w.seed();
  const Id a = w.newAssignment();
  // student: Submit (3), course, assignment, missing file -> error, then log out, exit
  Ui ui(w, "1\nstud1\nPassw0rd1\n3\n" + std::to_string(w.course) + "\n" + std::to_string(a) +
               "\n/no/such/file.pdf\n0\n0\n");
  EXPECT_EQ(ui.run(), 0);
  EXPECT_NE(ui.out.str().find("could not be found"), std::string::npos);
  EXPECT_NE(ui.out.str().find("You have been logged out"), std::string::npos);
}

TEST(ConsoleApp, StudentCompletesASubmissionThroughTheMenus) {
  World w;
  w.seed();
  const Id a = w.newAssignment();
  const std::string file = w.makeFile("report.pdf");
  Ui ui(w, "1\nstud1\nPassw0rd1\n3\n" + std::to_string(w.course) + "\n" + std::to_string(a) + "\n" + file +
               "\n4\n0\n0\n");
  EXPECT_EQ(ui.run(), 0);
  const std::string o = ui.out.str();
  EXPECT_NE(o.find("Submission received. Version 1"), std::string::npos);
  EXPECT_NE(o.find("You may resubmit until"), std::string::npos);
  EXPECT_NE(o.find("Submitted"), std::string::npos);  // shown in 'my submissions'
  EXPECT_EQ(o.find("Good work"), std::string::npos);
}

TEST(ConsoleApp, FullRolePipelineLecturerMarksHodModeratesAndReleasesStudentReads) {
  World w;
  w.seed();
  const Id a = w.newAssignment(3 * kDay, 50);
  w.submissionSvc.submit(w.login("stud1"), a, w.makeFile());
  const std::string C = std::to_string(w.course), A = std::to_string(a);
  // lecturer: Mark (6): course, assignment, submission id 1, raw 44, feedback
  {
    Ui ui(w, "1\nlect1\nPassw0rd1\n6\n" + C + "\n" + A + "\n1\n44\nWell structured report\n0\n0\n");
    ASSERT_EQ(ui.run(), 0);
    EXPECT_NE(ui.out.str().find("Mark saved"), std::string::npos);
  }
  // student sees nothing yet
  {
    Ui ui(w, "1\nstud1\nPassw0rd1\n4\n0\n0\n");
    ASSERT_EQ(ui.run(), 0);
    EXPECT_NE(ui.out.str().find("Awaiting release"), std::string::npos);
    EXPECT_EQ(ui.out.str().find("Well structured report"), std::string::npos);
  }
  // HoD: approve (2) submission 1 with no adjustment; release (4): all courses, confirm
  {
    Ui ui(w, "1\nhod1\nPassw0rd1\n2\n1\n\n4\ny\ny\n0\n0\n");
    ASSERT_EQ(ui.run(), 0);
    EXPECT_NE(ui.out.str().find("Approved."), std::string::npos);
    EXPECT_NE(ui.out.str().find("1 mark(s) released."), std::string::npos);
  }
  // student now sees mark and feedback
  {
    Ui ui(w, "1\nstud1\nPassw0rd1\n4\n0\n0\n");
    ASSERT_EQ(ui.run(), 0);
    EXPECT_NE(ui.out.str().find("44.00/50.00"), std::string::npos);
    EXPECT_NE(ui.out.str().find("Well structured report"), std::string::npos);
  }
}

TEST(ConsoleApp, EndOfInputExitsCleanlyAtAnyPoint) {
  World w;
  w.seed();
  for (const char* script : {"", "1\n", "1\nstud1\n", "1\nstud1\nPassw0rd1\n", "1\nstud1\nPassw0rd1\n3\n"}) {
    Ui ui(w, script);
    EXPECT_EQ(ui.run(), 0) << script;
  }
}

TEST(ConsoleApp, PersistenceFailureIsHiddenFromTheUserAndLogged) {
  World w;
  w.seed();
  w.db.exec("DROP TABLE notifications");  // simulate a corrupted database
  Ui ui(w, "1\nstud1\nPassw0rd1\n0\n0\n");
  EXPECT_EQ(ui.run(), 0);                                            // no crash
  EXPECT_EQ(ui.out.str().find("no such table"), std::string::npos);  // SQL detail not shown to the user
}
