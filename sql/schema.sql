-- Moderated LMS schema v1 (Software Design Document, section 3 iii).
-- Single source of truth: CMake embeds this file into the executable.
PRAGMA foreign_keys = ON;

CREATE TABLE users (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  username TEXT NOT NULL UNIQUE CHECK (length(username) BETWEEN 3 AND 32),
  full_name TEXT NOT NULL,
  email TEXT,
  password_hash TEXT NOT NULL,
  role TEXT NOT NULL CHECK (role IN ('ADMIN','LECTURER','HOD','STUDENT')),
  is_active INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0,1)),
  failed_attempts INTEGER NOT NULL DEFAULT 0,
  locked_until INTEGER,
  created_at INTEGER NOT NULL
);

CREATE TABLE departments (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  code TEXT NOT NULL UNIQUE,
  name TEXT NOT NULL UNIQUE
);

CREATE TABLE students (
  user_id INTEGER PRIMARY KEY REFERENCES users(id),
  student_number TEXT NOT NULL UNIQUE
);

CREATE TABLE lecturers (
  user_id INTEGER PRIMARY KEY REFERENCES users(id),
  department_id INTEGER NOT NULL REFERENCES departments(id)
);

CREATE TABLE heads_of_department (
  user_id INTEGER PRIMARY KEY REFERENCES users(id),
  department_id INTEGER NOT NULL UNIQUE REFERENCES departments(id)
);

CREATE TABLE courses (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  code TEXT NOT NULL,
  title TEXT NOT NULL,
  department_id INTEGER NOT NULL REFERENCES departments(id),
  academic_year INTEGER NOT NULL,
  UNIQUE (code, academic_year)
);

CREATE TABLE course_lecturers (
  course_id INTEGER NOT NULL REFERENCES courses(id),
  lecturer_id INTEGER NOT NULL REFERENCES lecturers(user_id),
  PRIMARY KEY (course_id, lecturer_id)
);

CREATE TABLE enrolments (
  course_id INTEGER NOT NULL REFERENCES courses(id),
  student_id INTEGER NOT NULL REFERENCES students(user_id),
  enrolled_at INTEGER NOT NULL,
  PRIMARY KEY (course_id, student_id)
);

CREATE TABLE assignments (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  course_id INTEGER NOT NULL REFERENCES courses(id),
  title TEXT NOT NULL,
  description TEXT,
  deadline INTEGER NOT NULL,
  max_mark REAL NOT NULL CHECK (max_mark > 0),
  created_by INTEGER NOT NULL REFERENCES lecturers(user_id),
  created_at INTEGER NOT NULL
);

CREATE TABLE extensions (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  assignment_id INTEGER NOT NULL REFERENCES assignments(id),
  student_id INTEGER NOT NULL REFERENCES students(user_id),
  new_deadline INTEGER NOT NULL,
  reason TEXT NOT NULL CHECK (length(reason) >= 5),
  granted_by INTEGER NOT NULL REFERENCES lecturers(user_id),
  granted_at INTEGER NOT NULL,
  UNIQUE (assignment_id, student_id)
);

CREATE TABLE submissions (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  assignment_id INTEGER NOT NULL REFERENCES assignments(id),
  student_id INTEGER NOT NULL REFERENCES students(user_id),
  status TEXT NOT NULL CHECK (status IN
    ('SUBMITTED','LATE','MARKED','RETURNED','MODERATED','RELEASED')),
  submitted_at INTEGER NOT NULL,
  days_late INTEGER NOT NULL DEFAULT 0 CHECK (days_late >= 0),
  penalty_percent REAL NOT NULL DEFAULT 0 CHECK (penalty_percent BETWEEN 0 AND 100),
  raw_mark REAL CHECK (raw_mark >= 0),
  final_mark REAL CHECK (final_mark >= 0),
  feedback TEXT,
  updated_at INTEGER NOT NULL,
  UNIQUE (assignment_id, student_id)
);

CREATE TABLE submission_versions (
  submission_id INTEGER NOT NULL REFERENCES submissions(id) ON DELETE CASCADE,
  version_no INTEGER NOT NULL CHECK (version_no >= 1),
  file_path TEXT NOT NULL,
  original_name TEXT NOT NULL,
  size_bytes INTEGER NOT NULL CHECK (size_bytes >= 0),
  uploaded_at INTEGER NOT NULL,
  PRIMARY KEY (submission_id, version_no)
);

CREATE TABLE mark_audit (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  submission_id INTEGER NOT NULL REFERENCES submissions(id),
  changed_by INTEGER NOT NULL REFERENCES users(id),
  changed_at INTEGER NOT NULL,
  field TEXT NOT NULL CHECK (field IN ('RAW_MARK','FINAL_MARK')),
  old_value REAL,
  new_value REAL NOT NULL,
  reason TEXT NOT NULL CHECK (length(reason) >= 1)
);

CREATE TABLE notifications (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  recipient_id INTEGER NOT NULL REFERENCES users(id),
  message TEXT NOT NULL,
  created_at INTEGER NOT NULL,
  is_read INTEGER NOT NULL DEFAULT 0 CHECK (is_read IN (0,1))
);

-- Indexes for the busiest queries (login, lists, moderation queue)
CREATE INDEX idx_courses_department ON courses(department_id);
CREATE INDEX idx_enrol_student ON enrolments(student_id);
CREATE INDEX idx_assign_course ON assignments(course_id);
CREATE INDEX idx_sub_student ON submissions(student_id);
CREATE INDEX idx_sub_assign_status ON submissions(assignment_id, status);
CREATE INDEX idx_audit_submission ON mark_audit(submission_id);
CREATE INDEX idx_notif_user ON notifications(recipient_id, is_read);

-- The audit log is append-only, enforced by the database itself
CREATE TRIGGER audit_no_update BEFORE UPDATE ON mark_audit
BEGIN SELECT RAISE(ABORT, 'audit log is append-only'); END;
CREATE TRIGGER audit_no_delete BEFORE DELETE ON mark_audit
BEGIN SELECT RAISE(ABORT, 'audit log is append-only'); END;

-- Only a user whose role is HOD may be made head of a department
CREATE TRIGGER hod_role_insert BEFORE INSERT ON heads_of_department
WHEN (SELECT role FROM users WHERE id = NEW.user_id) <> 'HOD'
BEGIN SELECT RAISE(ABORT, 'user is not a HOD'); END;
CREATE TRIGGER hod_role_update BEFORE UPDATE OF user_id ON heads_of_department
WHEN (SELECT role FROM users WHERE id = NEW.user_id) <> 'HOD'
BEGIN SELECT RAISE(ABORT, 'user is not a HOD'); END;
