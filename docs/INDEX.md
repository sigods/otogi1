# Documentation Index

Start here, then follow the order.

| Doc | Read it when |
|---|---|
| `../AGENTS.md` | Always, first. |
| `TITLE_OTOGI.md` | Before touching anything — title facts and open unknowns. |
| `PIPELINE.md` | Running or re-running the toolchain. |
| `ARCHITECTURE.md` | You need to know which layer owns a behaviour. |
| `BOOT_LOG.md` | You hit a crash — search it before debugging from scratch. |
| `RUNNING.md` | Building, running, and recording the executable. |

By symptom:
- "Crash in ICALL dispatch" → `BOOT_LOG.md`, then AGENTS.md §6.
- "It builds but signatures look wrong" → `PIPELINE.md` Step 4.5.
- "Build OOM-killed" → `PIPELINE.md` Step 5.
- "Where do I put my fix?" → `AGENTS.md` §2.
