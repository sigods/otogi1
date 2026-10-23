# Documentation Index

Start here, then follow the order.

| Doc | Read it when |
|---|---|
| `../AGENTS.md` | Always, first. |
| `../ROADMAP.md` | Milestone status — what is done, partial, or not started. |
| `TITLE_OTOGI.md` | Before touching anything — title facts and open unknowns. |
| `PIPELINE.md` | Running or re-running the toolchain. |
| `ARCHITECTURE.md` | You need to know which layer owns a behaviour. |
| `BOOT_LOG.md` | You hit a crash — search it before debugging from scratch. |
| `RUNNING.md` | Building, running, and recording the executable. |
| `KNOWN_ISSUES.md` | Your symptom may already be listed as an open blocker. |
| `FAQ.md` | You are new and want the short answers first. |

By symptom:
- "Crash in ICALL dispatch" → `BOOT_LOG.md`, then AGENTS.md §6.
- "It builds but signatures look wrong" → `PIPELINE.md` Step 4.5.
- "Build OOM-killed" → `PIPELINE.md` Step 5.
- "Where do I put my fix?" → `AGENTS.md` §2.
- "Is my problem already known?" → `KNOWN_ISSUES.md`.
- "I want to run it without reading the pipeline" → `RUNNING.md`.
