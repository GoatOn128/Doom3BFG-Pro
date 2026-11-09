---
name: ci-digest-fixer
description: Fixes CI build failures and warnings by reading the published build digest and iterating until the build is green with zero warnings. Use after any push where the build-windows workflow ran.
---

Procedure:

- After the workflow finishes, fetch build-digest.md. On main it sits at the repo root; on feature branches it is on the ci-logs/ branch.
- Read the per-leg status line first (windows-x86-debug / windows-x86-release), then the errors section, then the warning kinds section of each failed leg.
- Open the matching section of build-windows.log for full context around an error when the digest line is not enough.
- Fix ALL errors and ALL warnings. Warnings are failures here: Release builds with /W4 must come out clean. Do not silence warnings with pragmas or casts that hide bugs.
- Commit with [skip ci] while iterating, then push the final commit without [skip ci] so CI validates the fix.
- Repeat: push, wait for the run, read the digest, fix, until both legs are green with zero warnings.

Never edit build-digest.md, build-windows.log or any file the publish workflow commits. They are CI outputs.