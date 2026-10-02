# Branching Strategy

## Branches
- `main`: always builds and passes all tests. Releases are tagged here (`v1.0`, `v1.1`).
- `feature/<name>`: one new feature or requirement (for example `feature/user-hierarchy`).
- `fix/<name>`: one defect fix (for example `fix/late-day-rounding`).
- `release/v1.1` (maintenance only): used for change request CR1 if needed.

## Rules
1. Never commit directly to `main` (except the first setup commits).
2. Create a branch from an up-to-date `main`, make small commits, and push it.
3. Open a pull request into `main`. The CI pipeline (Linux and Windows) must be green.
4. Review the pull request against the code review checklist before merging.
5. Merge with a merge commit, then delete the branch.
6. After merging, update the traceability matrix if a requirement or test changed.

## Commit messages
Format: `type(scope): short message`. Types: feat, fix, test, docs, refactor, build, ci.

## Releases
- `v1.0` is tagged on `main` at the end of the deployment phase.
- `v1.1` is tagged after change request CR1 and the regression tests pass.
