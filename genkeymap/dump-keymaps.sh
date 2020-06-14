#!/bin/sh

which setxkbmap
if test $? -ne 0
then
  echo "error, setxkbmap not found"
  exit 1
fi

# English - US 'en-us' 0x00000409
setxkbmap -model pc104 -layout us
./xrdp-genkeymap ../instfiles/km-00000409.ini

# English - US 'dvorak' 0x00010409
setxkbmap -model pc104 -layout dvorak
./xrdp-genkeymap ../instfiles/km-00010409.ini

# English - US 'dvp' 0x19360409
OLD_SETTINGS=$(setxkbmap -query -verbose 4 | sed "s/^\([a-z]\+\):\s*\(.*\)$/-\1 \2/;s/^-options/-option \"\" -option/;s/,/ -option /g" | xargs -d \\n)
setxkbmap -rules xfree86 -model pc105 -layout us -variant dvp -option "" -option compose:102 -option caps:shift -option numpad:sg -option numpad:shift3 -option keypad:hex -option keypad:atm -option kpdl:semi -option lv3:ralt_alt
./xrdp-genkeymap ../instfiles/km-19360409.ini
setxkbmap ${OLD_SETTINGS}

# English - UK 'en-GB' 0x00000809
setxkbmap -model pc105 -layout gb
./xrdp-genkeymap ../instfiles/km-00000809.ini

# German 'de' 0x00000407
setxkbmap -model pc104 -layout de
./xrdp-genkeymap ../instfiles/km-00000407.ini

# Italian 'it' 0x00000410
setxkbmap -model pc104 -layout it
./xrdp-genkeymap ../instfiles/km-00000410.ini

# Japanese 'jp' 0x00000411
setxkbmap -model pc105 -layout jp -variant OADG109A
./xrdp-genkeymap ../instfiles/km-00000411.ini

# Polish 'pl' 0x00000415
setxkbmap -model pc104 -layout pl
./xrdp-genkeymap ../instfiles/km-00000415.ini

# Russia 'ru' 0x00000419
setxkbmap -model pc104 -layout ru
./xrdp-genkeymap ../instfiles/km-00000419.ini

# Sweden 'se' 0x0000041d
setxkbmap -model pc104 -layout se
./xrdp-genkeymap ../instfiles/km-0000041d.ini

# Portuguese -PT 'pt-pt' 0x00000816
setxkbmap -model pc104 -layout pt
./xrdp-genkeymap ../instfiles/km-00000816.ini

# set back to en-us
setxkbmap -model pc104 -layout us
