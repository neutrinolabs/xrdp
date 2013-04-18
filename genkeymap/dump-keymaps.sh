#!/bin/sh

which setxkbmap
if test $? -ne 0
then
  echo "error, setxkbmap not found"
  exit 1
fi

# English - US 'en-us' 0x0409
setxkbmap -model pc104 -layout us
./xrdp-genkeymap ../instfiles/km-0409.ini

# German 'de' 0x0407
setxkbmap -model pc104 -layout de
./xrdp-genkeymap ../instfiles/km-0407.ini

# Italy 'it' 0x0410
setxkbmap -model pc104 -layout it
./xrdp-genkeymap ../instfiles/km-0410.ini

# Russia 'ru' 0x0419
setxkbmap -model pc104 -layout ru
./xrdp-genkeymap ../instfiles/km-0419.ini

# Sweden 'se' 0x041d
setxkbmap -model pc104 -layout se
./xrdp-genkeymap ../instfiles/km-041d.ini

# Portuguese -PT 'pt-pt' 0x0816
setxkbmap -model pc104 -layout pt
./xrdp-genkeymap ../instfiles/km-0816.ini

# set back to en-us
setxkbmap -model pc104 -layout us
