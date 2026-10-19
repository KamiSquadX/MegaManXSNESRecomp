# Controls: modern pad, KEY CONFIG and Option Mode

The game now reads a modern controller's extra inputs and lets you assign every
action from the title screen, using the list and defaults from Alysson da Paz's Mega Man X8 16-bit demake.

## Launcher: Controls

Beside the SNES twelve, each player's **Input bindings** now include:

| Row | Default keyboard key | Default pad input |
| --- | --- | --- |
| L2 | Q | Left trigger |
| R2 | E | Right trigger |
| L3 | (unbound) | Left stick click |
| R3 | Keypad 5 | Right stick click |
| R.S. Up / Down / Left / Right | Keypad 8 / 2 / 4 / 6 | Right stick |

L2, R2, L3 and R3 form a fourth column; the right stick's four rows sit below
it, beside **Auto Map All** and **Reset to Defaults**. Each row is mapped with
the player's input source like any other row. The keyboard keys are stored in
`keybinds.ini` (`l2 r2 l3 r3 rs_up rs_down rs_left rs_right`); the pad inputs in
`config.ini` `[GamepadMap]` as `ControlsExt` / `ControlsExtP2`.

## Title screen: KEY CONFIG

**KEY CONFIG** sits between OPTION MODE and BOSS RUSH. It lists the X8
demake's twenty actions with the control each one uses, in X8's names
(*Face Button Left*, *R*, *L2*, *Select*, *Right Stick Up*, ...):

| Action | Default | Action | Default |
| --- | --- | --- | --- |
| left / right / up / down | DPAD | select buster | R3 |
| shot | Face Button Left | wheel left / right | Right Stick Left / Right |
| shot2 | Face Button Top | wheel up / down | Right Stick Up / Down |
| jump | Face Button Bottom | pause menu | Start |
| dash | Face Button Right | menu confirm | Face Button Left |
| change player | **Select** | cancel | **Face Button Bottom** |
| armor ability | R2 | prev / next weapon | L / R |

These are X8's defaults, except change player (Select, the recomp's
long-standing swap button) and cancel (Face Button Bottom). There is no
keyboard column: the launcher already maps keys onto these pad controls.

Move with the D-pad, press **menu confirm** (or Start) on a row and then the
control to assign. **DEFAULT** restores the list above, **EXIT** or **cancel**
returns to the title. The choices are saved in `key_config.ini`.

Face Button Left (Y on the SNES layout) is both shot and menu confirm, so on
menus it confirms like Start does. Actions that X1 itself has no use for yet
(shot2, armor ability, select buster, the weapon wheel) are delivered to the
game for upcoming characters such as Zero by KamiSquad_X. Key mapping happens on
your machine before input is sent, so online partners and replays see the
actions, not your bindings.

## Title screen: OPTION MODE

X1's **KEY CONFIG.** section is replaced by two options:

* **DOUBLE TAP DASH** (default **ON**, as in X1). **OFF** stops a double tap of
  left or right from dashing; only the dash action dashes.
* **HOLD DASH** (default **OFF**, as in X1). **ON** makes a wall kick a dash
  wall kick whenever dash is held, as in X2 onwards. Off, X1 needs dash
  pressed together with the kick.

Change them with left/right or confirm. They are saved in `mmx_options.ini`.
Online sessions start both players from the defaults so the game stays in sync;
changing them in Option Mode during a session applies to both.
