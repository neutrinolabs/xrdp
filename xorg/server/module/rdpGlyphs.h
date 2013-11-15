/*
Copyright 2012-2013 Jay Sorg

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

gylph(font) calls

*/

#ifndef _RDPGLYPHS_H
#define _RDPGLYPHS_H

struct rdp_font_char
{
    int offset;    /* x */
    int baseline;  /* y */
    int width;     /* cx */
    int height;    /* cy */
    int incby;
    int bpp;
    char *data;
    int data_bytes;
};

struct rdp_text
{
    RegionPtr reg;
    int font;
    int x;
    int y;
    int flags;
    int mixmode;
    char data[256];
    int data_bytes;
    struct rdp_font_char* chars[256];
    int num_chars;
    struct rdp_text* next;
};

int
rdpGlyphDeleteRdpText(struct rdp_text* rtext);
void
rdpGlyphs(CARD8 op, PicturePtr pSrc, PicturePtr pDst,
          PictFormatPtr maskFormat,
          INT16 xSrc, INT16 ySrc, int nlists, GlyphListPtr lists,
          GlyphPtr *glyphs);

#endif
