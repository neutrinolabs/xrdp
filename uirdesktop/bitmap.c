/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   Bitmap decompression routines
   Copyright (C) Matthew Chapman 1999-2005

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
*/

/* three seperate function for speed when decompressing the bitmaps */
/* when modifing one function make the change in the others */
/* comment out #define BITMAP_SPEED_OVER_SIZE below for one slower function */
/* j@american-data.com */

/* indent is confused by this file */
/* *INDENT-OFF* */

#include "rdesktop.h"
#include "uimain.h"

#define CVAL(p)   (*(p++))
#ifdef NEED_ALIGN
#ifdef L_ENDIAN
#define CVAL2(p, v) { v = (*(p++)); v |= (*(p++)) << 8; }
#else
#define CVAL2(p, v) { v = (*(p++)) << 8; v |= (*(p++)); }
#endif /* L_ENDIAN */
#else
#define CVAL2(p, v) { v = (*((uint16*)p)); p += 2; }
#endif /* NEED_ALIGN */

#define UNROLL8(exp) { exp exp exp exp exp exp exp exp }

#define REPEAT(statement) \
{ \
	while((count & ~0x7) && ((x+8) < width)) \
		UNROLL8( statement; count--; x++; ); \
	\
	while((count > 0) && (x < width)) \
	{ \
		statement; \
		count--; \
		x++; \
	} \
}

#define MASK_UPDATE() \
{ \
	mixmask <<= 1; \
	if (mixmask == 0) \
	{ \
		mask = fom_mask ? fom_mask : CVAL(input); \
		mixmask = 1; \
	} \
}

/* 1 byte bitmap decompress */
static BOOL
bitmap_decompress1(uint8 * output, int width, int height, uint8 * input, int size)
{
	uint8 *end = input + size;
	uint8 *prevline = NULL, *line = NULL;
	int opcode, count, offset, isfillormix, x = width;
	int lastopcode = -1, insertmix = False, bicolour = False;
	uint8 code;
	uint8 colour1 = 0, colour2 = 0;
	uint8 mixmask, mask = 0;
	uint8 mix = 0xff;
	int fom_mask;

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
					count = CVAL(input) + 1;
				else
					count = CVAL(input) + offset;
			}
			else if (isfillormix)
			{
				count <<= 3;
			}
		}
		/* Read preliminary data */
		switch (opcode)
		{
			case 0:	/* Fill */
				if ((lastopcode == opcode) && !((x == width) && (prevline == NULL)))
					insertmix = True;
				break;
			case 8:	/* Bicolour */
				colour1 = CVAL(input);
			case 3:	/* Colour */
				colour2 = CVAL(input);
				break;
			case 6:	/* SetMix/Mix */
			case 7:	/* SetMix/FillOrMix */
				mix = CVAL(input);
				opcode -= 5;
				break;
			case 9:	/* FillOrMix_1 */
				mask = 0x03;
				opcode = 0x02;
				fom_mask = 3;
				break;
			case 0x0a:	/* FillOrMix_2 */
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
					return False;
				x = 0;
				height--;
				prevline = line;
				line = output + height * width;
			}
			switch (opcode)
			{
				case 0:	/* Fill */
					if (insertmix)
					{
						if (prevline == NULL)
							line[x] = mix;
						else
							line[x] = prevline[x] ^ mix;
						insertmix = False;
						count--;
						x++;
					}
					if (prevline == NULL)
					{
						REPEAT(line[x] = 0)
					}
					else
					{
						REPEAT(line[x] = prevline[x])
					}
					break;
				case 1:	/* Mix */
					if (prevline == NULL)
					{
						REPEAT(line[x] = mix)
					}
					else
					{
						REPEAT(line[x] = prevline[x] ^ mix)
					}
					break;
				case 2:	/* Fill or Mix */
					if (prevline == NULL)
					{
						REPEAT
						(
							MASK_UPDATE();
							if (mask & mixmask)
								line[x] = mix;
							else
								line[x] = 0;
						)
					}
					else
					{
						REPEAT
						(
							MASK_UPDATE();
							if (mask & mixmask)
								line[x] = prevline[x] ^ mix;
							else
								line[x] = prevline[x];
						)
					}
					break;
				case 3:	/* Colour */
					REPEAT(line[x] = colour2)
					break;
				case 4:	/* Copy */
					REPEAT(line[x] = CVAL(input))
					break;
				case 8:	/* Bicolour */
					REPEAT
					(
						if (bicolour)
						{
							line[x] = colour2;
							bicolour = False;
						}
						else
						{
							line[x] = colour1;
							bicolour = True; count++;
						}
					)
					break;
				case 0xd:	/* White */
					REPEAT(line[x] = 0xff)
					break;
				case 0xe:	/* Black */
					REPEAT(line[x] = 0)
					break;
				default:
					unimpl("bitmap opcode 0x%x\n", opcode);
					return False;
			}
		}
	}
	return True;
}

/* 2 byte bitmap decompress */
static BOOL
bitmap_decompress2(uint8 * output, int width, int height, uint8 * input, int size)
{
	uint8 *end = input + size;
	uint16 *prevline = NULL, *line = NULL;
	int opcode, count, offset, isfillormix, x = width;
	int lastopcode = -1, insertmix = False, bicolour = False;
	uint8 code;
	uint16 colour1 = 0, colour2 = 0;
	uint8 mixmask, mask = 0;
	uint16 mix = 0xffff;
	int fom_mask;

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
					count = CVAL(input) + 1;
				else
					count = CVAL(input) + offset;
			}
			else if (isfillormix)
			{
				count <<= 3;
			}
		}
		/* Read preliminary data */
		switch (opcode)
		{
			case 0:	/* Fill */
				if ((lastopcode == opcode) && !((x == width) && (prevline == NULL)))
					insertmix = True;
				break;
			case 8:	/* Bicolour */
				CVAL2(input, colour1);
			case 3:	/* Colour */
				CVAL2(input, colour2);
				break;
			case 6:	/* SetMix/Mix */
			case 7:	/* SetMix/FillOrMix */
				CVAL2(input, mix);
				opcode -= 5;
				break;
			case 9:	/* FillOrMix_1 */
				mask = 0x03;
				opcode = 0x02;
				fom_mask = 3;
				break;
			case 0x0a:	/* FillOrMix_2 */
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
					return False;
				x = 0;
				height--;
				prevline = line;
				line = ((uint16 *) output) + height * width;
			}
			switch (opcode)
			{
				case 0:	/* Fill */
					if (insertmix)
					{
						if (prevline == NULL)
							line[x] = mix;
						else
							line[x] = prevline[x] ^ mix;
						insertmix = False;
						count--;
						x++;
					}
					if (prevline == NULL)
					{
						REPEAT(line[x] = 0)
					}
					else
					{
						REPEAT(line[x] = prevline[x])
					}
					break;
				case 1:	/* Mix */
					if (prevline == NULL)
					{
						REPEAT(line[x] = mix)
					}
					else
					{
						REPEAT(line[x] = prevline[x] ^ mix)
					}
					break;
				case 2:	/* Fill or Mix */
					if (prevline == NULL)
					{
						REPEAT
						(
							MASK_UPDATE();
							if (mask & mixmask)
								line[x] = mix;
							else
								line[x] = 0;
						)
					}
					else
					{
						REPEAT
						(
							MASK_UPDATE();
							if (mask & mixmask)
								line[x] = prevline[x] ^ mix;
							else
								line[x] = prevline[x];
						)
					}
					break;
				case 3:	/* Colour */
					REPEAT(line[x] = colour2)
					break;
				case 4:	/* Copy */
					REPEAT(CVAL2(input, line[x]))
					break;
				case 8:	/* Bicolour */
					REPEAT
					(
						if (bicolour)
						{
							line[x] = colour2;
							bicolour = False;
						}
						else
						{
							line[x] = colour1;
							bicolour = True;
							count++;
						}
					)
					break;
				case 0xd:	/* White */
					REPEAT(line[x] = 0xffff)
					break;
				case 0xe:	/* Black */
					REPEAT(line[x] = 0)
					break;
				default:
					unimpl("bitmap opcode 0x%x\n", opcode);
					return False;
			}
		}
	}
	return True;
}

/* 3 byte bitmap decompress */
static BOOL
bitmap_decompress3(uint8 * output, int width, int height, uint8 * input, int size)
{
	uint8 *end = input + size;
	uint8 *prevline = NULL, *line = NULL;
	int opcode, count, offset, isfillormix, x = width;
	int lastopcode = -1, insertmix = False, bicolour = False;
	uint8 code;
	uint8 colour1[3] = {0, 0, 0}, colour2[3] = {0, 0, 0};
	uint8 mixmask, mask = 0;
	uint8 mix[3] = {0xff, 0xff, 0xff};
	int fom_mask;

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
					count = (opcode <
						 0xb) ? 8 : 1;
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
					count = CVAL(input) + 1;
				else
					count = CVAL(input) + offset;
			}
			else if (isfillormix)
			{
				count <<= 3;
			}
		}
		/* Read preliminary data */
		switch (opcode)
		{
			case 0:	/* Fill */
				if ((lastopcode == opcode) && !((x == width) && (prevline == NULL)))
					insertmix = True;
				break;
			case 8:	/* Bicolour */
				colour1[0] = CVAL(input);
				colour1[1] = CVAL(input);
				colour1[2] = CVAL(input);
			case 3:	/* Colour */
				colour2[0] = CVAL(input);
				colour2[1] = CVAL(input);
				colour2[2] = CVAL(input);
				break;
			case 6:	/* SetMix/Mix */
			case 7:	/* SetMix/FillOrMix */
				mix[0] = CVAL(input);
				mix[1] = CVAL(input);
				mix[2] = CVAL(input);
				opcode -= 5;
				break;
			case 9:	/* FillOrMix_1 */
				mask = 0x03;
				opcode = 0x02;
				fom_mask = 3;
				break;
			case 0x0a:	/* FillOrMix_2 */
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
					return False;
				x = 0;
				height--;
				prevline = line;
				line = output + height * (width * 3);
			}
			switch (opcode)
			{
				case 0:	/* Fill */
					if (insertmix)
					{
						if (prevline == NULL)
						{
							line[x * 3] = mix[0];
							line[x * 3 + 1] = mix[1];
							line[x * 3 + 2] = mix[2];
						}
						else
						{
							line[x * 3] =
							 prevline[x * 3] ^ mix[0];
							line[x * 3 + 1] =
							 prevline[x * 3 + 1] ^ mix[1];
							line[x * 3 + 2] =
							 prevline[x * 3 + 2] ^ mix[2];
						}
						insertmix = False;
						count--;
						x++;
					}
					if (prevline == NULL)
					{
						REPEAT
						(
							line[x * 3] = 0;
							line[x * 3 + 1] = 0;
							line[x * 3 + 2] = 0;
						)
					}
					else
					{
						REPEAT
						(
							line[x * 3] = prevline[x * 3];
							line[x * 3 + 1] = prevline[x * 3 + 1];
							line[x * 3 + 2] = prevline[x * 3 + 2];
						)
					}
					break;
				case 1:	/* Mix */
					if (prevline == NULL)
					{
						REPEAT
						(
							line[x * 3] = mix[0];
							line[x * 3 + 1] = mix[1];
							line[x * 3 + 2] = mix[2];
						)
					}
					else
					{
						REPEAT
						(
							line[x * 3] =
							 prevline[x * 3] ^ mix[0];
							line[x * 3 + 1] =
							 prevline[x * 3 + 1] ^ mix[1];
							line[x * 3 + 2] =
							 prevline[x * 3 + 2] ^ mix[2];
						)
					}
					break;
				case 2:	/* Fill or Mix */
					if (prevline == NULL)
					{
						REPEAT
						(
							MASK_UPDATE();
							if (mask & mixmask)
							{
								line[x * 3] = mix[0];
								line[x * 3 + 1] = mix[1];
								line[x * 3 + 2] = mix[2];
							}
							else
							{
								line[x * 3] = 0;
								line[x * 3 + 1] = 0;
								line[x * 3 + 2] = 0;
							}
						)
					}
					else
					{
						REPEAT
						(
							MASK_UPDATE();
							if (mask & mixmask)
							{
								line[x * 3] = 
								 prevline[x * 3] ^ mix [0];
								line[x * 3 + 1] =
								 prevline[x * 3 + 1] ^ mix [1];
								line[x * 3 + 2] =
								 prevline[x * 3 + 2] ^ mix [2];
							}
							else
							{
								line[x * 3] =
								 prevline[x * 3];
								line[x * 3 + 1] =
								 prevline[x * 3 + 1];
								line[x * 3 + 2] =
								 prevline[x * 3 + 2];
							}
						)
					}
					break;
				case 3:	/* Colour */
					REPEAT
					(
						line[x * 3] = colour2 [0];
						line[x * 3 + 1] = colour2 [1];
						line[x * 3 + 2] = colour2 [2];
					)
					break;
				case 4:	/* Copy */
					REPEAT
					(
						line[x * 3] = CVAL(input);
						line[x * 3 + 1] = CVAL(input);
						line[x * 3 + 2] = CVAL(input);
					)
					break;
				case 8:	/* Bicolour */
					REPEAT
					(
						if (bicolour)
						{
							line[x * 3] = colour2[0];
							line[x * 3 + 1] = colour2[1];
							line[x * 3 + 2] = colour2[2];
							bicolour = False;
						}
						else
						{
							line[x * 3] = colour1[0];
							line[x * 3 + 1] = colour1[1];
							line[x * 3 + 2] = colour1[2];
							bicolour = True;
							count++;
						}
					)
					break;
				case 0xd:	/* White */
					REPEAT
					(
						line[x * 3] = 0xff;
						line[x * 3 + 1] = 0xff;
						line[x * 3 + 2] = 0xff;
					)
					break;
				case 0xe:	/* Black */
					REPEAT
					(
						line[x * 3] = 0;
						line[x * 3 + 1] = 0;
						line[x * 3 + 2] = 0;
					)
					break;
				default:
					unimpl("bitmap opcode 0x%x\n", opcode);
					return False;
			}
		}
	}
	return True;
}

/* main decompress function */
BOOL
bitmap_decompress(uint8 * output, int width, int height, uint8 * input, int size, int Bpp)
{
	BOOL rv = False;
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
	}
	return rv;
}

/* 2 byte bitmap decompress */
static BOOL
bitmap_decompress_ex_16_to_32(uint8 * output, int width, int height, uint8 * input, int size)
{
	uint8 * end = input + size;
	int * prevline = NULL, * line = NULL;
	int opcode, count, offset, isfillormix, x = width;
	int lastopcode = -1, insertmix = False, bicolour = False;
	uint8 code;
	int colour1 = 0, colour2 = 0;
	uint8 mixmask, mask = 0;
	int mix = 0xffffff;
	int fom_mask;
        int p, r, g, b;

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
			case 0:	/* Fill */
				if ((lastopcode == opcode) && !((x == width) && (prevline == NULL)))
				{
					insertmix = True;
				}
				break;
			case 8:	/* Bicolour */
				CVAL2(input, p);
				SPLIT_COLOUR16(p, r, g, b);
				MAKE_COLOUR32(p, r, g, b);
				colour1 = p;
			case 3:	/* Colour */
				CVAL2(input, p);
				SPLIT_COLOUR16(p, r, g, b);
				MAKE_COLOUR32(p, r, g, b);
				colour2 = p;
				break;
			case 6:	/* SetMix/Mix */
			case 7:	/* SetMix/FillOrMix */
				CVAL2(input, p);
				SPLIT_COLOUR16(p, r, g, b);
				MAKE_COLOUR32(p, r, g, b);
				mix = p;
				opcode -= 5;
				break;
			case 9:	/* FillOrMix_1 */
				mask = 0x03;
				opcode = 0x02;
				fom_mask = 3;
				break;
			case 0x0a:	/* FillOrMix_2 */
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
					return False;
				}
				x = 0;
				height--;
				prevline = line;
				line = ((int *) output) + height * width;
			}
			switch (opcode)
			{
				case 0:	/* Fill */
					if (insertmix)
					{
						if (prevline == NULL)
						{
							line[x] = mix;
						}
						else
						{
							line[x] = prevline[x] ^ mix;
						}
						insertmix = False;
						count--;
						x++;
					}
					if (prevline == NULL)
					{
						REPEAT(line[x] = 0)
					}
					else
					{
						REPEAT(line[x] = prevline[x])
					}
					break;
				case 1:	/* Mix */
					if (prevline == NULL)
					{
						REPEAT(line[x] = mix)
					}
					else
					{
						REPEAT(line[x] = prevline[x] ^ mix)
					}
					break;
				case 2:	/* Fill or Mix */
					if (prevline == NULL)
					{
						REPEAT
						(
							MASK_UPDATE();
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
							MASK_UPDATE();
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
				case 3:	/* Colour */
					REPEAT(line[x] = colour2)
					break;
				case 4:	/* Copy */
					REPEAT
					(
						CVAL2(input, p);
						SPLIT_COLOUR16(p, r, g, b);
						MAKE_COLOUR32(p, r, g, b);
						line[x] = p;
					)
					break;
				case 8:	/* Bicolour */
					REPEAT
					(
						if (bicolour)
						{
							line[x] = colour2;
							bicolour = False;
						}
						else
						{
							line[x] = colour1;
							bicolour = True;
							count++;
						}
					)
					break;
				case 0xd:	/* White */
					REPEAT(line[x] = 0xffffff)
					break;
				case 0xe:	/* Black */
					REPEAT(line[x] = 0)
					break;
				default:
					unimpl("bitmap opcode 0x%x\n", opcode);
					return False;
			}
		}
	}
	return True;
}

/* 3 byte bitmap decompress */
static BOOL
bitmap_decompress_ex_24_to_32(uint8 * output, int width, int height, uint8 * input, int size)
{
	uint8 * end = input + size;
	int * prevline = NULL, * line = NULL;
	int opcode, count, offset, isfillormix, x = width;
	int lastopcode = -1, insertmix = False, bicolour = False;
	uint8 code;
	int colour1 = 0, colour2 = 0;
	uint8 mixmask, mask = 0;
	int mix = 0xffffff;
	int fom_mask;
        int p, r, g, b;

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
					count = (opcode <
						 0xb) ? 8 : 1;
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
					count = CVAL(input) + 1;
				else
					count = CVAL(input) + offset;
			}
			else if (isfillormix)
			{
				count <<= 3;
			}
		}
		/* Read preliminary data */
		switch (opcode)
		{
			case 0:	/* Fill */
				if ((lastopcode == opcode) && !((x == width) && (prevline == NULL)))
					insertmix = True;
				break;
			case 8:	/* Bicolour */
				b = CVAL(input);
				g = CVAL(input);
				r = CVAL(input);
				MAKE_COLOUR32(p, r, g, b);
				colour1 = p;
			case 3:	/* Colour */
				b = CVAL(input);
				g = CVAL(input);
				r = CVAL(input);
				MAKE_COLOUR32(p, r, g, b);
				colour2 = p;
				break;
			case 6:	/* SetMix/Mix */
			case 7:	/* SetMix/FillOrMix */
				b = CVAL(input);
				g = CVAL(input);
				r = CVAL(input);
				MAKE_COLOUR32(p, r, g, b);
				mix = p;
				opcode -= 5;
				break;
			case 9:	/* FillOrMix_1 */
				mask = 0x03;
				opcode = 0x02;
				fom_mask = 3;
				break;
			case 0x0a:	/* FillOrMix_2 */
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
					return False;
				x = 0;
				height--;
				prevline = line;
				line = ((int *) output) + height * width;
			}
			switch (opcode)
			{
				case 0:	/* Fill */
					if (insertmix)
					{
						if (prevline == NULL)
						{
							line[x] = mix;
						}
						else
						{
							line[x] = prevline[x] ^ mix;
						}
						insertmix = False;
						count--;
						x++;
					}
					if (prevline == NULL)
					{
						REPEAT(line[x] = 0)
					}
					else
					{
						REPEAT(line[x] = prevline[x])
					}
					break;
				case 1:	/* Mix */
					if (prevline == NULL)
					{
						REPEAT(line[x] = mix)
					}
					else
					{
						REPEAT(line[x] = prevline[x] ^ mix)
					}
					break;
				case 2:	/* Fill or Mix */
					if (prevline == NULL)
					{
						REPEAT
						(
							MASK_UPDATE();
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
							MASK_UPDATE();
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
				case 3:	/* Colour */
					REPEAT(line[x] = colour2)
					break;
				case 4:	/* Copy */
					REPEAT
					(
						b = CVAL(input);
						g = CVAL(input);
						r = CVAL(input);
						MAKE_COLOUR32(p, r, g, b);
						line[x] = p;
					)
					break;
				case 8:	/* Bicolour */
					REPEAT
					(
						if (bicolour)
						{
							line[x] = colour2;
							bicolour = False;
						}
						else
						{
							line[x] = colour1;
							bicolour = True;
							count++;
						}
					)
					break;
				case 0xd:	/* White */
					REPEAT(line[x] = 0xffffff);
					break;
				case 0xe:	/* Black */
					REPEAT(line[x] = 0);
					break;
				default:
					unimpl("bitmap opcode 0x%x\n", opcode);
					return False;
			}
		}
	}
	return True;
}

BOOL
bitmap_decompress_ex(uint8 * output, int width, int height, uint8 * input, int size,
                     int in_bpp, int out_bpp)
{
	if (in_bpp == 16 && out_bpp == 32)
	{
		return bitmap_decompress_ex_16_to_32(output, width, height, input, size);
	}
	else if (in_bpp == 24 && out_bpp == 32)
	{
		return bitmap_decompress_ex_24_to_32(output, width, height, input, size);
	}
	return False;
}

/* *INDENT-ON* */
