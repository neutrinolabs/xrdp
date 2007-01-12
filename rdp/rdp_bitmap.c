/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2005-2007

   librdp bitmap routines

*/

#include "rdp.h"

/******************************************************************************/
#define CVAL(p) ((unsigned char)(*(p++)))

#if defined(B_ENDIAN)
#define EIK0 1
#define EIK1 0
#else
#define EIK0 0
#define EIK1 1
#endif

/******************************************************************************/
#define REPEAT(statement) \
{ \
  while ((count > 0) && (x < width)) \
  { \
    statement; \
    count--; \
    x++; \
  } \
}

/******************************************************************************/
#define MASK_UPDATE \
{ \
  mixmask <<= 1; \
  if ((mixmask & 0xff) == 0) \
  { \
    mask = fom_mask ? fom_mask : CVAL(input); \
    mixmask = 1; \
  } \
}

/******************************************************************************/
/* 1 byte bitmap decompress */
/* returns boolean */
static int APP_CC
bitmap_decompress1(char* output, int width, int height, char* input, int size)
{
  char* prevline;
  char* line;
  char* end;
  char color1;
  char color2;
  char mix;
  int code;
  int mixmask;
  int mask;
  int opcode;
  int count;
  int offset;
  int isfillormix;
  int x;
  int lastopcode;
  int insertmix;
  int bicolor;
  int fom_mask;

  end = input + size;
  prevline = 0;
  line = 0;
  x = width;
  lastopcode = -1;
  insertmix = 0;
  bicolor = 0;
  color1 = 0;
  color2 = 0;
  mix = 0xff;
  mask = 0;
  fom_mask = 0;

  while (input < end)
  {
    fom_mask = 0;
    code = CVAL(input);
    opcode = code >> 4;
    /* Handle different opcode forms */
    switch (opcode)
    {
      case 0xc:
      case 0xd:
      case 0xe:
        opcode -= 6;
        count = code & 0xf;
        offset = 16;
        break;
      case 0xf:
        opcode = code & 0xf;
        if (opcode < 9)
        {
          count = CVAL(input);
          count |= CVAL(input) << 8;
        }
        else
        {
          count = (opcode < 0xb) ? 8 : 1;
        }
        offset = 0;
        break;
      default:
        opcode >>= 1;
        count = code & 0x1f;
        offset = 32;
        break;
    }
    /* Handle strange cases for counts */
    if (offset != 0)
    {
      isfillormix = ((opcode == 2) || (opcode == 7));
      if (count == 0)
      {
        if (isfillormix)
        {
          count = CVAL(input) + 1;
        }
        else
        {
          count = CVAL(input) + offset;
        }
      }
      else if (isfillormix)
      {
        count <<= 3;
      }
    }
    /* Read preliminary data */
    switch (opcode)
    {
      case 0: /* Fill */
        if ((lastopcode == opcode) && !((x == width) && (prevline == 0)))
        {
          insertmix = 1;
        }
        break;
      case 8: /* Bicolor */
        color1 = CVAL(input);
      case 3: /* Color */
        color2 = CVAL(input);
        break;
      case 6: /* SetMix/Mix */
      case 7: /* SetMix/FillOrMix */
        mix = CVAL(input);
        opcode -= 5;
        break;
      case 9: /* FillOrMix_1 */
        mask = 0x03;
        opcode = 0x02;
        fom_mask = 3;
        break;
      case 0x0a: /* FillOrMix_2 */
        mask = 0x05;
        opcode = 0x02;
        fom_mask = 5;
        break;
    }
    lastopcode = opcode;
    mixmask = 0;
    /* Output body */
    while (count > 0)
    {
      if (x >= width)
      {
        if (height <= 0)
        {
          return 0;
        }
        x = 0;
        height--;
        prevline = line;
        line = output + height * width;
      }
      switch (opcode)
      {
        case 0: /* Fill */
          if (insertmix)
          {
            if (prevline == 0)
            {
              line[x] = mix;
            }
            else
            {
              line[x] = prevline[x] ^ mix;
            }
            insertmix = 0;
            count--;
            x++;
          }
          if (prevline == 0)
          {
            REPEAT(line[x] = 0)
          }
          else
          {
            REPEAT(line[x] = prevline[x])
          }
          break;
        case 1: /* Mix */
          if (prevline == 0)
          {
            REPEAT(line[x] = mix)
          }
          else
          {
            REPEAT(line[x] = prevline[x] ^ mix)
          }
          break;
        case 2: /* Fill or Mix */
          if (prevline == 0)
          {
            REPEAT
            (
              MASK_UPDATE;
              if (mask & mixmask)
              {
                line[x] = mix;
              }
              else
              {
                line[x] = 0;
              }
            )
          }
          else
          {
            REPEAT
            (
              MASK_UPDATE;
              if (mask & mixmask)
              {
                line[x] = prevline[x] ^ mix;
              }
              else
              {
                line[x] = prevline[x];
              }
            )
          }
          break;
        case 3: /* Color */
          REPEAT(line[x] = color2)
          break;
        case 4: /* Copy */
          REPEAT(line[x] = CVAL(input))
          break;
        case 8: /* Bicolor */
          REPEAT
          (
            if (bicolor)
            {
              line[x] = color2;
              bicolor = 0;
            }
            else
            {
              line[x] = color1;
              bicolor = 1;
              count++;
            }
          )
          break;
        case 0xd: /* White */
          REPEAT(line[x] = 0xff)
          break;
        case 0xe: /* Black */
          REPEAT(line[x] = 0)
          break;
        default:
          return 0;
          break;
      }
    }
  }
  return 1;
}

/******************************************************************************/
/* 2 byte bitmap decompress */
/* returns boolean */
static int APP_CC
bitmap_decompress2(char* output, int width, int height, char* input, int size)
{
  char* prevline;
  char* line;
  char* end;
  char color1[2];
  char color2[2];
  char mix[2];
  int code;
  int mixmask;
  int mask;
  int opcode;
  int count;
  int offset;
  int isfillormix;
  int x;
  int lastopcode;
  int insertmix;
  int bicolor;
  int fom_mask;

  end = input + size;
  prevline = 0;
  line = 0;
  x = width;
  lastopcode = -1;
  insertmix = 0;
  bicolor = 0;
  color1[0] = 0;
  color1[1] = 0;
  color2[0] = 0;
  color2[1] = 0;
  mix[0] = 0xff;
  mix[1] = 0xff;
  mask = 0;
  fom_mask = 0;

  while (input < end)
  {
    fom_mask = 0;
    code = CVAL(input);
    opcode = code >> 4;
    /* Handle different opcode forms */
    switch (opcode)
    {
      case 0xc:
      case 0xd:
      case 0xe:
        opcode -= 6;
        count = code & 0xf;
        offset = 16;
        break;
      case 0xf:
        opcode = code & 0xf;
        if (opcode < 9)
        {
          count = CVAL(input);
          count |= CVAL(input) << 8;
        }
        else
        {
          count = (opcode < 0xb) ? 8 : 1;
        }
        offset = 0;
        break;
      default:
        opcode >>= 1;
        count = code & 0x1f;
        offset = 32;
        break;
    }
    /* Handle strange cases for counts */
    if (offset != 0)
    {
      isfillormix = ((opcode == 2) || (opcode == 7));
      if (count == 0)
      {
        if (isfillormix)
        {
          count = CVAL(input) + 1;
        }
        else
        {
          count = CVAL(input) + offset;
        }
      }
      else if (isfillormix)
      {
        count <<= 3;
      }
    }
    /* Read preliminary data */
    switch (opcode)
    {
      case 0: /* Fill */
        if ((lastopcode == opcode) && !((x == width) && (prevline == 0)))
        {
          insertmix = 1;
        }
        break;
      case 8: /* Bicolor */
        color1[EIK0] = CVAL(input);
        color1[EIK1] = CVAL(input);
      case 3: /* Color */
        color2[EIK0] = CVAL(input);
        color2[EIK1] = CVAL(input);
        break;
      case 6: /* SetMix/Mix */
      case 7: /* SetMix/FillOrMix */
        mix[EIK0] = CVAL(input);
        mix[EIK1] = CVAL(input);
        opcode -= 5;
        break;
      case 9: /* FillOrMix_1 */
        mask = 0x03;
        opcode = 0x02;
        fom_mask = 3;
        break;
      case 0x0a: /* FillOrMix_2 */
        mask = 0x05;
        opcode = 0x02;
        fom_mask = 5;
        break;
    }
    lastopcode = opcode;
    mixmask = 0;
    /* Output body */
    while (count > 0)
    {
      if (x >= width)
      {
        if (height <= 0)
        {
          return 0;
        }
        x = 0;
        height--;
        prevline = line;
        line = output + height * (width * 2);
      }
      switch (opcode)
      {
        case 0: /* Fill */
          if (insertmix)
          {
            if (prevline == 0)
            {
              line[x * 2 + 0] = mix[0];
              line[x * 2 + 1] = mix[1];
            }
            else
            {
              line[x * 2 + 0] = prevline[x * 2 + 0] ^ mix[0];
              line[x * 2 + 1] = prevline[x * 2 + 1] ^ mix[1];
            }
            insertmix = 0;
            count--;
            x++;
          }
          if (prevline == 0)
          {
            REPEAT
            (
              line[x * 2 + 0] = 0;
              line[x * 2 + 1] = 0;
            )
          }
          else
          {
            REPEAT
            (
              line[x * 2 + 0] = prevline[x * 2 + 0];
              line[x * 2 + 1] = prevline[x * 2 + 1];
            )
          }
          break;
        case 1: /* Mix */
          if (prevline == 0)
          {
            REPEAT
            (
              line[x * 2 + 0] = mix[0];
              line[x * 2 + 1] = mix[1];
            )
          }
          else
          {
            REPEAT
            (
              line[x * 2 + 0] = prevline[x * 2 + 0] ^ mix[0];
              line[x * 2 + 1] = prevline[x * 2 + 1] ^ mix[1];
            )
          }
          break;
        case 2: /* Fill or Mix */
          if (prevline == 0)
          {
            REPEAT
            (
              MASK_UPDATE;
              if (mask & mixmask)
              {
                line[x * 2 + 0] = mix[0];
                line[x * 2 + 1] = mix[1];
              }
              else
              {
                line[x * 2 + 0] = 0;
                line[x * 2 + 1] = 0;
              }
            )
          }
          else
          {
            REPEAT
            (
              MASK_UPDATE;
              if (mask & mixmask)
              {
                line[x * 2 + 0] = prevline[x * 2 + 0] ^ mix[0];
                line[x * 2 + 1] = prevline[x * 2 + 1] ^ mix[1];
              }
              else
              {
                line[x * 2 + 0] = prevline[x * 2 + 0];
                line[x * 2 + 1] = prevline[x * 2 + 1];
              }
            )
          }
          break;
        case 3: /* Color */
          REPEAT
          (
            line[x * 2 + 0] = color2[0];
            line[x * 2 + 1] = color2[1];
          )
          break;
        case 4: /* Copy */
          REPEAT
          (
            line[x * 2 + EIK0] = CVAL(input);
            line[x * 2 + EIK1] = CVAL(input);
          )
          break;
        case 8: /* Bicolor */
          REPEAT
          (
            if (bicolor)
            {
              line[x * 2 + 0] = color2[0];
              line[x * 2 + 1] = color2[1];
              bicolor = 0;
            }
            else
            {
              line[x * 2 + 0] = color1[0];
              line[x * 2 + 1] = color1[1];
              bicolor = 1;
              count++;
            }
          )
          break;
        case 0xd: /* White */
          REPEAT
          (
            line[x * 2 + 0] = 0xff;
            line[x * 2 + 1] = 0xff;
          )
          break;
        case 0xe: /* Black */
          REPEAT
          (
            line[x * 2 + 0] = 0;
            line[x * 2 + 1] = 0;
          )
          break;
        default:
          return 0;
          break;
      }
    }
  }
  return 1;
}

/******************************************************************************/
/* 3 byte bitmap decompress */
/* returns boolean */
static int APP_CC
bitmap_decompress3(char* output, int width, int height, char* input, int size)
{
  char* prevline;
  char* line;
  char* end;
  char color1[3];
  char color2[3];
  char mix[3];
  int code;
  int mixmask;
  int mask;
  int opcode;
  int count;
  int offset;
  int isfillormix;
  int x;
  int lastopcode;
  int insertmix;
  int bicolor;
  int fom_mask;

  end = input + size;
  prevline = 0;
  line = 0;
  x = width;
  lastopcode = -1;
  insertmix = 0;
  bicolor = 0;
  color1[0] = 0;
  color1[1] = 0;
  color1[2] = 0;
  color2[0] = 0;
  color2[1] = 0;
  color2[2] = 0;
  mix[0] = 0xff;
  mix[1] = 0xff;
  mix[2] = 0xff;
  mask = 0;
  fom_mask = 0;

  while (input < end)
  {
    fom_mask = 0;
    code = CVAL(input);
    opcode = code >> 4;
    /* Handle different opcode forms */
    switch (opcode)
    {
      case 0xc:
      case 0xd:
      case 0xe:
        opcode -= 6;
        count = code & 0xf;
        offset = 16;
        break;
      case 0xf:
        opcode = code & 0xf;
        if (opcode < 9)
        {
          count = CVAL(input);
          count |= CVAL(input) << 8;
        }
        else
        {
          count = (opcode < 0xb) ? 8 : 1;
        }
        offset = 0;
        break;
      default:
        opcode >>= 1;
        count = code & 0x1f;
        offset = 32;
        break;
    }
    /* Handle strange cases for counts */
    if (offset != 0)
    {
      isfillormix = ((opcode == 2) || (opcode == 7));
      if (count == 0)
      {
        if (isfillormix)
        {
          count = CVAL(input) + 1;
        }
        else
        {
          count = CVAL(input) + offset;
        }
      }
      else if (isfillormix)
      {
        count <<= 3;
      }
    }
    /* Read preliminary data */
    switch (opcode)
    {
      case 0: /* Fill */
        if ((lastopcode == opcode) && !((x == width) && (prevline == 0)))
        {
          insertmix = 1;
        }
        break;
      case 8: /* Bicolor */
        color1[0] = CVAL(input);
        color1[1] = CVAL(input);
        color1[2] = CVAL(input);
      case 3: /* Color */
        color2[0] = CVAL(input);
        color2[1] = CVAL(input);
        color2[2] = CVAL(input);
        break;
      case 6: /* SetMix/Mix */
      case 7: /* SetMix/FillOrMix */
        mix[0] = CVAL(input);
        mix[1] = CVAL(input);
        mix[2] = CVAL(input);
        opcode -= 5;
        break;
      case 9: /* FillOrMix_1 */
        mask = 0x03;
        opcode = 0x02;
        fom_mask = 3;
        break;
      case 0x0a: /* FillOrMix_2 */
        mask = 0x05;
        opcode = 0x02;
        fom_mask = 5;
        break;
    }
    lastopcode = opcode;
    mixmask = 0;
    /* Output body */
    while (count > 0)
    {
      if (x >= width)
      {
        if (height <= 0)
        {
          return 0;
        }
        x = 0;
        height--;
        prevline = line;
        line = output + height * (width * 3);
      }
      switch (opcode)
      {
        case 0: /* Fill */
          if (insertmix)
          {
            if (prevline == 0)
            {
              line[x * 3 + 0] = mix[0];
              line[x * 3 + 1] = mix[1];
              line[x * 3 + 2] = mix[2];
            }
            else
            {
              line[x * 3 + 0] = prevline[x * 3 + 0] ^ mix[0];
              line[x * 3 + 1] = prevline[x * 3 + 1] ^ mix[1];
              line[x * 3 + 2] = prevline[x * 3 + 2] ^ mix[2];
            }
            insertmix = 0;
            count--;
            x++;
          }
          if (prevline == 0)
          {
            REPEAT
            (
              line[x * 3 + 0] = 0;
              line[x * 3 + 1] = 0;
              line[x * 3 + 2] = 0;
            )
          }
          else
          {
            REPEAT
            (
              line[x * 3 + 0] = prevline[x * 3 + 0];
              line[x * 3 + 1] = prevline[x * 3 + 1];
              line[x * 3 + 2] = prevline[x * 3 + 2];
            )
          }
          break;
        case 1: /* Mix */
          if (prevline == 0)
          {
            REPEAT
            (
              line[x * 3 + 0] = mix[0];
              line[x * 3 + 1] = mix[1];
              line[x * 3 + 2] = mix[2];
            )
          }
          else
          {
            REPEAT
            (
              line[x * 3 + 0] = prevline[x * 3 + 0] ^ mix[0];
              line[x * 3 + 1] = prevline[x * 3 + 1] ^ mix[1];
              line[x * 3 + 2] = prevline[x * 3 + 2] ^ mix[2];
            )
          }
          break;
        case 2: /* Fill or Mix */
          if (prevline == 0)
          {
            REPEAT
            (
              MASK_UPDATE;
              if (mask & mixmask)
              {
                line[x * 3 + 0] = mix[0];
                line[x * 3 + 1] = mix[1];
                line[x * 3 + 2] = mix[2];
              }
              else
              {
                line[x * 3 + 0] = 0;
                line[x * 3 + 1] = 0;
                line[x * 3 + 2] = 0;
              }
            )
          }
          else
          {
            REPEAT
            (
              MASK_UPDATE;
              if (mask & mixmask)
              {
                line[x * 3 + 0] = prevline[x * 3 + 0] ^ mix[0];
                line[x * 3 + 1] = prevline[x * 3 + 1] ^ mix[1];
                line[x * 3 + 2] = prevline[x * 3 + 2] ^ mix[2];
              }
              else
              {
                line[x * 3 + 0] = prevline[x * 3 + 0];
                line[x * 3 + 1] = prevline[x * 3 + 1];
                line[x * 3 + 2] = prevline[x * 3 + 2];
              }
            )
          }
          break;
        case 3: /* Color */
          REPEAT
          (
            line[x * 3 + 0] = color2[0];
            line[x * 3 + 1] = color2[1];
            line[x * 3 + 2] = color2[2];
          )
          break;
        case 4: /* Copy */
          REPEAT
          (
            line[x * 3 + 0] = CVAL(input);
            line[x * 3 + 1] = CVAL(input);
            line[x * 3 + 2] = CVAL(input);
          )
          break;
        case 8: /* Bicolor */
          REPEAT
          (
            if (bicolor)
            {
              line[x * 3 + 0] = color2[0];
              line[x * 3 + 1] = color2[1];
              line[x * 3 + 2] = color2[2];
              bicolor = 0;
            }
            else
            {
              line[x * 3 + 0] = color1[0];
              line[x * 3 + 1] = color1[1];
              line[x * 3 + 2] = color1[2];
              bicolor = 1;
              count++;
            }
          )
          break;
        case 0xd: /* White */
          REPEAT
          (
            line[x * 3 + 0] = 0xff;
            line[x * 3 + 1] = 0xff;
            line[x * 3 + 2] = 0xff;
          )
          break;
        case 0xe: /* Black */
          REPEAT
          (
            line[x * 3 + 0] = 0;
            line[x * 3 + 1] = 0;
            line[x * 3 + 2] = 0;
          )
          break;
        default:
          return 0;
          break;
      }
    }
  }
  return 1;
}

/*****************************************************************************/
/* returns boolean */
int APP_CC
rdp_bitmap_decompress(char* output, int width, int height, char* input,
                      int size, int Bpp)
{
  int rv;

  switch (Bpp)
  {
    case 1:
      rv = bitmap_decompress1(output, width, height, input, size);
      break;
    case 2:
      rv = bitmap_decompress2(output, width, height, input, size);
      break;
    case 3:
      rv = bitmap_decompress3(output, width, height, input, size);
      break;
    default:
      rv = 0;
      break;
  }
  return rv;
}
