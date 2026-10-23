# Add Axl (X8 Axl 0.0.1)

Axl from Mega Man X8 16-bit (Alysson da Paz; Zashiko / TagTeam mod), with
his original sprites. Turn on **Add Axl** under **Characters** in the
launcher. It can be on together with **Add Zero**; it turns off the co-op
modes.

Axl plays on X's body: X's physics, collision, health, upgrades and weapons
stay native, and Axl changes the art and the moveset.

## Controls

These are the KEY CONFIG actions (see `key-config.md`).

| Action | Axl |
| --- | --- |
| CHANGE PLAYER (Select) | Switch character. With Solo Team select: between the starting and partner characters. Without it: X and Axl, or X, Zero and Axl in turn when Add Zero is on too. X and Axl share health and weapons; Zero keeps his own. |
| SHOT | Rapid fire while held; Axl never charges. |
| d-pad while shooting | Aim in 8 directions. On the ground he plants his feet and the d-pad only aims and turns him; in the air he aims while he moves. On a wall he stops sliding while he shoots and aims in the five directions up, up-away, away, down-away and down, never into the wall; while SHOT or SHOT2 is held he cannot let go of the wall (jumping still works). |
| SHOT2 | No weapon equipped: the copy shot, a purple ball that does 2 damage. With a weapon: that weapon's full charge at once (Chameleon Sting's invincibility, Rolling Shield's orb, ...), at its charged energy cost, with or without the arm parts. |
| JUMP again in the air, held | Hover for 2 seconds, drifting slowly (Up/Down raise or lower him). Still holding jump after that, he sinks slowly until he lands. |
| DASH in the air | Dash in any of 8 directions, once per jump; every direction dashes for the same short time and then stops (a downward dash also stops when it lands). Up, up-left or up-right + DASH on the ground dashes upward. A level ground dash is X's. |
| Down + DASH | Dodge roll: nothing can hit Axl while he tumbles, and he flashes to show it; the crouch and rise at its end can be hit. Holding Down while dashing (also a double-tap dash) rolls too. |
| ARMOR ABILITY (R2) | GigaCrash, with a full bar (the purple meter left of the health bar). Axl stands still and cannot be hurt; a 0.75 s intro, then for 2 seconds a hitbox over the whole 4:3 screen hits each enemy once: 32 damage, 8 to bosses. The bar refills over about a minute. |

On the title, when Axl starts, he stands at the menu cursor and fires three
pistol shots instead of the charge shot.

Axl beams into a stage as X8 16-bit's intro, leaves it in X8's finish, and
CHANGE PLAYER to or from Zero carries his art in the beam. He dies in X8's
hurt pose. The health bar's badge is X8 16-bit's A, and the pause menu shows
his X8 portrait and life head. Boss doors, ladders,
the dodge roll's flashes and the hurt blink never show X's body. His shot
pose keeps the aim of its shot, and the pistol bullet and his up and down air
dashes use turned copies of X8's art.

## Solo Team select

**Solo Team select** (Characters) picks the **Starting character**, who
starts the game and appears on the title and menus, and the **Partner
character** CHANGE PLAYER brings in. It is on exactly while Add Zero or Add
Axl is (its checkbox is locked), and each list offers X plus the characters
that are added; the one the other option holds is greyed out too. Partner
**None** plays only the starting character: CHANGE PLAYER does nothing.

## X / Zero / Axl Co-op 0.0.2

Player 1 and Player 2 each choose X, Zero or Axl; a character one player
holds is greyed out for the other. Axl is not playable in co-op yet: a team
with Axl starts as X and Zero.

## Notes for this version

- Only the pistol and copy shot aim in 8 directions; special weapons fire
  forward. His gun art does not change with the weapon yet.
- GigaCrash plays X8 16-bit's start and blast sounds; the scene behind the
  sprites darkens and its streak burst covers the 4:3 screen.
- Axl's own X8 weapons are not ported. In X8 the bar refills from hits; here
  it refills over time.
- Ordinary enemies take GigaCrash's damage directly. Bosses take their hits
  through their own tables, so Axl adds the rest of the 8 after the boss's
  own hit, leaving the finishing blow to the boss's code.

## Implementation

- `src/mmx_axl.c`: the moveset. Input runs before the frame; hover, air dash
  and the GigaCrash stance run in the player hooks Zero's code already calls
  (`MmxZeroSetBodyHooks`); shots and animation after the frame. The state is
  in save chunk v26.
- `src/mods/mmx_axl_plugin.c`: the launcher plugins `megaman-x.axl` and
  `megaman-x.team`.
- `src/mmx_renderer.c`: the Axl provider replaces X's body and armor pieces,
  draws Axl's shots and his bar, and stands him on the title.
- GigaCrash's box is four bytes of free ROM padding at `$86:FFF0`, written only during the attack: the bytes lie inside Saber Zero's ground hitbox window (`$86:FFD8`), which he installs only into empty padding.
- Art: `assets/axl` (with `CREDITS.md` and the X8 16-bit license). The
  sidecar is built by `cmake/axl.cmake` from `tools/axl/axl_manifest.json`.
  Regenerate the manifest with
  `tools/axl/make_axl_manifest.py --x8 <X8 16-bit source>`.
- Test: `MMX_AXL_TEST=1 mmx_state_tests.exe mmx.sfc` (ctest `mmx_axl`).
  `MMX_AXL_ZERO_ASSETS=<x3-zero cache>` adds the Axl and Zero team checks.
