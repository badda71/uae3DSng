# Notes for Claude

- **Git identity:** commit as `badda71 <11392517+badda71@users.noreply.github.com>`. At the start of every session, before the first commit, run
  `git config user.name "badda71"` and `git config user.email "11392517+badda71@users.noreply.github.com"` in the clone. Never commit under another author or committer name.
  This is his GitHub noreply address: his real email is private on GitHub, and a push containing it is rejected (GH007), so it must not appear in commits or files.
- **No attribution lines:** do not add `Co-Authored-By` or `Claude-Session` trailers to commit messages, and no "Generated with Claude Code" or session-URL footer to pull request descriptions.
- **No commit signing:** commits must be unsigned so they do not show up as "unverified" on GitHub. The session's global git config turns signing on, so also run `git config commit.gpgsign false` in the clone before the first commit.
