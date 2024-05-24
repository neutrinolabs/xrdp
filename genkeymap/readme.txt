Keymap file description
-----------------------

The keymap files are used by the xrdp login screen, and also when
sending keyboard input to a VNC server.

The names of the files are of the format;

km-xxxxxxxx.toml

where the xxxxxxxx is replaced by the hex number of the layout of interest.

The contents of the files are documented in xrdp-km.toml(5)

See also xrdp-genkeymap(8) which describes the utility used to
generate these files.

Creating a new file
-------------------

To create a new file:-
1) Start an X server
2) Use the 'setxkbmap' command to get the keyboard configured
   for the X server.
3) Run the 'xrdp-genkeymap' command to extract the keyboard
   mappings

   Example: ./xrdp-genkeymap ./km-00000409.toml

4) Copy the generated file to /etc/xrdp/

Using the X server of your current session may not be a good idea, as
session and window managers can interfere with key bindings. A good option
is to use an 'Xvfb' dummy X server to do this.

Getting a file added to xrdp
----------------------------

The file dump-keymaps.sh in this directory is used to auto-generate
all keymap files. It runs on Linux currently, but will generate
keymap files suitable for any xrdp platform.

1) Add a line towards the end of this file which causes your mapping to
   be generated. Use the other lines in this file as a guide.
2) Run the dump-keymaps.sh script to generate a new file in
   instfiles/
3) Add your mapping to the list in instfiless/Makefile.am
4) Submit a pull request to the project containing the above three
   changes.
