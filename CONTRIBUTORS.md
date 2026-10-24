# Contributors

## Maintainer

**sigods** — author and maintainer. Every change in this repository was reviewed
and verified by the maintainer before being committed.

## AI assistance

Code, debugging and documentation here were produced with substantial AI
assistance. The AI did not review itself: each change and each claim in the
README and `docs/` was checked against observed runtime behaviour before it was
committed, and unverified work was left out rather than described.

How to work with an agent on this repository is in
[`CONTRIBUTING.md`](CONTRIBUTING.md).

## Upstream and third-party work

This project stands on other people's work:

- **[xboxrecomp](https://github.com/sp00nznet/xboxrecomp)** and its authors — the
  static recompilation toolkit, a submodule at `xboxrecomp/`. Most of it is
  MIT, but the parts derived from xemu are LGPL-2.1-or-later. Local changes are
  listed in `xboxrecomp/UPSTREAM.md`.
- **xemu** and its contributors — the MCPX APU sources and the NV2A register
  definitions, **LGPL-2.1-or-later**. [`NOTICE`](NOTICE) lists the affected
  paths, the copyright holders, and where the licence text lives.
- **FromSoftware, Inc. and Sega Corporation** — *Otogi: Myth of Demons* itself,
  which this project recompiles. No game code or assets are distributed here.
