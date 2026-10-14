# Changelog

## v1.3 — first public release

- Experimental gyro (`gyro = yes`): the Direwolf 2's horizontal and vertical motion goes to the DS4 gyro, with configurable sensitivity and axis inversion
- Touchpad swipe (`touchpad_mode = swipe`): while holding View, the right stick becomes a finger on the touchpad
- Every touchpad click now comes with a finger in the centre, like a real DS4
- With `log = yes`, the log describes every USB device, to help support other controllers
- On-screen notifications, log and settings file are now in English; the Portuguese words (`sim`/`nao`, `XIS`, `BOLA`…) still work in `flydigi_ds4.ini`

## v1.1

- Settings file `/data/GoldHEN/flydigi_ds4.ini`: remapping (including combos), deadzones, triggers, rumble, notifications and log
- Per-game sections (`[CUSAxxxxx]`)
- On-screen notification with the line number when the file has a mistake
- Log off by default

## v1.0

- Flydigi Direwolf 2 over USB working as a DS4 in games: buttons, sticks, analog triggers and rumble
- Works with the DS4 turned off
- Plug and unplug while the game is running
