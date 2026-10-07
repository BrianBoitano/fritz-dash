#!/bin/bash
# Fritz Dash - Windows smoke test under Wine and Xvfb, inside fritzdash-builder:3
# (README.md, "Windows (PC)").
# Usage: win_smoke.sh <folder with FritzDash.exe> <output folder>
# 1. FritzDash.exe --selftest: BMPs and selftest.txt, saves in the Wine user's
#    %APPDATA%\FritzDash.
# 2. The same with SDL's dummy audio driver, so the synth callback runs (Wine
#    in a container has no sound device).
# 3. A normal run: a screen grab of the title, then keyboard presses (Space,
#    Esc, F11) sent with xdotool, a grab after each.
# 4. --selftest in a second Wine prefix whose user is "Mr_ Mügatu", so the
#    saves go to a folder with a space and an accent in its name.
# Everything ends up as PNG in <output folder>/shots.
set -u
EXE=$1/FritzDash.exe
OUT=$2
export WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" DISPLAY=:99 LANG=C.UTF-8
SHOTS=$OUT/shots
DRIVE='C'':'                    # the Wine prefixes' system drive
ODD='Mr_ Mügatu'
mkdir -p "$SHOTS"

Xvfb :99 -screen 0 1920x1080x24 -nolisten tcp >/dev/null 2>&1 &
XVFB=$!
sleep 2

grab() {
    xwd -root -silent -display :99 | convert xwd:- "$SHOTS/$1.png"
    echo "grab $SHOTS/$1.png"
}

key() {
    xdotool keydown "$1"
    sleep 0.25
    xdotool keyup "$1"
    sleep "$2"
}

# selftest <label> <Windows output folder> <same folder as a Unix path>
selftest() {
    echo "== $1"
    timeout 180 wine "$EXE" --selftest "$2" 2>/dev/null
    echo "selftest exit $?"
    cat "$3/selftest.txt"
    for b in "$3"/*.bmp; do
        convert "$b" "$SHOTS/$1_$(basename "$b" .bmp).png" && echo "png $SHOTS/$1_$(basename "$b" .bmp).png"
    done
}

export WINEPREFIX=$OUT/prefix-root USER=root WINEUSERNAME=root
wineboot -i >/dev/null 2>&1
wineserver -w
ROAM=$WINEPREFIX/drive_c/users/root/AppData/Roaming
selftest selftest "$DRIVE\\users\\root\\AppData\\Roaming\\FritzDash\\selftest" "$ROAM/FritzDash/selftest"
SDL_AUDIODRIVER=dummy selftest dummyaudio "$DRIVE\\users\\root\\AppData\\Roaming\\FritzDash\\selftest dummy audio" \
    "$ROAM/FritzDash/selftest dummy audio"

echo "== normal run with keyboard"
wine "$EXE" 2>/dev/null &
GAME=$!
sleep 12
grab run_1_title
key space 2
grab run_2_worlds_after_space
key space 2
key space 3
grab run_3_playing_after_space
key Escape 2
grab run_4_paused_after_esc
key F11 3
grab run_5_windowed_after_f11
wineserver -k
wait $GAME
echo "== saves ($ROAM)"
(cd "$ROAM" && find FritzDash -type f -printf '%10s  %p\n' | sort -k2)
echo "== log.txt"
cat "$ROAM/FritzDash/log.txt"

export WINEPREFIX=$OUT/prefix-odd USER=$ODD WINEUSERNAME=$ODD
wineboot -i >/dev/null 2>&1
wineserver -w
ROAM=$WINEPREFIX/drive_c/users/$ODD/AppData/Roaming
selftest odduser "$DRIVE\\users\\$ODD\\AppData\\Roaming\\FritzDash\\selftest ü" "$ROAM/FritzDash/selftest ü"
wineserver -k
echo "== saves ($ROAM)"
(cd "$ROAM" && find FritzDash -type f -printf '%10s  %p\n' | sort -k2)
echo "== log.txt"
cat "$ROAM/FritzDash/log.txt"
kill $XVFB
