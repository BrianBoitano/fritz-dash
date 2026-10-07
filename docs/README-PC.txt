FRITZ DASH 3.2 FOR WINDOWS
==========================

Double-click FritzDash.exe to play. It is the whole game: nothing to install.
It starts full screen. F11 or Alt+Enter switches to a window and back.
Alt+F4 (or closing the window) quits. Esc is pause, not quit.

A PS4 or Xbox controller works (plug it in before or while playing; up to 4).
The keyboard counts as player 1's controller.

CONTROLS
--------
PS4 button        Xbox button       Keyboard
----------        -----------       --------
X (Cross)         A                 Space or Z
Circle            B                 X or Backspace
Square            X                 C
Triangle          Y                 V
L1                LB                Q
R1                RB                E
L2                LT                1
R2                RT                3
Options           Start (Menu)      Esc or Enter
Touchpad          Back (View)       Tab
L3 (click left)   Left stick click  F
D-pad             D-pad             Arrow keys
Left stick        Left stick        Arrow keys
Right stick       Right stick       I J K L (up, left, down, right)

2 player race: with one controller, the controller is player 1 and the
keyboard is player 2 (Space or Z jumps). With two controllers, the first
one plugged in is player 1 (with the keyboard) and the second is player 2.
Hold L1+R1 (Q+E) on the title for the diagnostic line.

SAVES
-----
Levels, songs, icons, progress and settings live in
    %APPDATA%\FritzDash
(paste that into the Explorer address bar). The files are the same as on
the PS4's /data/fritzdash, so levels can be copied between the two.
log.txt in that folder says how the last start went (screen, sound,
controllers); it helps when something does not work.

UNINSTALL
---------
Delete this folder and any shortcuts to FritzDash.exe. To remove the
saves as well, delete %APPDATA%\FritzDash.

CHECKING A BUILD
----------------
FritzDash.exe --selftest <folder> plays a short scripted run, writes
screenshots and selftest.txt to the folder, and exits.
