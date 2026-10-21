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
- The build needs `src/game/recomp/gen/`, generated from your own disc by
  `regen.sh`. It is not in the repository and never will be.

## Working with an AI agent

Point it at `AGENTS.md` first, then require evidence for every claim: a log
line, a memory read, a screenshot. An agent that cannot run the game will
reliably describe what it expects to happen; that is not the same as what
happened. `CONTRIBUTORS.md` describes how that rule was applied to this
project's own history.

## Credits

See [`CONTRIBUTORS.md`](CONTRIBUTORS.md) for the maintainer, the AI assistance
disclosure and the upstream projects this builds on.
