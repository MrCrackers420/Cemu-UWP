# Racing wheel bridge (Thrustmaster T98) for Cemu UWP

**Status:** compiles and packages on GitHub Actions (`Cemu-UWP-Host_1.0.10.98_x64.msixbundle`), and the simulated logic tests pass there. **Not tested on a physical T98, an Xbox, or Mario Kart 8.** Every hardware test below is marked NOT RUN until someone runs it on the console.

Base: `XboxEmuPorts/Cemu-UWP` at `bbaf33a`. The Cemu core (`XboxEmuPorts/Cemu`, branch `build-msvc-d3d11`) is unchanged.

## What it does

- One `Windows.Gaming.Input.RacingWheel` is exactly one emulated Wii U player. Default Player 1, selectable 1–4 under **Settings → Racing wheel**.
- The wheel never shares a player with a gamepad and never creates a second player. If the Xbox also reports the wheel as a gamepad, that copy is ignored.
- Xbox gamepads fill the remaining players and keep their player when anything else connects or disconnects.
- Unplugging the wheel releases all of its inputs at once. Its player stays reserved, so no gamepad inherits it; plugging the wheel back in resumes the same player.
- No Xbox gamepad is needed: the wheel's D-pad, A and B drive Cemu's own interface.

## Default controls

| T98 input | Layer | Wii U control | Mario Kart 8 |
| --- | --- | --- | --- |
| Steering | Racing | Left stick (analog) | Steer |
| Accelerator pedal | Racing | A | Accelerate (also confirms in menus) |
| Brake pedal | Racing | B | Brake / reverse (also goes back in menus) |
| Right paddle | Racing | ZR | Drift / hop |
| Left paddle | Racing | ZL | Item |
| D-pad | Both | D-pad | Menu navigation |
| A / B | Menu | A / B | Confirm / back |
| X / Y | Menu | X / Y | |
| Menu | Both | + | Start / pause |
| View | Both | − | Back / minus |

Pause, resume, restart and quit a race from Mario Kart 8's own pause menu: **Menu** to pause, **D-pad** to choose, **A** to confirm.

### Binding layers

There are two complete binding tables, **Menu** and **Racing**, each mapping all 16 Wii U buttons to any wheel input (pedals, paddles, D-pad, A/B/X/Y, Menu, View, wheel buttons 1–16, steer left/right).

- **Menu and racing bindings together** (default): both tables are active at once. This is the table above.
- **Switch layers with a wheel button**: only one table is active. A title starts in the Menu layer; the switch button (default **Y**) toggles, with a notice on screen. In the Menu layer the pedals and steering do nothing.

### Steering values

| Setting | Default | Meaning |
| --- | --- | --- |
| Wheel travel for full steering | 70 % | Wheel rotation that gives a full stick |
| Steering centre deadzone | 2 % | Ignored travel around centre |
| Stick deadzone compensation | 25 % | Cancels Cemu's own 0.25 stick deadzone so small wheel movements steer |
| Pedal press point | 30 % | Pedal travel that counts as pressed |

## Cemu's own interface

The wheel's D-pad moves focus, **A** activates, **B** closes an open list or returns to the side bar. **Wheel navigation of this interface** chooses who does that:

- **Automatic** (default): if the Xbox already turns the wheel press into navigation, Cemu stays out; otherwise Cemu performs it about 0.1 s later.
- **Always handled by Cemu** / **Xbox system only**: force one or the other if Automatic navigates twice or not at all.

Which of the two the Xbox actually does for a T98 is unknown until tested on hardware.

## Logs

- `LocalState\t98_bridge.log`: wheel connect/disconnect, player assignment, moved gamepads, layer switches, read failures with HRESULT. Appended and flushed per line, never deleted; one `.old` generation above 1 MB.
- `LocalState\log.txt`: Cemu's own log.
- **Settings → Racing wheel → Live input** shows steering, pedals and every pressed wheel input, for checking the physical wheel.

Fetch both files with Xbox Device Portal → File explorer → `LocalAppData\<package>\LocalState`.

## Installing without losing saves

1. In Device Portal, copy `LocalState` from the installed Cemu package to a PC first.
2. Install the new `.msixbundle` **over** the existing app (Device Portal → Add, with `Microsoft.VCLibs.x64.14.00.appx` as a dependency if asked). Do not uninstall: uninstalling deletes `LocalState`.
3. The package keeps upstream's identity and signing key and is versioned `1.0.10.98`, above upstream's `1.0.10.2`, so it updates in place.
4. If Device Portal refuses the update (different signer or a higher installed version), stop and keep the existing install.

## Tests

### Simulated (run in CI and locally)

`tests/t98_bridge_tests.cpp` compiles the same `T98Bridge.h` that ships in the package: default mapping, pedal hysteresis, steering curve, both layer modes, wheel-only, wheel as P1 with gamepads P2–P4, wheel as P2, unplug/replug, no duplicate wheel, changing the wheel's player, five devices, interface navigation, and a randomized connect/disconnect sequence that checks no input ever reaches another player.

```
g++ -std=c++14 -Wall -Wextra tests/t98_bridge_tests.cpp -o t98_tests && ./t98_tests
```

### Hardware (all NOT RUN)

| # | Test | Result |
| --- | --- | --- |
| 1 | Wheel detected: Settings shows "… connected as Player 1"; Live input follows steering, pedals, paddles, D-pad, A/B/X/Y, Menu, View | NOT RUN |
| 2 | Navigate Cemu's interface with only the wheel: side bar, Library, confirm a game, Start game, Settings lists and sliders | NOT RUN |
| 3 | Start Mario Kart 8 with only the wheel (no gamepad powered on) | NOT RUN |
| 4 | Mario Kart 8 menus with only the wheel: character, kart, course, start | NOT RUN |
| 5 | Complete a race with only the wheel: steer, accelerate, brake, drift, item | NOT RUN |
| 6 | Pause, resume, restart, quit a race from the wheel | NOT RUN |
| 7 | Two players: wheel P1 + gamepad P2; each controls only its own kart | NOT RUN |
| 8 | Two players: gamepad P1 + wheel set to P2 | NOT RUN |
| 9 | Four players: wheel P1 + gamepads P2–P4; no fifth or phantom player | NOT RUN |
| 10 | Unplug the wheel mid-race: its kart stops receiving input, other players unaffected, no gamepad becomes P1 | NOT RUN |
| 11 | Replug the wheel: same player resumes | NOT RUN |
| 12 | Turn a gamepad off and on mid-race: it returns to its own player, wheel unaffected | NOT RUN |
| 13 | Layer switch mode: Y toggles, notice appears, pedals inert in Menu layer | NOT RUN |
| 14 | Update installed over the existing app; saves and settings intact | NOT RUN |

For any failure keep `t98_bridge.log` and `log.txt` from the same session.

## Known limits

- One wheel. A second wheel is ignored.
- No force feedback or rumble on the wheel.
- Pedals are buttons (Mario Kart 8 accelerates digitally).
- Host options stay hidden while a title runs (upstream behaviour), so wheel settings are changed before starting a game.
- If the wheel first appears while a gamepad is using its player in a running game, the wheel waits until the game stops, then the gamepad moves to the next free player.
