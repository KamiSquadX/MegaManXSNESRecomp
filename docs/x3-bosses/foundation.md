# X2/X3 Mavericks in Boss Rush — foundation

Goal: all 24 Mavericks (X1, X2, X3) in Boss Rush, then later the full X2 and
X3 stages inside the X1 recomp, with L/R on the stage-select map switching
between the three games' maps so every Maverick can be fought in any order.
This document covers the Boss Rush foundation and the execution plan that
should carry over to full stages. Rhino's source trace:
[tunnel-rhino.md](tunnel-rhino.md).

## Roster (implemented)

- Roster ids: `0..7` X1 (unchanged order), `8..15` X2, `16..23` X3, each game
  ordered by its native weapon id (`kMmxBossRushBosses` in
  `src/mmx_boss_rush.c`). X2 controllers are not traced yet.
- `MmxBossRushRosterState` holds the shuffle queue for up to 24 bosses and
  the run's **available** set. The available set is copied from the offered
  set when a run starts, so launcher edits cannot change a run in progress
  or make netplay peers diverge.
- The original `MmxBossRushState` layout is unchanged; its legacy 8-entry
  queue stays empty for new runs. Older snapshots and render captures load
  as before.
- Game snapshot chunk **v23** appends the roster after the v22 view bounds.
  Loading v18–v22 imports the legacy X1 queue as an X1-only roster. A loaded
  run must contain both present bosses in its roster.
- HUD initials exist for all 24 bosses.

## Packs and source ROMs (implemented)

- Package `megaman-x.boss-rush.x3-bosses` (Game Modes, off by default) asks
  for the X3 USA ROM using the shared `megaman-x.source.x3` selection.
  An X2 package uses the same plugin (`megaman-x.boss-rush.x2-bosses`) and
  will be added with the first X2 boss.
- `src/mods/mmx_boss_rush_foreign_plugin.c` validates the ROM with the same
  checksum rules as the other packs (`MmxSourceRomLoad`) and keeps the image
  **in memory only**.
- `src/mmx_boss_rush_foreign.c` offers a boss to new runs only when its pack
  is loaded **and** its body is implemented. No X2/X3 body exists yet, so
  runs stay X1-only even with the pack enabled.
- Netplay refuses to start when an enabled Maverick pack failed to load.

## Running X2/X3 boss code (next)

X1 and X3 share one engine generation. Every helper Rhino calls has a direct
X1 counterpart with the same object layout:

| X3 | X1 | Purpose |
| --- | --- | --- |
| `$04:B967` | `$84:8F07` | start animation |
| `$04:B94A` | `$84:8EEA` | animate and draw |
| `$04:C0F7` | `$84:91BE` | terrain collision |
| `$04:CB74` | `$84:9B43` | take player-shot damage |
| `$04:CB31` | `$84:9B03` | contact damage |
| `$04:BE35` | `$84:9086` | RNG |
| `$02:D636` | `$82:80B4` | off-screen cull |
| `$02:D8DE` | `$82:8358` | allocate projectile slot |

Two ways to bring a Maverick over:

1. **Relocated original code (recommended).** At activation, build an
   extension bank from the user's ROM: copy the boss controller, children
   and data tables, and rewrite JSL/JML targets, RAM addresses and data-bank
   references through a per-boss, address-only relocation table. The X1
   interpreter then runs the boss natively. Damage, weapon weaknesses, co-op,
   snapshots, rollback and netplay keep using X1's own paths. Per-boss work
   is a relocation table plus shims for X3-only helpers, instead of a full
   rewrite. The same machinery is what full X2/X3 stages would need for
   hundreds of enemies, so it is the only approach that scales to that goal.
2. **Hand-written C bodies.** No cartridge changes, but each boss's AI,
   physics and damage handling is reimplemented, roughly 2–4 KB of 65816 per
   boss, with drift risk and duplicated engine logic.

**Feasibility spike before committing to (1):**

- Can the cart image be extended at activation without changing the
  original game's mirroring? Findings so far: `snes_loadRom` pads X1's
  1.5 MB image to 2 MB by mirroring its last 512 KB, and LoROM banks
  `$C0`–`$FF` map to image offsets 2–4 MB (`cart.c`, `bank & $7F`), which X1
  never reads. A 4 MB image whose upper half mirrors the original 2 MB keeps
  every existing read unchanged; extension code can then occupy banks
  `$F0`–`$FD` (`$8000`–`$FFFF`). The framework already grows images in
  memory for IPS/BPS content variants (`rom_patch.h`).
- Does the interpreter execute and hook code in those banks? AOT coverage
  only exists for X1 banks; uncovered code runs through the interpreter.
- Can a Rush seat's class dispatch (`$80:D544`, `JMP ($F8DD,x)`) be
  redirected to an extension-bank controller with a pre-opcode hook?
- Do the build identity and netplay checks accept a derived image that both
  peers construct identically from the same verified ROMs?

If the spike fails, fall back to (2) using the same roster, packs and seat
ownership already in place.

## Remaining foundation pieces

- Foreign body spawn in a Rush seat (today a non-X1 id is skipped).
- Private art and palettes for foreign bodies in the renderer (decode the
  source resource once from the in-memory ROM, as X1 bosses do).
- Foreign sound effects through the isolated SPC renderer, using the source
  game's boss sound bank.
- Weakness rows for foreign bodies in `src/mmx_weapon_weakness.inc`
  covering X1, X2 and X3 weapons.
