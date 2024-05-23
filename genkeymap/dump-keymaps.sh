#!/bin/sh

# Use evdev rules to obtain the mappings
XKB_RULES=evdev

# -----------------------------------------------------------------------------
# K B G E N
#
# Generates a keyboard mapping
# $1 - LCID (See [MS-LCID])
# $2 - Language tag (See [MS-LCID])
# $3 - Current operating system
# $4.. Arguments to setxkbmap to select the required keyboard
# -----------------------------------------------------------------------------
kbgen()
{
    file=$(printf "../instfiles/km-%08s.toml" "$1" | tr ' ' '0')
    desc="$2"
    os="$3"
    shift 3
    setxkbmap "$@"
    ./xrdp-genkeymap \
        -c "Description: $desc" \
        -c "Operating system: $os" \
        "$file"
}

# -----------------------------------------------------------------------------
# M A I N
# -----------------------------------------------------------------------------
# Run the keymap extraction in a clean X session
if [ "$1" != _IN_XVFB_SESSION_ ]; then
    # All commands available?
    ok=1
    for cmd in setxkbmap xvfb-run xauth; do
        if ! command -v $cmd >/dev/null
        then
          echo "Error. $cmd not found" >&2
          ok=
        fi
    done
    if [ -z "$ok" ]; then
        exit 1
    fi

    xvfb-run --auto-servernum --server-args="-noreset" "$0" _IN_XVFB_SESSION_
    exit $?
fi

OLD_SETTINGS=$(setxkbmap -query -verbose 4 | sed "s/^\([a-z]\+\):\s*\(.*\)$/-\1 \2/;s/^-options/-option \"\" -option/;s/,/ -option /g" | xargs -d \\n)

setxkbmap -rules "$XKB_RULES"

# Assign NumLock to mod2
xmodmap -e "clear mod2" -e "add mod2 = Num_Lock"

# Find the operating system
if command -v hostnamectl >/dev/null; then
    os=$(hostnamectl status | sed -ne 's/^i *Operating[^:]*: *//p')
fi
if [ -z "$os" ] && command -v lsb_release -v >/dev/null; then
    os=$(lsb_release --description --short)
fi
if [ -z "$os" ]; then
    os="Unknown"
fi

kbgen 0406 "da-DK"        "$os" -model pc105 -layout dk
kbgen 0407 "de-DE"        "$os" -model pc104 -layout de
kbgen 0409 "en-US"        "$os" -model pc104 -layout us
kbgen 10409 "en-US"       "$os" -model pc104 -layout dvorak
kbgen 040a "es-ES_tradnl" "$os" -model pc105 -layout es
kbgen 040b "fi-FI"        "$os" -model pc105 -layout 'fi'
kbgen 040c "fr-FR"        "$os" -model pc105 -layout fr
kbgen 0410 "it-IT"        "$os" -model pc104 -layout it
kbgen 0411 "ja-JP"        "$os" -model pc105 -layout jp -variant OADG109A
kbgen 0412 "ko-KR"        "$os" -model pc105 -layout kr
kbgen 0414 "nb-NO"        "$os" -model pc105 -layout no
kbgen 0415 "pl-PL"        "$os" -model pc104 -layout pl
kbgen 0416 "pt-BR"        "$os" -model pc105 -layout br
kbgen 0419 "ru-RU"        "$os" -model pc104 -layout ru
kbgen 041d "sv-SE"        "$os" -model pc104 -layout se
kbgen 0807 "de-CH"        "$os" -model pc105 -layout ch
kbgen 0809 "en-GB"        "$os" -model pc105 -layout gb
kbgen 080a "es-MX"        "$os" -model pc105 -layout latam
kbgen 080c "fr-BE"        "$os" -model pc105 -layout be -variant oss
kbgen 0813 "nl-BE"        "$os" -model pc105 -layout be
kbgen 0816 "pt-PT"        "$os" -model pc104 -layout pt
kbgen 100c "fr-CH"        "$os" -model pc105 -layout ch -variant fr

# Put keyboards which change options below the ones that don't to
# prevent unexpected things happening to keypads, etc
kbgen 19360409 "en-US"    "$os" -model pc105 -layout us -variant dvp \
    -option "" -option compose:102 -option caps:shift -option numpad:sg \
    -option numpad:shift3 -option keypad:hex -option keypad:atm \
    -option kpdl:semi -option lv3:ralt_alt
# set back to entry settings
# shellcheck disable=SC2086
setxkbmap ${OLD_SETTINGS}
