Creating a new keymap file.
---------------------------

The names of the files are of the format;

km-xxxxxxxx.ini

where the xxxxxxxx is replaced by the hex number of the layout of interest.

The files have 8 sections;

[noshift], [shift], [altgr], [shiftaltgr], [capslock], [capslockaltgr],
[shiftcapslock], [shiftcapslockaltgr]

In each section there are multiple lines for each key.

An example line looks like;

Key10=49:49

In this line, 10 is the X11 scancode, the first 49 is the keysym value,
the second 49 if the unicode value of the key.  This is the definition
for the 'noshift' '1' key on a en-us keyboard.  In this case, the keysym
and the unicode value are the same.

Here is an example where they are not;

This is the definition for the backspace key;
Key22=65288:8

And this is the star on the keypad;
Key63=65450:42

To create a new file run "xrdp-genkeymap <filename>"

Example: ./xrdp-genkeymap /etc/xrdp/km-00000409.ini

Note: You need to have enough rights to be able to write to the
/etc/xrdp directory.

Alternatively, create the keymap file in a directory of your choice, then
copy or move it over to /etc/xrdp using sudo/su.

