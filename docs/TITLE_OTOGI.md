# Title: Otogi: Myth of Demons

## Confirmed

| Field | Value | Source |
|---|---|---|
| Title ID | `0x46530002` (FS-002) | xemu title DB |
| Developer | FromSoftware | Wikipedia |
| Publisher | JP: FromSoftware; NA/EU: Sega | Wikipedia |
| Released | JP 2002-12-12; NA 2003-08-26; EU 2003-09-05 | Wikipedia |
| Genre | Action RPG / hack-and-slash | Wikipedia |
| Xemu status | Playable, minor issues (community report, last tested 2024-06-29) | xemu title DB; [xemu compatibility page](https://xemu.app/titles/46530002/) |
| Attract video | Plays — the title's own XMV decoder | `README.md` § Status |

## Unknown — determine empirically, then update this file

- **Engine.** The toolkit's candidate list names no engine for Otogi. It is
  *not* RenderWare. Working hypothesis: a FromSoftware custom engine (shared
  with Otogi 2). **Confirm at Step 3** by inspecting the `func_id` categories
  and static libraries; record what you find.
- **XDK version.** Read from `tools.xbe_parser` output (Step 2).
- **.text size and function count.** From Step 3 stats. Typical Xbox title:
  10k–25k functions.
- **Kernel imports used** (out of 147). From Step 2.
- ~~XMV / video usage.~~ **Answered:** the title's own XMV decoder plays the
  looping attract video, and the disc extracts with a `movie/` directory. The
  toolkit's host video player is a POSIX stub and is not required.
- **Xbox Live dependency.** Otogi 1 is single-player; confirm nothing in the
  boot path requires Live.
- **Backward-compatibility status.** Otogi 2 is BC on Xbox One/Series; Otogi 1's
  status is **unverified**. Check before long-term investment — if Otogi 1 is in
  the BC library it may arrive officially on PC via the expanding BC-on-PC
  program, which changes the preservation calculus.

## Reference / assets

- Hidden Palace holds a **May 16, 2003 English localization prototype** —
  useful for diffing build behaviour, not for shipping.
- Xemu is the differential oracle. Boot the same scene in xemu under GDB.
