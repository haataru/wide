# Git Workflow

How this repository is developed: branching, commits, pull requests, and the
commands you actually need day to day.

## Model

```
main          ── always buildable, always green CI
  └── feature/<name>   a branch with one topic of work
```

- `main` is protected by convention: never commit to it directly, never
  `git push --force` to it.
- Every change lives on a branch. The branch name describes the work:
  `feature/bloom-utils`, `fix/sstable-footer-alignment`, `chore/ci-workflow`.
- One branch = one topic. If a branch does two unrelated things, split it.

## The daily loop

```bash
git switch main             # go to the trunk
git pull                     # get the latest
git switch -c feature/xyz   # create a branch for the new work
# ... write code, build, test ...
git add -A                  # stage the changes
git commit -m "feat: add xyz"
git push -u origin feature/xyz
# open a Pull Request on GitHub
```

After a review:

```bash
git switch main
git pull
git switch feature/xyz
git rebase main              # replay your commits on top of the updated main
git push --force-with-lease  # safe force push: it refuses if someone else pushed
```

`--force-with-lease` instead of `--force`: if a colleague pushed to your branch
in the meantime, the push is rejected instead of silently deleting their work.

## Commit messages

The repository uses [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>: <short summary in the imperative mood>

<optional body: why, not what - the diff already shows what>

<optional footer: refs, notes>
```

Types used here:

| Type | When |
|---|---|
| `feat` | new functionality |
| `fix` | bug fix |
| `docs` | documentation only |
| `test` | tests only |
| `refactor` | no behaviour change |
| `perf` | performance work |
| `build` | build system, dependencies, CI |
| `chore` | housekeeping (formatting, .gitignore) |
| `study` | learning exercises, experiments, throwaway code |

Rules that matter:

- The summary is a **command**: `feat: add bloom filter`, not `added bloom
  filter` or `Bloom filter`.
- One line, up to ~72 characters. The body starts with a blank line.
- **One commit = one logical change.** A commit that touches 10 files for 5
  different reasons cannot be reviewed, cannot be reverted, and cannot be
  bisected.
- Never write `fix`, `wip`, `asdf`, `final`, `final2` as the only message.
  You will need to find that commit later, and you will not be able to.

Good examples from this repository:

```
feat: initial commit of WideDB Storage Engine SDK
docs: add comprehensive README and architecture documentation
study: add BloomFilter utils empty/size_bytes/clear with tests
```

## What not to do

**Do not commit:**

- build output, `build/`, `_deps/`, object files - they are in `.gitignore`;
- secrets. A token pushed to a remote has to be considered leaked and
  rotated, even if you delete the commit later;
- generated files that can be regenerated;
- a whole feature in one 2000-line commit.

**Do not:**

- rewrite published history. `git push --force` to a branch other people use
  destroys their work;
- mix formatting changes with functional changes. If the diff is 300 lines of
  whitespace, nobody can see the real change. Format in a separate commit;
- commit directly to `main`;
- keep a broken `main`. CI must be green on `main`, always.

**Watch out for:**

- `git add .` staging files you did not mean to stage (build artefacts,
  secrets in `.env`). Prefer `git add <specific files>` and check
  `git status` before committing;
- large binaries in the history. They stay in the repository forever, even
  after `git rm`.

## Inspecting history

```bash
git log --oneline --graph --decorate --all   # compact history graph
git show <hash>                               # what exactly changed
git diff                                     # unstaged changes
git diff --staged                             # staged changes
git blame <file>                              # who wrote which line
git log --stat -- src/storage/                # history of one directory
```

`git blame` is not for assigning blame, it is for finding out *when* a line
appeared and in which commit - and that commit usually explains why the line
looks strange.

## Undoing things

| Situation | Command |
|---|---|
| Not staged yet | `git restore <file>` |
| Staged, but not committed | `git restore --staged <file>` |
| Wrong commit message | `git commit --amend` |
| Wrong commit content | `git add -A && git commit --amend` |
| Want to keep changes but drop the commit | `git reset --soft HEAD~1` |
| Want to throw away the commit and the changes | `git reset --hard HEAD~1` |
| Committed, but not pushed, want it gone | `git reset --hard HEAD~1` |
| Pushed a mistake | `git revert <hash>` (safe: adds a new commit) |

`git reset --hard` destroys uncommitted work. It is the fastest way to lose an
evening of work. Think twice.

`git revert` is the right tool for published history: it creates a new commit
that undoes the old one, so nobody else's clone breaks.

## Pull requests

- The PR description explains **what** changed and **why**. The diff already
  shows what.
- Link the issue it closes: `Closes #12`.
- CI must be green before merging.
- Review comments are about the code, not the person.
- Squash-merge a feature branch into `main` if the branch has noisy
  intermediate commits ("address review comments", "fix typo"). The result on
  `main` should read as a clean sequence of logical changes.

## Configuration

The identity used for commits is set per repository:

```bash
git config user.name  "haataru"
git config user.email "mark20122009@gmail.com"
```

`git config --list --local` shows the settings of this repository,
`git config --list --global` shows the settings of the whole machine.
Repository settings win over global ones.

## Recovering from "oh no"

```bash
git reflog                  # the list of where HEAD has been
git reset --hard HEAD@{5}   # go back to that state
```

`git reflog` remembers every position of `HEAD`, even for commits that are no
longer reachable by any branch. Almost everything is recoverable within a few
days. The exception: data that was never committed.
