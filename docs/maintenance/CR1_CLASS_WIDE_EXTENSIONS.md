# CR1: Class-Wide Assignment Deadline Extensions

## Change request

A lecturer must be able to extend an assignment deadline for a whole class, rather than only for an individual student. Updated deadlines must be visible in students' assignment lists.

## Maintenance classification

This is perfective maintenance because it adds a requested capability to an existing LMS. It must also preserve existing deadline and submission behaviour.

## Impact analysis

### Requirements

The change extends the assignment-management functionality to support class-wide deadline extensions.

### Application layer

AssignmentService was updated to coordinate class-wide extensions while retaining lecturer authorisation and existing deadline-extension rules.

### Data and integration

The implementation reuses existing course enrolment and extension functionality. AppContext was updated to support the operation's database integration.

### User interface

LecturerWindow was updated with an "Extend whole class" action.

### Existing functionality at risk

Potential regression areas include individual extensions, lecturer permissions, course enrolment checks, deadline calculations, and students' assignment lists.

## Implementation evidence

Feature branch: feature/class-wide-extensions

Implementation commit: be3f23d

Modified components:
- src/application/AssignmentService.h
- src/application/AssignmentService.cpp
- src/app/AppContext.cpp
- src/ui/LecturerWindow.h
- src/ui/LecturerWindow.cpp

Additional tests:
- tests/AssignmentServiceTests.cpp

## Verification and validation

Automated regression tests: 489 of 489 passed.

Test execution time: 139.96 seconds.

Recorded test output: docs/evidence/test-results.txt

Manual acceptance check:
1. Opened the lecturer interface.
2. Confirmed the "Extend whole class" button appeared.
3. Applied a class-wide deadline extension.
4. Opened an enrolled student's assignment list.
5. Confirmed the displayed deadline had changed.

Result: Passed.

## Release status

Implemented and tested on the feature branch. Merge into main and create release v1.1 after final review.

## Limitations

The manual check confirms the displayed deadline changed for an enrolled student. It does not independently establish that every possible enrolment, submission state, or failure scenario was exercised through the GUI.
