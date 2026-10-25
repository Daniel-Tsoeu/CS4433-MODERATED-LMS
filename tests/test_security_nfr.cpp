// Security and non-functional requirements (NFR1 login <= 2 s with 800 users,
// NFR4 hashed passwords, robustness: invalid input never crashes the program).
#include <chrono>
#include <random>

#include "TestSupport.hpp"
#include "domain/UserFactory.hpp"

using namespace lms::test;
using Clock = std::chrono::steady_clock;

static long long millisSince(Clock::time_point t) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t).count();
}

TEST(PasswordSecurity, DatabaseNeverContainsThePlainPassword) {
  World w;
  w.seed();
  auto st = w.db.prepare("SELECT username,password_hash FROM users");
  int rows = 0;
  while (st.step()) {
    ++rows;
    const std::string hash = st.textAt(1);
    EXPECT_EQ(hash.rfind("$argon2id$", 0), 0u) << st.textAt(0);  // Argon2id, self-describing
    EXPECT_EQ(hash.find(kPassword), std::string::npos);          // no plaintext inside
    EXPECT_GT(hash.size(), 60u);
  }
  EXPECT_EQ(rows, 4);
}

TEST(PasswordSecurity, SamePasswordGivesDifferentHashesBecauseOfRandomSalt) {
  World w;
  w.seed();
  const auto a = w.users.findByUsername("stud1")->passwordHash();
  const auto b = w.users.findByUsername("lect1")->passwordHash();
  EXPECT_NE(a, b);
  EXPECT_TRUE(w.hasher.verify(a, kPassword));
  EXPECT_TRUE(w.hasher.verify(b, kPassword));
  EXPECT_FALSE(w.hasher.verify(a, "passw0rd1"));           // case matters
  EXPECT_FALSE(w.hasher.verify("not-a-hash", kPassword));  // garbage hash fails closed
}

TEST(PasswordSecurity, ProductionCostProfileUsesTheInteractiveArgon2Parameters) {
  const SodiumPasswordHasher prod;  // default = production cost
  const std::string h = prod.hash("Passw0rd1");
  EXPECT_NE(h.find("$argon2id$"), std::string::npos);
  EXPECT_NE(h.find("m=65536"), std::string::npos);  // 64 MiB memory cost
  EXPECT_NE(h.find("t=2"), std::string::npos);      // 2 passes
  EXPECT_TRUE(prod.verify(h, "Passw0rd1"));
}

TEST(PasswordSecurity, PlainPasswordAppearsInNoTextColumnOfAnyTable) {
  World w;
  w.seed();
  w.submitAndMark(w.newAssignment());
  const std::pair<const char*, const char*> columns[] = {
      {"users", "username"},       {"users", "full_name"},       {"users", "email"},
      {"users", "password_hash"},  {"notifications", "message"}, {"mark_audit", "reason"},
      {"submissions", "feedback"}, {"assignments", "title"},     {"assignments", "description"},
      {"departments", "name"},     {"courses", "title"}};
  for (const auto& [table, column] : columns) {
    auto st = w.db.prepare(std::string("SELECT COUNT(*) FROM ") + table + " WHERE " + column + " LIKE ?");
    st.bindText(1, std::string("%") + kPassword + "%");
    ASSERT_TRUE(st.step());
    EXPECT_EQ(st.intAt(0), 0) << table << "." << column;
  }
}

// NFR1: "login completes within 2 seconds with 800 users stored".
TEST(Performance, LoginWithEightHundredStudentsStoredTakesUnderTwoSeconds) {
  World w;
  w.seed();
  {
    Transaction tx(w.uow);  // bulk-load 800 students with the cheap hasher (setup only)
    const std::string cheap = w.hasher.hash(kPassword);
    for (int i = 0; i < 800; ++i) {
      UserData d;
      d.username = "bulk" + std::to_string(i);
      d.fullName = "Bulk " + std::to_string(i);
      d.passwordHash = cheap;
      d.studentNumber = "B" + std::to_string(100000 + i);
      d.createdAt = w.clock.now();
      w.users.insert(*UserFactory::create(Role::Student, d));
    }
    tx.commit();
  }
  ASSERT_GE(w.users.count(), 804u);

  // The measured user is hashed at the real production cost.
  const SodiumPasswordHasher production;
  UserData d;
  d.username = "timed";
  d.fullName = "Timed User";
  d.passwordHash = production.hash(kPassword);
  d.studentNumber = "T99999";
  d.createdAt = w.clock.now();
  w.users.insert(*UserFactory::create(Role::Student, d));
  AuthService prodAuth(w.users, const_cast<SodiumPasswordHasher&>(production), w.clock);

  const auto t0 = Clock::now();
  const Session s = prodAuth.login("timed", kPassword);
  const auto ms = millisSince(t0);
  EXPECT_EQ(s.role(), Role::Student);
  EXPECT_LT(ms, 2000) << "login took " << ms << " ms";
  RecordProperty("login_ms_with_800_students", static_cast<int>(ms));
}

TEST(Performance, ListingAndModerationQueuesStayFastWithEightHundredSubmissions) {
  World w;
  w.seed();
  const Id a = w.newAssignment(10 * kDay, 100);
  {
    Transaction tx(w.uow);
    for (int i = 0; i < 800; ++i) {
      UserData d;
      d.username = "bulk" + std::to_string(i);
      d.fullName = "Bulk " + std::to_string(i);
      d.passwordHash = "unused";
      d.studentNumber = "B" + std::to_string(100000 + i);
      d.createdAt = w.clock.now();
      const Id id = w.users.insert(*UserFactory::create(Role::Student, d));
      w.courses.enrol(w.course, id, w.clock.now());
      Submission s = Submission::createNew(a, id, w.clock.now(), {});
      const Id sid = w.submissions.insert(s);
      Submission m = *w.submissions.findByAssignmentAndStudent(a, id);
      m.applyMark(60, 100, "ok", w.clock.now());
      w.submissions.update(m);
      (void)sid;
    }
    tx.commit();
  }
  const auto t0 = Clock::now();
  const auto queue = w.moderation.listForModeration(w.login("hod1"));
  const auto all = w.marking.listSubmissions(w.login("lect1"), a);
  const auto ms = millisSince(t0);
  EXPECT_EQ(queue.size(), 800u);
  EXPECT_EQ(all.size(), 800u);
  EXPECT_LT(ms, 1000) << "listing took " << ms << " ms";
  RecordProperty("list_ms_800_submissions", static_cast<int>(ms));
}

TEST(Performance, ReleasingEightHundredMarksIsOneAtomicTransactionUnderTwoSeconds) {
  World w;
  w.seed();
  const Id a = w.newAssignment(10 * kDay, 100);
  {
    Transaction tx(w.uow);
    for (int i = 0; i < 800; ++i) {
      UserData d;
      d.username = "bulk" + std::to_string(i);
      d.fullName = "Bulk";
      d.passwordHash = "unused";
      d.studentNumber = "B" + std::to_string(100000 + i);
      d.createdAt = w.clock.now();
      const Id id = w.users.insert(*UserFactory::create(Role::Student, d));
      w.courses.enrol(w.course, id, w.clock.now());
      w.submissions.insert(Submission::createNew(a, id, w.clock.now(), {}));
      Submission m = *w.submissions.findByAssignmentAndStudent(a, id);
      m.applyMark(60, 100, "ok", 1);
      m.approve(std::nullopt, 100, 2);
      w.submissions.update(m);
    }
    tx.commit();
  }
  const auto t0 = Clock::now();
  EXPECT_EQ(w.moderation.release(w.login("hod1"), std::nullopt), 800);
  EXPECT_LT(millisSince(t0), 2000);
}

// Robustness: random hostile input may produce a domain error, never a crash
// or an unexpected exception type (Phase 4: exceptions and input validation).
TEST(Robustness, RandomInputOnlyEverRaisesDomainErrors) {
  World w;
  w.seed();
  const Id a = w.newAssignment();
  const Session admin = w.login("admin1"), lect = w.login("lect1"), hod = w.login("hod1"),
                stud = w.login("stud1");
  std::mt19937 rng(12345);
  auto word = [&]() {
    static const char* pool[] = {"",
                                 " ",
                                 "'",
                                 "\"",
                                 "; DROP TABLE users;--",
                                 "%s%n",
                                 "\n",
                                 "A",
                                 "\x01",
                                 "9999999999999999999",
                                 "-1",
                                 "nan",
                                 "0",
                                 "x' OR '1'='1",
                                 "../../etc/passwd",
                                 std::string(5000, 'z').c_str(),
                                 "ünïcödé",
                                 "NULL"};
    return std::string(pool[rng() % (sizeof pool / sizeof *pool)]);
  };
  auto num = [&]() { return static_cast<Id>(static_cast<int>(rng() % 40) - 5); };
  auto real = [&]() {
    static const double v[] = {-1, 0, 0.001, 49.99, 50, 50.01, 1e308, -1e308};
    return v[rng() % 8];
  };
  int domainErrors = 0, ok = 0;
  for (int i = 0; i < 1500; ++i) {
    try {
      switch (rng() % 12) {
        case 0:
          w.admin.createUser(admin, World::request(static_cast<Role>(rng() % 4), word(), word(), num()));
          break;
        case 1:
          w.admin.createDepartment(admin, word(), word());
          break;
        case 2:
          w.admin.createCourse(admin, word(), word(), num(), static_cast<int>(num()) + 2020);
          break;
        case 3:
          w.assignmentSvc.createAssignment(lect, num(), word(), word(), w.clock.now() + num() * kDay, real());
          break;
        case 4:
          w.assignmentSvc.grantExtension(lect, num(), num(), w.clock.now() + num() * kDay, word());
          break;
        case 5:
          w.submissionSvc.submit(stud, num(), word());
          break;
        case 6:
          w.marking.mark(lect, num(), real(), word(), word());
          break;
        case 7:
          w.moderation.approve(hod, num(), real(), word());
          break;
        case 8:
          w.moderation.returnForRemark(hod, num(), word());
          break;
        case 9:
          w.moderation.reopen(hod, num(), real(), word());
          break;
        case 10:
          w.auth.login(word(), word());
          break;
        case 11:
          w.admin.resetPassword(admin, num(), word());
          break;
      }
      ++ok;
    } catch (const LmsError&) {
      ++domainErrors;  // expected: validation, authorisation, not found, conflicts ...
    }
    // anything else (std::exception, crash) fails the test by escaping
  }
  (void)a;
  EXPECT_GT(domainErrors, 500);
  EXPECT_EQ(w.users.count() >= 4u, true);  // data still intact
  EXPECT_NO_THROW(w.login("admin1"));
}
