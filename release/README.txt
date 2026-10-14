flydigi_ds4 v1.3 - Flydigi Direwolf 2 (USB) as a PS4 controller
=================================================================
(Em portugues: LEIA-ME.txt)

Requirements: jailbroken PS4 with GoldHEN 2.4 or newer.
Tested on: firmware 12.52 + GoldHEN v2.4b18.7.

INSTALLATION (FileZilla, port 2121)
-----------------------------------
1. Upload flydigi_ds4.prx to  /data/GoldHEN/plugins/
2. Upload flydigi_ds4.ini to  /data/GoldHEN/
3. In /data/GoldHEN/plugins.ini (right-click > View/Edit),
   add this line under [default]:

       /data/GoldHEN/plugins/flydigi_ds4.prx

   If you don't have a plugins.ini, upload the one in this zip.
4. In the GoldHEN menu, turn on "Enable Plugins Loader".
5. Plug the Flydigi in with the USB cable and start a game.
   You should see "Flydigi connected".

SETTINGS
--------
Edit /data/GoldHEN/flydigi_ds4.ini (button remapping, deadzones,
rumble, touchpad, gyro...). Everything is explained inside it.

TROUBLESHOOTING
---------------
- Keeps connecting and disconnecting: replace the USB cable.
- Never connects: set "log = yes" in the .ini and check
  /data/GoldHEN/flydigi_ds4.txt
- Game closes: delete the line from plugins.ini and report it on GitHub.
- To uninstall: delete the line from plugins.ini.

Source code, help and donations: https://github.com/fsagugu-web/flydigi_ds4
GPL-3.0 license. Not affiliated with Sony, Flydigi or Microsoft.
