# MODERATED LEARNING MANAGEMENT SYSTEM
## Installation and Deployment Guide

**Project:** CS4433 Software Engineering  
**Version:** 1.0.0  
**Target Operating System:** Ubuntu 24.04 LTS  
**Intended Audience:** School IT Technician

---

## 1. Introduction

This guide provides instructions for installing, configuring, testing and launching the Moderated Learning Management System (LMS).

The LMS is a C++17 console-based application designed to manage academic departments, users, courses, assignments, submissions, marking and moderation.

The application uses an embedded SQLite database and local file storage. It does not require a separate database server or an internet connection during normal operation.

## 2. System Requirements

### 2.1 Hardware Requirements

The deployment computer should have:

- A working desktop or laptop computer.
- Sufficient disk space for the application, database and submitted files.
- A keyboard and monitor.
- Reliable local storage with appropriate backup arrangements.

### 2.2 Software Requirements

| Software | Requirement |
|---|---|
| Operating system | Ubuntu 24.04 LTS |
| Compiler | GCC 13 or compatible C++17 compiler |
| Build system | CMake 3.28 or compatible version |
| Build utility | GNU Make |
| Database library | SQLite3 |
| Security library | libsodium |
| Testing framework | GoogleTest |

**Note:** The application uses POSIX terminal functionality (`termios`). The documented deployment environment is Linux, specifically Ubuntu 24.04.

## 3. Installation Procedure

### Step 1: Prepare the computer

Start the Ubuntu computer and log in using an account with administrator privileges.

Open the Terminal.

Update the package information:

```bash
sudo apt update
```

### Step 2: Install dependencies

Install the compiler, build tools and required libraries:

```bash
sudo apt install -y g++ cmake pkg-config libsqlite3-dev libsodium-dev libgtest-dev
```

Verify the compiler and CMake installations:

```bash
g++ --version
cmake --version
```

### Step 3: Obtain the application source code

Clone the project repository:

```bash
git clone https://github.com/Daniel-Tsoeu/CS4433-MODERATED-LMS.git
```

Enter the project directory:

```bash
cd CS4433-MODERATED-LMS
```

### Step 4: Configure the application

Create the build configuration:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

CMake checks the required dependencies and prepares the application for compilation.

### Step 5: Build the application

Compile the application:

```bash
cmake --build build -j"$(nproc)"
```

After successful compilation, the executable will be available at:

```text
build/lms
```

### Step 6: Run automated tests

Verify the installation by running:

```bash
ctest --test-dir build --output-on-failure
```

The project includes 138 automated tests. A successful test run indicates that the test suite completed without reported failures.

## 4. Deployment Configuration

### 4.1 Create the data directory

Create a dedicated directory for application data:

```bash
sudo mkdir -p /var/lms/uploads
```

Allow the account running the application to write to this directory:

```bash
sudo chown -R "$USER":"$USER" /var/lms
```

### 4.2 Database and file storage

The application uses the following deployment paths:

| Resource | Location |
|---|---|
| Application executable | `build/lms` |
| Database | `/var/lms/lms.db` |
| Student submissions | `/var/lms/uploads/` |
| Technical error log | `/var/lms/lms-error.log` |

The database is created and initialized automatically when the application starts if it does not already exist.

## 5. Launching the Application

From the project directory, execute:

```bash
./build/lms --db /var/lms/lms.db --uploads /var/lms/uploads --log /var/lms/lms-error.log
```

The application should start and display its console interface.

### 5.1 Initial Administrator Setup

On the first launch with an empty database:

1. The application requests the creation of an administrator account.
2. Enter the administrator account details.
3. Set a password containing at least eight characters, including a letter and a digit.
4. The password is securely hashed using Argon2id rather than stored as plain text.
5. Log in using the administrator account.

## 6. Initial System Configuration

After logging in as the administrator, perform the following setup tasks in order:

1. Create the academic department.
2. Create lecturer accounts.
3. Create Head of Department accounts.
4. Create student accounts.
5. Create the course.
6. Assign the lecturer to the course.
7. Enrol the relevant students.

The system is then ready for normal academic operations.

## 7. Operational Verification

The technician should verify that the application is functioning correctly.

| Test | Expected result |
|---|---|
| Launch the application | Console interface starts |
| Administrator login | Administrator can access authorised functions |
| Create a department | Department record is stored |
| Create a course | Course record is stored |
| Create an assignment | Assignment is available |
| Submit student work | Submission is stored |
| Mark an assignment | Lecturer can record marks and feedback |
| Moderate a mark | HoD can approve or adjust the mark |
| Release results | Student can view released marks and feedback |

## 8. Backup and Maintenance

The application stores its records in the SQLite database and submitted files in the uploads directory.

The technician should regularly