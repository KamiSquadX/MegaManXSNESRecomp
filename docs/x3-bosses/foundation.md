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

## Feasibility spike (passed, 2026-10-17)

Approach (1) is viable. `src/mmx_cart_extension.c` and the Rush dispatch hook
implement it; `MMX_BOSS_RUSH_FOREIGN_SPIKE_TEST=1` with `MMX_BOSS_RUSH_TEST=1`
checks it against the USA ROM.

- **Cart extension.** `snes_loadRom` pads X1's 1.5 MB image to 2 MB, and
  LoROM banks `$C0`-`$FF` map to image offsets 2-4 MB (`bank & $7F`), which
  X1 never reads. `MmxCartExtensionEnsure` grows the image to 4 MB with the
  upper half mirroring the original 2 MB, so every existing read is
  unchanged, and replaces both `cart->rom` and `g_rom`. `romImageSize`, and
  therefore the image identity hash, is unchanged. Writes are limited to
  banks `$F0`-`$FD`, `$8000`-`$FFFF`, and invalidate the interpreter's
  decode cache.
- **Dispatch.** X1's enemy loop (`$80:D4EA`/`$D507`) already runs in the
  interpreter while Rush is active, so a pre-opcode hook at the class
  dispatch `$80:D544` (`JMP ($F8DD,x)`) fires without regenerating code.
  For a seat whose roster id has a registered controller, the hook pushes a
  JSL frame returning to an existing native `RTS` (`$80:F8C3`) and redirects
  into the extension bank, which is exactly what a stock `JSL; RTS` stub does.
- **Measured:** a bank-`$F0` controller (`INC $3E; JSL $84:9086; STA $3F;
  RTL`) for roster id 23 in seat 1 ran exactly once per frame for 60 frames.
  Its native RNG call returned into the extension bank every frame. A
  native Penguin kept running in seat 0. A 30-frame snapshot replay
  matched all 128 KB of WRAM byte for byte, and clearing restored the
  mirrored bank. The full 28-pair Rush suite still passes.
- **Not yet covered:** netplay between two peers with an extended image
  (both peers build identical bytes from the same verified ROMs, and the
  image identity is unchanged, but this is untested); renderer captures of
  foreign bodies; any real relocated X3 code.

## Tunnel Rhino relocation (running, 2026-10-17)

Rhino's original X3 code runs inside X1's engine. `tools/relocate_boss.py`
traces his body, his drill projectile and the X3 helpers they need, then
writes `src/mmx_boss_rush_x3_rhino.inc`: 1,326 traced instructions and 183
rewritten operands, as addresses plus the original operand values.
`src/mmx_boss_rush_relocate.c` rebuilds him from the user's X3 ROM when the
pack loads.

**Layout.** X1 uses data bank `$86` for its enemy loop, the same convention
as X3, and X1's damage, terrain and overlap helpers read hitboxes through
that bank. Rhino therefore runs with X1's `$86`:

| Where | What |
| --- | --- |
| `$3F` (X3 `$3F`, same canonical bank) | Rhino's controller, X3 animation sets/records |
| `$33`, `$38`, `$34`, `$32` (X3 `$13`, `$08`, `$04`, `$02`) | palette helpers, drill class `$26`, X3 animation/motion/overlap helpers |
| `$86:FA80`-`$FAFF` | shims (`RTL`, finished fade, resume combat, delete, class init) |
| `$86:FB00`-`$FBB2` | his `$86` tables: body/drill hitboxes, drill spawn tables, angle table |
| `$86:FE00`-`$FFFF` | zero/sink area for X3-only globals |

All copied banks sit in X1's unused `$30`-`$3F` padding; the `$86` tail is
`$FF` padding in the USA ROM. Clearing restores both.

**Classification** (anything else fails generation):

- *X1 helpers:* cull `$82:80B4`, terrain `$84:91BE`, RNG `$84:9086`, shot
  damage `$84:9B43`, contact `$84:9B03`, projectile slot `$82:8358`, face
  player `$84:ACA5`.
- *Relocated X3 helpers:* animation start/advance (`$04:B967`/`$B94A`, so he
  reads X3's own animation sets), velocity (`$02:D7F3`/`$D78E`/`$D6F8`),
  hitbox overlap `$04:CC5C`, flash timer `$02:DF64`, player-death guard
  `$04:D8D5`, his palette helpers `$13:F7D6`/`$F7FC`.
- *Shims:* music, SPC upload, sound effects, intro/pin player lock, screen
  shake, dust, stock explosion and weapon award, stun, Doppler graphics; the
  generic hurt reaction resumes combat; deletes clear the object; class
  init writes his sprite set, HP, contact damage and defense profile from
  the ROM's class row.
- *RAM:* X3's `$09xx`/`$0Axx` player block is X1's `+$1D0`; camera
  `$1E5D/$1E60`→`$1E4D/$1E50`; HUD boss `$1F1F`→`$1F0E`; the player object
  immediate `$09D8`→`$0BA8`. X3-only flags read zero and ignore writes:
  stage/defeated/music/last-weapon checks, state mirrors for children,
  OBJ palette buffer `$0480`-`$04BF`, palette backup `$7F:CAC0`.

**Dispatch.** Hooks at X1's enemy (`$80:D544`) and enemy-projectile
(`$80:D4E7`) class dispatches enter registered controllers for objects owned
by a foreign seat. Rush treats a foreign body as fighting at state `$01=2`
and dying at `$01=4`; defeat and cleanup use the normal Rush path.

**Measured** (`MMX_BOSS_RUSH_RHINO_TEST=1` with `MMX_BOSS_RUSH_TEST=1` and
`MMX_BOSS_RUSH_X3_ROM`): his intro drops in and fills his meter to 32, then
he chooses among his original charge, drill volley and idle behaviors,
charges wall to wall at 4 px/frame, and fires all three volley drills as
relocated projectiles. A native Penguin fights beside him. Replay is
byte-exact, Rush counts his defeat, and cleanup removes him and his drills.
Unloading the pack removes him.

**Art (done).** `src/mmx_boss_rush_foreign_art.c` decodes his compressed
OBJ resource (`$1D`, record `$86:F732`), his palette (list `$3E` at
`$86:8180`, 16 colors from `$8C:E628`) and rasterizes poses from the X3
layout table (`$8D:8000` + sprite set) once, from the in-memory ROM. The
renderer drops the pieces X1's sprite builder makes for foreign-owned
objects (X1 tables cannot describe an X3 set) and draws each foreign body
and projectile whole from its object's `+$16` set and `+$17` pose: bit 6 of
`+$11` faces right, bits 4-5 give OBJ priority, and palette 0 in bits 1-3 is
the native hit flash on live CGRAM. Foreign sprites draw before native
pieces, so players and their shots stay in front. The Rhino fixture checks
that his body and drills draw in his own palette and saves private
`rush-rhino-combat.ppm` / `rush-rhino-drills.ppm` captures.

X1's enemy-projectile actor (`$80:D4D3`/`$D4DD`, dispatch `$80:D4E7`) has a
compiled body, so `tools/apply_boss_rush_hooks.py` now routes it through the
interpreter during Rush; without that, X1's own projectile class `$26` ran
his drills.

**Not done** (why he is preview-only, `MMX_BOSS_RUSH_PREVIEW=1`):

- Sound: all effects are shimmed silent; bank `$5D` needs the isolated SPC
  path.
- Weaknesses: profile `$18` currently indexes X1's damage table; his
  Acid Burst flinch (`$1F2F`) is disabled.
- Pinning: X3 player-state values (`$12`, `$1E`, `$74`) are compared against
  X1 states, so the drill-charge pin never triggers.
- Palette effects (drill-charge flashing) are sunk.
- `$09CB`/`$09CC` frame-counter mapping (`+$1D0`) is unverified.
- Netplay with relocated bodies is untested; preview is a local setting and
  must not differ between peers.

## Remaining foundation pieces

- Foreign body spawn in a Rush seat: done for registered controllers (X1
  boss class byte `$02`, so class-indexed native tables stay in range).
- Private art for foreign bodies: done for relocated bodies (pose-level;
  bosses whose source art streams per-pose DMA will need that path).
- Foreign sound effects through the isolated SPC renderer, using the source
  game's boss sound bank.
- Weakness rows for foreign bodies in `src/mmx_weapon_weakness.inc`
  covering X1, X2 and X3 weapons.
