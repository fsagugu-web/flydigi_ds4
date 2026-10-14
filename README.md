# flydigi_ds4

**Use your Flydigi Direwolf 2 (USB) as a DualShock 4 in jailbroken PS4 games.**

A [GoldHEN](https://github.com/GoldHEN/GoldHEN) plugin that reads the controller over USB and hands its input to the game as if it came from a DS4. It works **with the DS4 turned off**, needs no adapter, and installs with FileZilla in a few minutes.

> 🇧🇷 **[Leia em português](README.pt-BR.md)**

---

## Why this exists

The PS4 only accepts controllers that identify as a DualShock 4 and pass Sony's cryptographic check. PC controllers (Flydigi, generic pads, 8BitDo…) speak "Xbox" and are simply ignored. The usual fix is buying an adapter.

This plugin takes another route: it runs **inside the game**, reads the controller straight from USB and gives the buttons to the game, which thinks it is talking to a DS4. Because the PS4 system never sees the controller, there is no auth check and no 8-minute timeout for unlicensed controllers.

## Features

- ✅ All buttons, sticks and analog triggers
- ✅ Rumble
- ✅ Works with the DS4 turned off, or alongside it (both at once)
- ✅ Plug and unplug the cable while the game is running
- ✅ Settings file: button remapping (including combos), deadzones, triggers, rumble
- ✅ Per-game settings
- ✅ Touchpad: click, or swipe with the right stick
- 🧪 Gyro (experimental) for games with motion aiming or motion camera
- ✅ On-screen notifications (connected, disconnected, mistakes in the settings file)

## Compatibility

| | Status |
|---|---|
| **Console** | Jailbroken PS4 with GoldHEN 2.4 or newer (with the plugin loader) |
| **Tested on** | Firmware 12.52 + GoldHEN v2.4b18.7 |
| **Flydigi Direwolf 2** (USB cable) | ✅ Tested |
| **Other XInput controllers** (generic, 8BitDo, GameSir…) | Should work, but untested. [Tell us how it went](#controller-not-working) |
| **Xbox One / Series controllers** | ❌ Not supported (different protocol) |
| **Bluetooth / wireless dongle** | ❌ USB cable only (Bluetooth is owned by the system) |
| **Tested games** | CUSA17776. Tried another one? Open an *issue* and tell us! |

## Installation

You need FileZilla (or any FTP client) and GoldHEN with its FTP server on (port **2121**).

1. Download `flydigi_ds4-v1.3.zip` from the [Releases](../../releases) page and extract it.
2. In FileZilla, connect to the PS4's IP address, port `2121`.
3. Upload **`flydigi_ds4.prx`** to `/data/GoldHEN/plugins/`
4. Upload **`flydigi_ds4.ini`** to `/data/GoldHEN/`
5. Add the plugin to `/data/GoldHEN/plugins.ini`:
   - Right-click `plugins.ini` → **View/Edit**
   - Under the `[default]` line, add:
     ```
     /data/GoldHEN/plugins/flydigi_ds4.prx
     ```
   - Save, close and confirm the upload. *(If the file does not exist, upload the `plugins.ini` from the zip.)*
6. In the GoldHEN menu, make sure **Enable Plugins Loader** is on.
7. Plug the Flydigi in with the USB cable and start a game. A few seconds later you will see **"Flydigi connected"**.

To uninstall, delete the line from `plugins.ini`.

## Default button map

| Flydigi | PS4 |
|---|---|
| A / B / X / Y | ✕ / ○ / □ / △ |
| LB / RB | L1 / R1 |
| LT / RT | L2 / R2 (analog) |
| Menu (Start) | Options |
| View (Back) | Touchpad click |
| L3 / R3, sticks, d-pad | same |
| Flydigi/Home button | nothing (the PS button belongs to the system) |

## Settings

Everything lives in `/data/GoldHEN/flydigi_ds4.ini`, which you edit through FileZilla (right-click → **View/Edit**). Changes apply the next time you start a game. If something is wrong, an on-screen notification tells you the line.

| Option | Values | Default | What it does |
|---|---|---|---|
| `A`, `B`, `X`, `Y`, `LB`, `RB`, `LT`, `RT`, `START`, `BACK`, `GUIDE`, `L3`, `R3`, `UP`, `DOWN`, `LEFT`, `RIGHT` | `CROSS`, `CIRCLE`, `SQUARE`, `TRIANGLE`, `L1`, `R1`, `L2`, `R2`, `L3`, `R3`, `OPTIONS`, `TOUCHPAD`, `UP`, `DOWN`, `LEFT`, `RIGHT`, `NONE` | table above | Remaps the button. Combos work too: `LB = L1+R1` |
| `deadzone_left`, `deadzone_right` | 0 to 50 (%) | 9 | Stick deadzone |
| `trigger_threshold` | 1 to 100 (%) | 8 | How far LT/RT must be pressed to count as pressed |
| `vibration` | 0 to 100 (%) | 100 | Rumble strength |
| `touchpad_mode` | `click` or `swipe` | `click` | See [Touchpad](#touchpad) |
| `gyro` | `yes` / `no` | `no` | See [Gyro](#gyro-experimental) |
| `gyro_sensitivity` | 1 to 500 (%) | 100 | Gyro sensitivity |
| `gyro_invert_horizontal`, `gyro_invert_vertical` | `yes` / `no` | `no` | Inverts one gyro axis |
| `notifications` | `yes` / `no` | `yes` | Connected/disconnected notifications |
| `log` | `yes` / `no` | `no` | Writes `/data/GoldHEN/flydigi_ds4.txt` for troubleshooting |

**Per-game settings:** add a section named after the game's ID and put only what changes in it:

```ini
[default]
vibration = 100

[CUSA17776]
vibration = 0
A = CIRCLE
B = CROSS
```

### Touchpad

- `touchpad_mode = click`: View clicks the touchpad.
- `touchpad_mode = swipe`: while you hold View, the **right stick becomes your finger** on the touchpad, so you can swipe.
  - Quick tap on View: click
  - Hold View still for half a second: click and hold
  - R3 while holding View: click where the finger is
  - Release View and the right stick controls the camera again

### Gyro (experimental)

With `gyro = yes`, the Direwolf 2's motion goes to the DS4 gyro, for games with motion aiming or motion camera.

Over USB the Direwolf 2 only reports **horizontal and vertical** motion, already processed. There is no tilt and no accelerometer, so games that use the controller as a steering wheel or a level will not work. Direction and scale were calibrated with motion tests but may vary between games: if the aim moves the wrong way, use `gyro_invert_horizontal` / `gyro_invert_vertical`, and tune the strength with `gyro_sensitivity`.

## Limitations

- **PS and Share buttons** stay on the DS4: they belong to the system, not the game.
- **The PS4 menus** (outside games) still need the DS4 or the PS4 Second Screen app on your phone.
- **One controller only**: the plugin cannot add a player 2, because the PS4 only creates player 2 when another user signs in with a controller the system recognizes.
- Do not use it together with the `gamepad_helper` plugin (both patch the same part of the game).

## Troubleshooting

**The controller keeps connecting and disconnecting**
Replace the USB cable. A bad cable is exactly what caused this during testing.

**"Flydigi connected" never shows up**
Check that `Enable Plugins Loader` is on, that the line is in `plugins.ini`, and that the controller was plugged in before or during the game. Set `log = yes` and look at `/data/GoldHEN/flydigi_ds4.txt`.

**The game closes when it starts**
Delete the line from `plugins.ini` and open an *issue* with the game ID (CUSAxxxxx) and the log.

**The controller keeps rumbling after closing the game**
Unplug the cable and plug it back in.

## Controller not working?

Support can be added without me owning the controller:

1. In `flydigi_ds4.ini`, set `log = yes`
2. Plug the controller in and start a game
3. Download `/data/GoldHEN/flydigi_ds4.txt` and attach it to an [issue](../../issues/new/choose)

The log describes everything connected over USB, which is what I need to understand the controller.

## Building (Windows)

```powershell
powershell -ExecutionPolicy Bypass -File setup.ps1          # downloads the toolchain (~350 MB)
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1 # PC-side tests
powershell -ExecutionPolicy Bypass -File build.ps1           # builds out\flydigi_ds4.prx
```

Uses [llvm-mingw](https://github.com/mstorsjo/llvm-mingw), the [OpenOrbis PS4 Toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain) and the [GoldHEN Plugins SDK](https://github.com/GoldHEN/GoldHEN_Plugins_SDK), at pinned versions downloaded by `setup.ps1`.

### How it works inside

| File | Role |
|---|---|
| `source/main.c` | Hooks `scePadRead`/`scePadReadState` (same technique as the official `gamepad_helper` plugin), merges the Flydigi into the DS4 data and forwards rumble |
| `source/usb.c` | Thread that finds the controller (any XInput `FF/5D/01` interface), reads its reports through `libSceUsbd` and reconnects on its own |
| `source/xinput.c` | Xbox 360 USB report → neutral state (`RawPad`), including the Direwolf 2's motion bytes |
| `source/translate.c` | `RawPad` → DS4, applying the settings and the touchpad |
| `source/gyro.c` | Motion → DS4 gyro angular velocity |
| `source/config.c` | `flydigi_ds4.ini` parser |
| `tests/tests.c` | PC-side tests, including real reports and motion captured from a Direwolf 2 |

## Credits

- [GoldHEN](https://github.com/GoldHEN) — plugin SDK, and the pad hooking technique from the `gamepad_helper` plugin (by [jocover](https://github.com/jocover))
- [OpenOrbis](https://github.com/OpenOrbis) — PS4 homebrew toolchain
- [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) — compiler

License details in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## License and disclaimer

Open source under the **GPL-3.0** license ([LICENSE](LICENSE)). You may use, study, modify and redistribute it, as long as it stays open source under the same license.

Independent project, **not affiliated with Sony, Flydigi or Microsoft**. PlayStation, DualShock and PS4 are trademarks of Sony Interactive Entertainment; Flydigi and Xbox belong to their respective owners. Use it on consoles you own, at your own risk. This project has nothing to do with piracy and must not be used for it.

---

## ☕ Support the project

flydigi_ds4 is and will stay **free and open source**. It came from a simple frustration: a good controller I already owned that the PS4 refused to recognize. It took many hours of reverse engineering, console testing and tweaking to get here.

If the plugin saved you from buying an adapter, or just got you playing again with the controller you like, please consider a donation of any amount. It keeps the project alive, buys other controllers to test and gives me time to answer *issues* properly.

<table>
  <tr>
    <td align="center">
      <b>Pix</b> (Brazil)<br><br>
      Random key:<br>
      <code>b5a43469-bdb5-417d-9d75-68f3427b6f67</code>
    </td>
  </tr>
</table>

Can't donate? No problem! A ⭐ on the repository, telling us which games work, or recommending it to a friend helps a lot too. **Thank you!** 💙
