# Configuration Management Strategy

## 1. Version Control System

Git was used for source-code version control, with GitHub hosting the central project repository.

Repository: https://github.com/Daniel-Tsoeu/CS4433-MODERATED-LMS

The repository maintains the project's source code, documentation, automated tests, configuration files and CI workflow.

## 2. Branching Strategy

A feature-branch workflow was adopted to separate development work from the stable main branch.

| Branch | Purpose |
|---|---|
| `main` | Holds the stable and integrated project version. |
| `feature/*` | Used for implementing and testing specific features or project activities. |
| `backup/*` | Used to preserve a backup of earlier local work where necessary. |

For Phase 6, the branch `feature/phase-6-quality-assurance` was created from `main`.

Changes were implemented and committed on the feature branch before being pushed to GitHub.

## 3. Commit Management

Meaningful commit messages were used to identify the purpose of changes.

Examples include:

- `build(ci): GitHub Actions pipeline, coverage and static-analysis scripts`
- `test: unit, integration, security, NFR and scripted UI tests`
- `ci: modernise GitHub Actions workflow`
- `docs(qa): update quality assurance evidence`

This approach makes changes easier to trace and review.

## 4. Pull Request and Integration Procedure

The intended integration procedure is:

1. Create a feature branch from `main`.
2. Implement the required changes.
3. Test the changes locally or through the CI pipeline.
4. Commit and push the feature branch.
5. Open a pull request targeting `main`.
6. Review the changes and confirm that automated checks pass.
7. Merge the approved changes into `main`.

The pull-request process helps prevent unverified changes from being integrated into the stable branch.

## 5. Continuous Integration

GitHub Actions is configured in `.github/workflows/ci.yml`.

The workflow runs automatically on pushes and pull requests.

It contains two jobs:

- **Build and Test:** Configures and builds the C++ project and runs the automated tests.
- **Quality Assurance:** Runs Cppcheck, Clang-Tidy and the code coverage script.

The workflow was successfully executed on 2 November 2026, with both jobs passing.

## 6. Release and Tagging Strategy

Git tags are used to identify stable project releases.

The project uses semantic versioning:

`vMAJOR.MINOR.PATCH`

- MAJOR: Incompatible system changes.
- MINOR: New functionality that maintains compatibility.
- PATCH: Bug fixes and minor improvements.

A release tag is created after the relevant changes have been reviewed, merged into `main` and verified.

This provides a traceable reference to each stable project version.