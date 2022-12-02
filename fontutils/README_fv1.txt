The fv1 font format has the following characteristics:-

1) Bitmap fonts (i.e. pixels are on or off)
2) 96 DPI is assumed
3) Glyphs from U+0020 up to a pre-defined limit are stored in the file.
   At the time of writing this limit is U+4DFF. To change the limit
   requires a change to xrdp/xrdp_types.h and (potentially)
   fontutils/mkfv1.c
4) Font format is header, plus a variable data area for each glyph.

The intention (over time) is to build support for the freetype2 library
directly into xrdp. This will allow for modern font features like
anti-aliasing and kerning to be supported.

General Info
------------
All numeric values are 2 octets, and stored in twos-complement
little-endian format.

Dimensions are all measured in pixels.

Font header
-----------

signature     4 octets   File signature - "FNT1"
font~name    32 octets   Null-terminated if less that 32 octets long
point_size    2 octets   Assumes 96 DPI.
style         2 octets   Unused. Set to 1.
body_height   2 octets   Line spacing for font.
min_descender 2 octets   The common lowest descender value in the font
                         glyphs (A few glyphs may be lower than
                         this). Used to calculate where the font baseline
                         is in relation to the text box for the font.
<padding>     4 octets   Set to zero.

Older fonts, generated for xrdp v.0.9x and earlier, have zero values
for the body_height and min_descender. For these fonts, the body height is
calculated as (-baseline + 1) for the first glyph, and the glyphs are
all offset so that a min_descender of 0 works OK.

Glyph data
----------
The header is followed by a number of glyphs representing U+0020 upwards. The
glyphs have a variable size. The format of each glyph is as follows:-

width         2 octets   Width of character data
height        2 octets   Height of character data
baseline      2 octets   Offset from font baseline to 1st row of glyph data
offset        2 octets   Space before glyph is drawn (can be -ve)
inc_by        2 octets   Total width of glyph + spacing. The width of
                         a string is obtained by adding up all the inc_by
                         fields for all the glyphs
data          <variable> Bitmap data.

Bitmap data is laid out in rows from top to bottom.  Within each row the
most significant bit of each octet is on the left.  Row data is padded
to the nearest octet (e.g. a 14 bit width would be padded by 2 bits to
16 bits (2 octets). The total data is padded out with between 0 and 3
octets to end on a 4-octet boundary.

Example glyph:-

Glyph : U+0067
  Width : 12
  Height : 18
  Baseline : -13
  Offset : 1
  Inc By : 15
  Data :
    -13: ...XXXXX..XX    1F 30
    -12: ..XXXXXXXXXX    3F F0
    -11: .XXX....XXXX    70 F0
    -10: XXX......XXX    E0 70
     -9: XX........XX    C0 30
     -8: XX........XX    C0 30
     -7: XX........XX    C0 30
     -6: XX........XX    C0 30
     -5: XX........XX    C0 30
     -4: XXX......XXX    E0 70
     -3: .XXX....XXXX    70 F0
     -2: ..XXXXXXXXXX    3F F0
     -1: ...XXXXX..XX    1F 30
     +0: ..........XX    00 30
     +1: .........XXX    00 70
     +2: ..X.....XXX.    20 E0
     +3: ..XXXXXXXX..    3F C0
     +4: ...XXXXXX...    1F 80
