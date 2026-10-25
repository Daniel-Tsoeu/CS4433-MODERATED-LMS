// NFR 3 (full user population stays under 2 s) and NFR 6 (>= 99% of 1,000
// consecutive operations complete successfully).
#include <chrono>

#include "TestSupport.hpp"
#include "domain/UserFactory.hpp"

using namespace lms::test;
using Clock = std::chrono::steady_clock;

namespace {
long long msSince(Clock::time_point t) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t).count();
}
Id addUser(World& w, Role role, const std::string& name, const std::string& hash,
           const std::string& number = "", std::optional<Id> dept = std::nullopt) {
  UserData d;
  d.username = name;
  d.fullName = name;
  d.passwordHash = hash;
  d.studentNumber = number;
  d.departmentId = dept;
  d.createdAt = w.clock.now();
  return w.users.insert(*UserFactory::create(role, d));
}
}  // namespace

TEST(Nfr3, FullPopulationOf800Students50Lecturers20Hods10AdminsStaysUnderTwoSeconds) {
  World w;
  w.seed();
  const std::string hash = w.hasher.hash(kPassword);
  {
    Transaction tx(w.uow);
    for (int i = 0; i < 800; ++i)
      addUser(w, Role::Student, "stu" + std::to_string(i), hash, "N" + std::to_string(10000 + i));
    for (int i = 0; i < 50; ++i) addUser(w, Role::Lecturer, "lec" + std::to_string(i), hash, "", w.dept);
    for (int i = 0; i < 10; ++i) addUser(w, Role::Admin, "adm" + std::to_string(i), hash);
    for (int i = 0; i < 20; ++i) {
      const Id d = w.departments.insert({0, "D" + std::to_string(i), "Department " + std::to_string(i)});
      addUser(w, Role::Hod, "hdp" + std::to_string(i), hash, "", d);
    }
    tx.commit();
  }
  EXPECT_GE(w.users.count(), 884u);

  auto t0 = Clock::now();
  for (const char* who : {"stu799", "lec49", "adm9", "hdp19"}) EXPECT_NO_THROW(w.login(who));
  EXPECT_LT(msSince(t0), 2000);

  t0 = Clock::now();
  const auto users = w.admin.listUsers(w.login("adm0"));
  EXPECT_GE(users.size(), 884u);
  EXPECT_LT(msSince(t0), 2000);

  t0 = Clock::now();
  EXPECT_EQ(w.users.findByUsername("stu400")->role(), Role::Student);
  EXPECT_LT(msSince(t0), 2000);
}

TEST(Nfr6, ThousandConsecutiveOperationsCompleteWithAtLeast99PercentSuccess) {
  World w;
  w.seed();
  const Id a = w.newAssignment(30 * kDay, 100);
  const std::string hash = w.hasher.hash(kPassword);
  const Session admin = w.login("admin1"), lect = w.login("lect1"), hod = w.login("hod1");
  int attempted = 0, succeeded = 0;
  std::vector<std::string> failures;
  auto op = [&](auto&& fn) {
    ++attempted;
    try {
      fn();
      ++succeeded;
    } catch (const std::exception& e) {
      if (failures.size() < 5) failures.push_back(e.what());
    }
  };
  std::vector<std::pair<std::string, Id>> students;
  for (int i = 0; i < 200; ++i) {  // 200 x create student
    const std::string name = "usr" + std::to_string(i);
    Id id = 0;
    op([&] { id = addUser(w, Role::Student, name, hash, "R" + std::to_string(10000 + i)); });
    students.push_back({name, id});
  }
  for (const auto& s : students) op([&] { w.admin.enrolStudent(admin, w.course, s.second); });  // 200 x enrol
  std::vector<Id> subs(students.size());
  const std::string file = w.makeFile();
  for (std::size_t i = 0; i < students.size(); ++i)  // 200 x submit
    op([&] { subs[i] = w.submissionSvc.submit(w.login(students[i].first), a, file).submissionId; });
  for (std::size_t i = 0; i < students.size(); ++i)  // 200 x mark
    op([&] { w.marking.mark(lect, subs[i], 40 + static_cast<double>(i % 50), "Feedback", ""); });
  for (std::size_t i = 0; i < students.size(); ++i)  // 200 x approve
    op([&] { w.moderation.approve(hod, subs[i], std::nullopt, ""); });
  EXPECT_EQ(attempted, 1000);
  EXPECT_GE(succeeded * 100, attempted * 99)
      << succeeded << "/" << attempted
      << " succeeded; first failures: " << (failures.empty() ? "none" : failures[0]) << " | "
      << (failures.size() > 1 ? failures[1] : "");
  EXPECT_EQ(w.moderation.release(hod, std::nullopt), 200);  // and the batch release still works
  RecordProperty("operations_succeeded", succeeded);
}
