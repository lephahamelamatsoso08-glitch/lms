# Moderated LMS — Project Roadmap

This roadmap distinguishes completed software releases from proposed future work.

## Completed Milestones

### v1.0 — Initial Release
- Role-based administration
- Course and enrolment management
- Assignment creation and submission
- Late-submission penalty calculation
- Departmental moderation and mark release
- Audit trail for mark changes
- SQLite persistence
- Automated testing and Windows packaging

### v1.1 — Maintenance Release
- Class-wide assignment deadline extensions
- CR1 impact analysis
- Additional regression tests
- 489 automated tests passed
- Updated Windows release package

## Proposed Future Improvements

### Shared Multi-User Architecture
Investigate replacing local-only storage with a service-based architecture supporting concurrent access from multiple computers.

### Submission File Management
Improve file storage so submitted documents remain accessible if their original paths change.

### Security and Reliability
Expand security testing, backup verification, recovery testing and deployment validation.

### User Experience
Improve navigation, feedback messages and administrative reporting based on user acceptance testing.

## Engineering Priorities

Future changes should follow:
1. Requirements and impact analysis
2. Architecture and design review
3. Implementation on a feature branch
4. Automated and manual testing
5. Documentation updates
6. Reviewed release and version tagging

## Release History

- [v1.0](https://github.com/lephahamelamatsoso08-glitch/lms/releases/tag/v1.0)
- [v1.1](https://github.com/lephahamelamatsoso08-glitch/lms/releases/tag/v1.1)

