# Frequently asked questions

Short answers. Each one points at the document that has the detail.

## Is this playable?

No. It boots the retail XBE, plays the attract video and BGM, and draws 2D UI,
then stops at the title screen. See [`../README.md`](../README.md) § Status and
[`../ROADMAP.md`](../ROADMAP.md).

## Do I need the game disc?

Yes. Supply your own legally obtained Xbox disc image; `regen.sh` reads its
`default.xbe`. No game data is distributed here — see
[`../README.md`](../README.md) § Requirements.

## Which disc region?

The USA image is what this project was developed against. It carries a video
partition in front of the filesystem, which is why the toolkit's own reader
(`python3 -m tools.xiso`) is required to unpack it. Other regions are not
documented in this repository.

## Why is there no 3D?

The guest's GPU command stream is decoded and the 2D path rasterises, but no 3D
scene is drawn. [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md) has the current state.

## Does it run on Steam Deck?

Not documented here. The verified path is Linux x86-64 with SDL2 and libepoxy,
and nothing in this repository records a Steam Deck run.

## Why is the generated C not in the repository?

It is an output of `regen.sh`, derived from the game's own `default.xbe`, so
shipping it would ship the game. See [`../CONTRIBUTING.md`](../CONTRIBUTING.md)
§ Two rules.

## Is an emulator involved?

No. The game's own x86 is translated to C and compiled natively: no CPU
emulator and no interpreter. [`../AGENTS.md`](../AGENTS.md) § 1 is explicit
about this.

## How much of this is AI-written?

Substantial AI assistance, with every change and claim reviewed and verified by
the maintainer. See [`../CONTRIBUTORS.md`](../CONTRIBUTORS.md).

## Why Linux first?

Linux x86-64 is the primary and verified path; the MinGW-w64 cross-build is the
verified Windows path, and MSVC 2022 is supported but has not been run. Both
platforms are covered in [`../README.md`](../README.md) § Requirements.

## Can I contribute without reverse-engineering experience?

This repository does not give an entry requirement in those terms. What every
contribution does need is evidence: a log line, a memory read, or a screenshot
behind each claim. Start with [`../CONTRIBUTING.md`](../CONTRIBUTING.md).

## Where do I start reading?

[`INDEX.md`](INDEX.md), then [`../AGENTS.md`](../AGENTS.md).

## Something crashes. Is it already known?

Check [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md) first, then search
[`BOOT_LOG.md`](BOOT_LOG.md): it records dead ends so they are not repeated.
