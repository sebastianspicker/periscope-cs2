# Decision: keep Radar and Vision independent

Status: accepted

Radar and Vision remain separate projects in one repository. Radar is an
MIT-licensed C++ red/blue lab with opt-in real-platform research paths. Vision
is an AGPL-3.0-only Python pixel-processing package. They have different users,
runtimes, dependencies, threat models, and release units.

The repository root may provide documentation, CI routing, and verification
dispatch only. It must not contain shared runtime source or create a build-time
dependency between the projects. Similar-looking code is duplicated across the
license boundary unless it represents a deliberately licensed external package.
