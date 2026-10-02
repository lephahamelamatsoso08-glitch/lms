PRAGMA foreign_keys = ON;

CREATE TABLE user_account (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  username TEXT NOT NULL UNIQUE,
  full_name TEXT NOT NULL,
  password_hash TEXT NOT NULL,
  role TEXT NOT NULL CHECK (role IN ('ADMIN','LECTURER','HOD','STUDENT')),
  active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1))
);
CREATE TABLE student (
  user_id INTEGER PRIMARY KEY REFERENCES user_account(id) ON DELETE CASCADE,
  student_number TEXT NOT NULL UNIQUE
);
CREATE TABLE department (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL UNIQUE,
  hod_user_id INTEGER UNIQUE REFERENCES user_account(id)
);
CREATE TABLE course (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  code TEXT NOT NULL UNIQUE,
  title TEXT NOT NULL,
  department_id INTEGER NOT NULL REFERENCES department(id)
);
CREATE TABLE course_lecturer (
  course_id INTEGER NOT NULL REFERENCES course(id) ON DELETE CASCADE,
  lecturer_id INTEGER NOT NULL REFERENCES user_account(id),
  PRIMARY KEY (course_id, lecturer_id)
);
CREATE TABLE enrolment (
  course_id INTEGER NOT NULL REFERENCES course(id) ON DELETE CASCADE,
  student_id INTEGER NOT NULL REFERENCES student(user_id),
  PRIMARY KEY (course_id, student_id)
);
CREATE TABLE assignment (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  course_id INTEGER NOT NULL REFERENCES course(id),
  title TEXT NOT NULL,
  deadline TEXT NOT NULL,
  max_mark REAL NOT NULL CHECK (max_mark > 0)
);
CREATE TABLE extension (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  assignment_id INTEGER NOT NULL REFERENCES assignment(id),
  student_id INTEGER NOT NULL REFERENCES student(user_id),
  new_deadline TEXT NOT NULL,
  reason TEXT NOT NULL CHECK (length(reason) > 0),
  UNIQUE (assignment_id, student_id)
);
CREATE TABLE submission (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  assignment_id INTEGER NOT NULL REFERENCES assignment(id),
  student_id INTEGER NOT NULL REFERENCES student(user_id),
  status TEXT NOT NULL CHECK (status IN ('SUBMITTED','LATE','MARKED','MODERATED','RELEASED')),
  late_days INTEGER NOT NULL DEFAULT 0 CHECK (late_days >= 0),
  raw_mark REAL CHECK (raw_mark IS NULL OR raw_mark >= 0),
  final_mark REAL CHECK (final_mark IS NULL OR final_mark >= 0),
  feedback TEXT,
  released_at TEXT,
  UNIQUE (assignment_id, student_id)
);
CREATE TABLE submission_version (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  submission_id INTEGER NOT NULL REFERENCES submission(id) ON DELETE CASCADE,
  version_no INTEGER NOT NULL CHECK (version_no >= 1),
  file_path TEXT NOT NULL,
  submitted_at TEXT NOT NULL,
  UNIQUE (submission_id, version_no)
);
CREATE TABLE mark_audit (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  submission_id INTEGER NOT NULL REFERENCES submission(id),
  changed_by INTEGER NOT NULL REFERENCES user_account(id),
  changed_at TEXT NOT NULL,
  old_value REAL,
  new_value REAL NOT NULL,
  reason TEXT NOT NULL CHECK (length(reason) > 0)
);
CREATE TABLE notification (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  user_id INTEGER NOT NULL REFERENCES user_account(id) ON DELETE CASCADE,
  message TEXT NOT NULL,
  created_at TEXT NOT NULL,
  is_read INTEGER NOT NULL DEFAULT 0 CHECK (is_read IN (0,1))
);
CREATE INDEX idx_enrolment_student ON enrolment(student_id);
CREATE INDEX idx_assignment_course ON assignment(course_id);
CREATE INDEX idx_submission_assign ON submission(assignment_id);
CREATE INDEX idx_submission_student ON submission(student_id);
CREATE INDEX idx_version_submission ON submission_version(submission_id);
CREATE INDEX idx_audit_submission ON mark_audit(submission_id);
CREATE INDEX idx_notification_user ON notification(user_id);
