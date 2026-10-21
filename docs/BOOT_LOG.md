# Boot Log — append-only findings ledger

Rules: append only. One entry per finding. Always include the exact stderr line,
the RVA, and what you changed. Never rewrite history — add a correcting entry.

Template:

```
## YYYY-MM-DD — <short symptom>
- Phase: CRT / hardware init / assets / menus / gameplay
- Symptom: <exact stderr line(s)>
- RVA / target: 0x........
- Diagnosis: ICALL miss | bad memory access | unimplemented kernel fn | bad vtable | other
- Fix: file + change (or "upstream: xboxrecomp issue #N")
- Result: <new state; next blocker>
```

---

## 2026-10-19 — project scaffold created

- Phase: pre-boot
- Status: scaffold only. No XBE processed yet.
- Next: run `./regen.sh`, capture Step 2 constants, fill `src/game/main.c`.
