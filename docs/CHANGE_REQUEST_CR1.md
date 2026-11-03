# CR1 – Maintenance Report: Class-Wide Assignment Deadline Extension

## 1. Change Request

**Reference:** CR1  
**System:** Moderated Learning Management System (LMS)  
**Release:** Version 1.1.0  
**Maintenance Type:** Perfective Maintenance

### Description
The original system allowed lecturers to extend assignment deadlines for individual students only. CR1 requested the ability for lecturers to extend an assignment deadline for an entire class.

The extension must also be visible to students on their assignment lists.

## 2. Classification and Justification

CR1 is classified as **Perfective Maintenance** because it enhances existing functionality by introducing a class-wide deadline extension feature. It improves the usability and flexibility of the LMS without correcting an existing software defect.

## 3. Impact Analysis

The change affects the following system components:

- **Presentation layer:** Lecturer menu updated to support class-wide deadline extensions.
- **Application layer:** Assignment service updated to grant class-wide extensions and calculate effective deadlines.
- **Domain layer:** Class-wide extension entity introduced.
- **Infrastructure layer:** Repository operations and database persistence updated.
- **Database:** A `class_extensions` table introduced, with schema version updated from 1 to 2.
- **Student interface:** Assignment listings updated to display the effective deadline and extension status.

## 4. Implementation Summary

The system was enhanced to allow authorized lecturers to extend an assignment deadline for all students enrolled in the relevant class.

The implementation includes:

1. Recording the assignment, revised deadline, reason, lecturer and grant date.
2. Storing class-wide extensions separately from individual extensions.
3. Displaying the revised deadline on student assignment lists.
4. Applying the effective deadline when processing submissions.
5. Preserving the existing individual extension functionality.
6. Introducing a database schema migration from version 1 to version 2.

Where both individual and class-wide extensions exist, the effective deadline should reflect the later applicable deadline.

## 5. Regression Testing

Regression testing is intended to verify that the new functionality works without breaking existing LMS functionality.

The following tests are included or required:

| Test | Expected result |
|---|---|
| Class-wide extension | All enrolled students receive the revised deadline |
| Student assignment list | Revised deadline is displayed |
| Individual extension | Existing individual extension remains functional |
| Submission deadline | Submission is accepted before the effective deadline |
| Database migration | Existing database data is preserved |
| Existing LMS tests | Previously working functionality remains operational |

**Execution results:** To be completed using the actual test-run output. No test is recorded as passed until its execution is verified.

## 6. Release Information

**Release version:** 1.1.0  
**Release type:** Feature enhancement  
**Change reference:** CR1

### Release highlights
- Class-wide assignment deadline extensions.
- Improved visibility of extended deadlines for students.
- Database support for class-wide extension records.
- Preservation of individual deadline extensions.

## 7. Conclusion

CR1 enhances the LMS by providing lecturers with a more efficient way to manage assignment deadline extensions for entire classes. It also improves communication by ensuring that students can see the applicable revised deadlines on their assignment lists.

The release is subject to successful build verification and regression testing.