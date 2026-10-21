# Contributors

## Maintainer

**sigods** — author and maintainer. Every change in this repository was reviewed
and verified by the maintainer before being committed.

## AI assistance

Code, debugging and documentation here were produced with substantial AI
assistance. The AI did not review itself: each change and each claim in the
README and `docs/` was checked against observed runtime behaviour before it was
committed, and unverified work was left out rather than described.

## Upstream and third-party work

This project stands on other people's work:

- **[xboxrecomp](https://github.com/sp00nznet/xboxrecomp)** and its authors — the
  static recompilation toolkit, vendored at `third_party/xboxrecomp/` under the
  MIT license. Local changes are listed in
  `third_party/xboxrecomp/UPSTREAM.md`.
- **xemu** and its contributors — the MCPX APU and NV2A derived sources
  attributed in [`NOTICE`](NOTICE).
- **FromSoftware, Inc. and Sega Corporation** — *Otogi: Myth of Demons* itself,
  which this project recompiles. No game code or assets are distributed here.

## Contributing

Bug reports, corrections and pull requests are welcome. Two rules:

1. **Never commit game data.** No disc image, XBE, extracted asset, or generated
   recompiled source (`src/game/recomp/gen/`). The CI job `repo hygiene` fails
   the build if one appears.
2. **Say what was observed.** A claim about what the game does is worth
   something only with the log line, memory read, or screenshot behind it.

If you work with an AI agent on this repository, point it at
[`AGENTS.md`](AGENTS.md) first — it holds the hard rules and the pipeline gates
that fail silently.
