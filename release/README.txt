Camera Distance @VERSION@ for MHGU Recomp
=====================================

Sets how far the quest camera stays behind your hunter and behind your
Prowler, from 30% to 300% of the game's distance. Changes show at once, with
no restart and no replaced game files.

Only tested on MHGU Recomp, with the English game text.

Requirements
------------
  - MHGU Recomp with MHGU 1.4.0 (Forge is built into the recomp).
  - Purple-lib r19 or later for the settings page in the game's menu.
    Optional: https://gamebanana.com/mods/740376

Install
-------
Put this "camera-distance" folder in the recomp's "mods" folder:
  - macOS: ~/Library/Application Support/MHGURecomp/mods/
  - Windows and Linux: the "mods" folder next to the recomp's executable
Install Purple-lib the same way for the settings page.

Settings
--------
With Purple-lib: open the start menu, then Options > Mod Options > Camera
Distance. Hunter and Prowler are percentages of the game's distance (100% is
the game's own); Defaults puts both back to 100%. They're saved when you close
the window.

Without Purple-lib: press Insert in game and open "Camera Distance".

The settings are in save_0/camdist.ini, next to your save (by default in the
recomp's "save" folder). You can edit it while the game is closed.

Other camera mods
-----------------
Mods that edit camera\param\questcamera in quest.arc change the same two
numbers. The percentages apply on top of theirs, so 100% keeps that mod's
distance. Remove one of them unless you want both.

Uninstall
---------
Delete the "camera-distance" folder, or turn the mod off in the recomp's Mods
tab. You can also delete save_0/camdist.ini; the game's save doesn't need it.

Source code, issues and more details: https://github.com/mhvuze/mhgu-camera-distance
