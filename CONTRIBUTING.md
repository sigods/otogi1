# Contributing

Bug reports, corrections and pull requests are welcome.

## Two rules

1. **Never commit game data.** No disc image, XBE, extracted asset, or generated
   recompiled source (`src/game/recomp/gen/`). The `repo hygiene` CI job fails
   the build if one appears, and a pull request that adds one is rejected
   without discussion.
2. **Say what was observed.** A claim about what the game does is worth
   something only with the log line, memory read, or screenshot behind it.
   "It should work" is not a fix.

## Before you start

- Read [`AGENTS.md`](AGENTS.md). It holds the hard rules and the pipeline gates
  that fail silently.
- Read [`docs/BOOT_LOG.md`](docs/BOOT_LOG.md). It records what was tried and
  what it produced, including the dead ends. Repeating a dead end is the most
  common wasted evening here.
- Read [`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md) for the current blockers,
  and [`docs/INDEX.md`](docs/INDEX.md) for which document covers your area.
- Long-form build, run and record instructions:
  [`docs/RUNNING.md`](docs/RUNNING.md).
- The build needs `src/game/recomp/gen/`, generated from your own disc by
  `regen.sh`. It is not in the repository and never will be.

## Pull request checklist

- [ ] No game data, disc image, XBE, or generated recompiled source
      (`src/game/recomp/gen/`) is included.
- [ ] Every claim is backed by a log line, a memory read, or a screenshot.
- [ ] `docs/BOOT_LOG.md` was appended to, never rewritten, if runtime behaviour
      changed.
- [ ] Docs were updated if a switch, a milestone, or a build path changed.
- [ ] The pull request says where the fix belongs: `config/seed_functions.json`,
      `src/game/recomp_manual.c`, or the `xboxrecomp` toolkit.

## Commit messages

- One finding per commit where possible; the message states the symptom and the
  fix, e.g. `boot: stub XAPI quick-reboot; reach CRT startup`.
- Never commit generated code, disc data, or build output.
- Keep `docs/BOOT_LOG.md` append-only; corrections go in new entries.

## Reporting a bug

A useful report contains:

- the platform and build path (Linux native, MinGW cross-build, or MSVC);
- the `RECOMP_*` switches you set;
- the exact stderr lines, not a paraphrase;
- the RVA or target address, if the log has one;
- what you expected and what happened instead.

Do not attach game data or disc images to an issue. Check
[`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md) first — it may already be listed.

## Working with an AI agent

Point it at `AGENTS.md` first, then require evidence for every claim: a log
line, a memory read, a screenshot. An agent that cannot run the game will
reliably describe what it expects to happen; that is not the same as what
happened. `CONTRIBUTORS.md` describes how that rule was applied to this
project's own history.

## Credits

See [`CONTRIBUTORS.md`](CONTRIBUTORS.md) for the maintainer, the AI assistance
disclosure and the upstream projects this builds on.
