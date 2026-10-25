// Persistence and security of the data-access layer (Phase 4: persistence/security 5 marks).
#include "TestSupport.hpp"

using namespace lms::test;

TEST(Schema, CreatedOnceAndVersioned) {
  SqliteDatabase db(":memory:");
  db.initialiseSchema();
  EXPECT_NO_THROW(db.initialiseSchema());  // idempotent
  auto st = db.prepare("PRAGMA user_version");
  ASSERT_TRUE(st.step());
  EXPECT_EQ(st.intAt(0), SqliteDatabase::kSchemaVersion);
}

TEST(Schema, AllDesignTablesExist) {
  SqliteDatabase db(":memory:");
  db.initialiseSchema();
  for (const char* t : {"users", "departments", "students", "lecturers", "heads_of_department", "courses",
                        "course_lecturers", "enrolments", "assignments", "extensions", "submissions",
                        "submission_versions", "mark_audit", "notifications"}) {
    auto st = db.prepare("SELECT 1 FROM sqlite_master WHERE type='table' AND name=?");
    st.bindText(1, t);
    EXPECT_TRUE(st.step()) << t;
  }
}

TEST(Schema, DatabaseRefusesNewerOrUnknownSchemaVersion) {
  SqliteDatabase db(":memory:");
  db.exec("PRAGMA user_version = 99");
  EXPECT_THROW(db.initialiseSchema(), PersistenceError);
}

TEST(Constraints, CheckAndUniqueRulesAreEnforcedByTheDatabase) {
  World w;
  w.seed();
  // role CHECK
  EXPECT_THROW(w.db.exec("INSERT INTO users(username,full_name,password_hash,role,created_at) "
                         "VALUES('zzz','Z','h','TEACHER',1)"),
               ConflictError);
  // username length CHECK
  EXPECT_THROW(w.db.exec("INSERT INTO users(username,full_name,password_hash,role,created_at) "
                         "VALUES('ab','Z','h','ADMIN',1)"),
               ConflictError);
  // duplicate username
  EXPECT_THROW(w.db.exec("INSERT INTO users(username,full_name,password_hash,role,created_at) "
                         "VALUES('admin1','Z','h','ADMIN',1)"),
               ConflictError);
  // status CHECK
  const Id a = w.newAssignment();
  EXPECT_THROW(w.db.exec(("INSERT INTO submissions(assignment_id,student_id,status,submitted_at,updated_at)"
                          " VALUES(" +
                          std::to_string(a) + "," + std::to_string(w.studentId) + ",'DONE',1,1)")
                             .c_str()),
               ConflictError);
}

TEST(Constraints, ForeignKeysAreEnforced) {
  World w;
  w.seed();
  EXPECT_THROW(w.db.exec("INSERT INTO enrolments(course_id,student_id,enrolled_at) VALUES(999,999,1)"),
               ConflictError);
}

TEST(Constraints, OneHeadPerDepartmentAndOnlyHodRoleMayHeadOne) {
  World w;
  w.seed();
  const Session a = w.login("admin1");
  EXPECT_THROW(w.admin.createUser(a, World::request(Role::Hod, "hod2", "", w.dept)), ConflictError);
  // trigger: a student cannot be made a head of department
  EXPECT_THROW(w.db.exec(("INSERT INTO heads_of_department(user_id,department_id) VALUES(" +
                          std::to_string(w.studentId) + "," + std::to_string(w.dept) + ")")
                             .c_str()),
               ConflictError);
}

TEST(AuditLog, IsAppendOnlyEnforcedByTriggers) {
  World w;
  w.seed();
  const Id sub = w.submitAndMark(w.newAssignment());
  ASSERT_FALSE(w.audit.listForSubmission(sub).empty());
  EXPECT_THROW(w.db.exec("UPDATE mark_audit SET new_value = 99"), ConflictError);
  EXPECT_THROW(w.db.exec("DELETE FROM mark_audit"), ConflictError);
  EXPECT_FALSE(w.audit.listForSubmission(sub).empty());
}

TEST(ParameterisedQueries, InjectionStringsAreStoredAsPlainDataAndDoNothing) {
  World w;
  w.seed();
  const std::string evil = "x'); DROP TABLE users; --";
  const Session a = w.login("admin1");
  const Id id = w.admin.createDepartment(a, "XX", evil);
  EXPECT_EQ(w.departments.findById(id)->name, evil);  // stored literally
  EXPECT_EQ(w.users.count(), 4u);                     // users table still intact
  // lookup by a hostile username finds nothing and breaks nothing
  EXPECT_EQ(w.users.findByUsername("' OR '1'='1"), nullptr);
  EXPECT_EQ(w.users.findByUsername("admin1' --"), nullptr);
  EXPECT_THROW(w.login("' OR '1'='1", "anything"), AuthenticationError);
}

TEST(UnitOfWork, RollbackLeavesNoPartialData) {
  World w;
  w.seed();
  {
    Transaction tx(w.uow);
    w.departments.insert({0, "TMP", "Temporary"});
    // no commit -> rolled back by the destructor
  }
  auto st = w.db.prepare("SELECT COUNT(*) FROM departments WHERE code='TMP'");
  st.step();
  EXPECT_EQ(st.intAt(0), 0);
}

TEST(UnitOfWork, NestedBeginIsRejected) {
  World w;
  Transaction tx(w.uow);
  EXPECT_THROW(w.uow.begin(), PersistenceError);
}

TEST(UnitOfWork, ExceptionMidwayRollsBackEverything) {
  World w;
  w.seed();
  const Id a = w.newAssignment();
  const auto before = w.audit.listRecent(100).size();
  try {
    Transaction tx(w.uow);
    w.audit.append({0, 1, w.hodId, w.clock.now(), AuditField::FinalMark, std::nullopt, 1, "x", ""});
    (void)a;
    throw std::runtime_error("simulated crash");
  } catch (const std::runtime_error&) {
  }
  EXPECT_EQ(w.audit.listRecent(100).size(), before);
}

TEST(Persistence, DataSurvivesClosingAndReopeningTheDatabaseFile) {
  TempDir tmp;
  const std::string path = (tmp.path / "lms.db").string();
  {
    SqliteDatabase db(path);
    db.initialiseSchema();
    SqliteDepartmentRepository deps(db);
    deps.insert({0, "MCS", "Maths and CS"});
  }
  SqliteDatabase db(path);
  db.initialiseSchema();
  SqliteDepartmentRepository deps(db);
  ASSERT_EQ(deps.listAll().size(), 1u);
  EXPECT_EQ(deps.listAll()[0].code, "MCS");
}

TEST(Persistence, NewDatabaseFileIsOwnerOnly) {
  TempDir tmp;
  const auto path = tmp.path / "lms.db";
  { SqliteDatabase db(path.string()); }
  const auto perms = std::filesystem::status(path).permissions();
  namespace fs = std::filesystem;
  EXPECT_EQ(perms & (fs::perms::group_all | fs::perms::others_all), fs::perms::none);
}

TEST(Persistence, DatabaseErrorBecomesPersistenceErrorNotACrash) {
  EXPECT_THROW(SqliteDatabase("/nonexistent_dir/x/lms.db"), PersistenceError);
}

TEST(FileStore, RejectsPathTraversalAndMissingFiles) {
  World w;
  const auto src = w.makeFile("a.txt");
  EXPECT_THROW(w.files.store(src, "../evil.txt"), PersistenceError);
  EXPECT_THROW(w.files.store(src, "sub/evil.txt"), PersistenceError);
  EXPECT_THROW(w.files.inspect((w.tmp.path / "missing.pdf").string()), ValidationError);
  EXPECT_THROW(w.files.inspect(w.tmp.path.string()), ValidationError);  // a directory
  EXPECT_THROW(w.files.inspect(""), ValidationError);
}
