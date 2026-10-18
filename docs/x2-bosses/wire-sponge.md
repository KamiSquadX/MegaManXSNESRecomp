# Wire Sponge (X2) — source trace for Boss Rush

First X2 Maverick in Boss Rush. This is a trace of the original USA X2 ROM
(headerless, normalized SHA-256 starting `f3246755`). Addresses are SNES LoROM
addresses in that ROM. Nothing here is ROM data; the port reads his code, art,
palettes and sound from the user's own X2 ROM when the X2 Mavericks pack
loads, per [user-supplied source ROMs](../mod-source-roms.md). The shared
relocation machinery is described in
[the X2/X3 foundation](../x3-bosses/foundation.md).

## How X2 finds its enemies

| Item | Value |
| --- | --- |
| Enemy list | `$0D18`–`$10D8`, 64-byte slots, class at `+$0A` |
| Enemy dispatch | `$80:D221`, table `$FB8E` |
| Enemy-projectile list | `$1318`–`$1518`, table `$FA37` |
| Effect list | `$1818`–`$1D18`, 32-byte slots, `JSR ($F653,x)` |
| Damage rows | `$86:F3A4` (X1: `$86:EF37`) |
| Animation directory | `$2F:A000` + set×3 (pointer + bank byte); advance `$08:C673`, start `$08:C690` |
| Resource records / palette lists | `$86:FA01` / `$86:817A` (colors in bank `$85`) |
| Pose layouts | `$8D:8000` (as X3) |
| SPC upload table | `$09:8000` (as X1/X3), driver entry `$2C0` |

The player (`$09D8`), camera (`$1E5D`/`$1E60`), last-hit class (`$1F2F`), hit
direction (`$1F2D`) and scene flag (`$1F1D`) match X3, and data bank `$86`
is the same convention, so X2 code takes the same X1 patches as X3 code.

## Wire Sponge

| Item | Value |
| --- | --- |
| Class / controller | `$23`, `$04:9A29`–`$A9CD` |
| Rush roster id | 13 |
| Weapon byte | `$1FC5` Strike Chain |
| Art | sprite set `$3E`, resource `$45`, palette list `$9E` (48 colors; the first 16 are his green body) |
| Sound | SPC bank `$58`, effects `$15`, `$21`, `$29`, `$2B`, `$72`, `$73`, `$74` |
| Children | projectile class `$10` → `$03:D5C9` (vine and lightning bolts); effect class `$1F` → `$03:ECBE` (whip segments); effect class `$2B` → `$01:F068` (split halves, variants `$52`/`$53`, table `$86:EB38`) |
| Profiles (`+$28`) | `$09` normal (Sonic Slicer `$0B`/`$14` is the weakness), `$05` flash, `$03` init; children `$03`, `$02`, `$0A` (deflects everything but Sonic Slicer) |

**Spawn.** X2's class init `$00:DA22` also copies his graphics slot's OBJ
attributes from `$7F:8300` into `+$11`. Rush has no X2 slot table, so the
init shim writes `$29` (priority 2, palette 4) as Rhino's own init does:
with 0 he drew behind the arena and counted as a permanent hit flash
(found in the first playtest, 2026-10-18). His children copy `+$11` from
him.

**Entrance.** He drops from the top of X2's 256-pixel room
(`cam_y & $1F00`, read at `$04:9ADB`) while his vine lowers: the segment
counter `$7F:CD90` counts down every 6 frames during the fall, and the vine
child writes `$80` into his `+$0F` to release intro step 6. The Rush arena has
a ceiling there, so Rush answers that read with the arena top plus `$20`
(`MMX_SPONGE_ENTRY_Y`).

His vine waits for the counter to reach 0 before it climbs to him. Once he
has landed and his landing pose has finished (`+$0F` bit 6), he waits for
the vine's `$80` in `+$0F`. The vine's store replaces all of `+$0F`, so it
must not arrive before that. In X2 the long fall lowers all 11 segments
(one every 6 frames, 66 frames) before he lands. Rush's fall takes 62–66
frames: a 62-frame fall left one segment and the vine waited forever
(playtest 2026-10-18). A Rush poke at his landing (`$04:9BD5`) now lowers
the rest, which keeps the vine's arrival after his landing pose. Lowering
segments faster instead brought the vine too early and stuck him at his
landing. The fixture shortens his fall by 24 pixels to cover this.

**Death.** `+$3D` holds the last hit's class. At `$04:A70C`, `CMP #$0B`
picks death path `+$3B` = 6 (Sonic Slicer: he falls, lands, and splits into
two class-`$2B` halves) or 2 (the ordinary explosion). Both end in X2's stock
death scene `$2A:BFCB`, replaced by the `stockdeath` shim (wait 60 frames,
then hand back to Rush).

## Boss Rush behavior

- **Weakness.** X2's Sonic Slicer (X2 weapon pack, page 1) is his weakness:
  3 damage and a flinch. X1's Fire Wave, which his relocated X1 row would
  otherwise treat as the weakness class `$0B`, is neutralized to ordinary
  damage.
- **Zero's saber splits him too** (owner request). Saber hits from every
  Zero (X3 Zero's `$5A53` slash, Modern Zero's swing, Saber Zero's attack and
  wave families) report `MMX_RUSH_SABER_MARKER` as their reaction class; the
  death test at `$04:A70C` then reads Sonic Slicer's class, so a saber kill
  takes path 6.
- **Own death sequence.** While `+$3B` = 6 Rush lets his controller run for
  180 frames (fall, land, halves part) before its own explosion and
  cleanup.
- **Scratch RAM.** `$7F:CD90`–`$CD96` (vine state) is swapped per seat.

## Verified (2026-10-18)

`MMX_BOSS_RUSH_SPONGE_TEST=1 MMX_BOSS_RUSH_X2_ROM=<X2 ROM>`, standalone and
through the launcher for X, Zero, Modern Zero and Saber Zero
(`MMX_BOSS_RUSH_RHINO_CHARACTER`):

| Character | Basic attack | Sonic Slicer | Fire Wave | Ordinary kill | Slicer kill | Saber kill |
| --- | --- | --- | --- | --- | --- | --- |
| X | 1 | 3, flinch | 1 | explodes | splits | — |
| Zero | 2 | 3, flinch | 1 | explodes | splits | — |
| Modern Zero | 3 (swing) | 3, flinch | 1 | splits (swing) | splits | splits |
| Saber Zero | 2 | 3, flinch | 1 | explodes | splits | splits |

All seven of his sound effects render from his own driver and bank, and his
intro fills the 32-HP meter. He is offered to normal Boss Rush runs once the
X2 Mavericks pack has loaded the X2 ROM.

## Not done

- The lightning attack's screen darkening is dropped.
- His alternate palettes are not used.
- Extra X1/X2/X3 weaknesses come once all 24 bosses exist.
