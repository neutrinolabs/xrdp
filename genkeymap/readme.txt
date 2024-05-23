Keymap file description
-----------------------

The keymap files are used by the xrdp login screen, and also when
sending keyboard input to a VNC server.

The names of the files are of the format;

km-xxxxxxxx.toml

where the xxxxxxxx is replaced by the hex number of the layout of interest.

The files are TOML compatible, with 10 sections;

[General], [noshift], [shift], [altgr], [shiftaltgr], [capslock],
[capslockaltgr], [shiftcapslock], [shiftcapslockaltgr], [numlock]

The [General] section contains information about the file. All other
sections contain key mappings corresponding to the state of the modifier
keys when the key is pressed.

An example line looks like;

<RDP scancode>="<KeySym>[:<unicode>]"  # comment

RDP scancode
------------
The RDP scancode is the code received from the RDP client for each
key. RDP scancodes are more-or-less the same as Windows scancodes,
or "Scan Code Set 1" key-down values.

Example scancodes might be '1C' for the enter key on most European
keyboards, or 'E0 1C' for the number pad enter key.

A good website to consult for scancodes for a wide range of keyboards is
https://kbdlayout.info/

KeySym
------
The KeySym is a value used by the X server as an abstraction of the
engraving on the key being pressed. It is needed to interact with the
VNC server.

Unicode
-------
Keys which generate a character when pressed have this value added.
This is used by the xrdp login screen.

Comment
-------
For generated keymap files, the comment is the name of the X11 KeySym
for the key. This makes it a lot easier to see what is going on in
the file.

Creating a new file
-------------------

To create a new file:-
1) Start an X server
2) Use the 'setxkbmap' command to get the keyboard configured
   for the X server. Currently this has to use the 'evdev' rules so
   that xrdp and the X server agree on the low-level X11 keycodes to
   be used for the keys.
3) Run the 'xrdp-genkeymap' command to extract the keyboard
   mappings

   Example: ./xrdp-genkeymap /etc/xrdp/km-00000409.toml

Note: You need to have enough rights to be able to write to the
/etc/xrdp directory.

Alternatively, create the keymap file in a directory of your choice, then
copy or move it over to /etc/xrdp using sudo/su.

Using the X server of your current session may not be a good idea, as
session and window managers can interfere with key bindings. A good option
is to use an 'Xvfb' dummy X server to do this.

See also the dump_keymaps.sh script which auto-generates many keymap
files for the xrdp distribution. Consider adding your keyboard into this
list.
