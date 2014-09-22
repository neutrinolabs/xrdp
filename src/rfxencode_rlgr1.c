/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - RLGR
 *
 * Copyright 2011 Vic Lee
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * This implementation of RLGR refers to
 * [MS-RDPRFX] 3.1.8.1.7.3 RLGR1/RLGR3 Pseudocode
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rfx_bitstream.h"

/* Constants used within the RLGR1/RLGR3 algorithm */
#define KPMAX   (80)  /* max value for kp or krp */
#define LSGR    (3)   /* shift count to convert kp to k */
#define UP_GR   (4)   /* increase in kp after a zero run in RL mode */
#define DN_GR   (6)   /* decrease in kp after a nonzero symbol in RL mode */
#define UQ_GR   (3)   /* increase in kp after nonzero symbol in GR mode */
#define DQ_GR   (3)   /* decrease in kp after zero symbol in GR mode */

/* Returns the least number of bits required to represent a given value */
#define GetMinBits(_val, _nbits) \
do { \
    uint32 _v = _val; \
    _nbits = 0; \
    while (_v) \
    { \
        _v >>= 1; \
        _nbits++; \
    } \
} while (0)

/*
 * Update the passed parameter and clamp it to the range [0, KPMAX]
 * Return the value of parameter right-shifted by LSGR
 */
#define UpdateParam(_param, _deltaP, _k) \
do { \
    _param += _deltaP; \
    if (_param > KPMAX) \
    { \
        _param = KPMAX; \
    } \
    if (_param < 0) \
    { \
        _param = 0; \
    } \
    _k = (_param >> LSGR); \
} while (0)

/* Returns the next coefficient (a signed int) to encode, from the input stream */
#define GetNextInput(_n) \
do { \
    if (data_size > 0) \
    { \
        _n = *data++; \
        data_size--; \
    } \
    else \
    { \
        _n = 0; \
    } \
} while (0)

/* Emit bitPattern to the output bitstream */
#define OutputBits(_numBits, _bitPattern) rfx_bitstream_put_bits(bs, _bitPattern, _numBits)

/* Emit a bit (0 or 1), count number of times, to the output bitstream */
#define OutputBit(_count, _bit) \
do \
{   \
    uint16 b = (_bit ? 0xFFFF : 0); \
    int c = _count; \
    for (; c > 0; c -= 16) \
    { \
        rfx_bitstream_put_bits(bs, b, (c > 16 ? 16 : c)); \
    } \
} while (0)

/* Converts the input value to (2 * abs(input) - sign(input)), where sign(input) = (input < 0 ? 1 : 0) and returns it */
#define Get2MagSign(_input) ((_input) >= 0 ? 2 * (_input) : -2 * (_input) - 1)

/* Outputs the Golomb/Rice encoding of a non-negative integer */
#define CodeGR(_krp, _val) \
do { \
    int lkr = (_krp) >> LSGR; \
    int lval = _val; \
    /* unary part of GR code */ \
    uint32 lvk = lval >> lkr; \
    OutputBit(lvk, 1); \
    OutputBit(1, 0); \
    /* remainder part of GR code, if needed */ \
    if (lkr) \
    { \
        OutputBits(lkr, lval & ((1 << lkr) - 1)); \
    } \
    /* update krp, only if it is not equal to 1 */ \
    if (lvk == 0) \
    { \
        UpdateParam(_krp, -2, lkr); \
    } \
    else if (lvk > 1) \
    { \
        UpdateParam(_krp, lvk, lkr); \
    } \
} while (0)

int
rfx_rlgr1_encode(const sint16* data, int data_size, uint8* buffer, int buffer_size)
{
    int k;
    int kp;
    int krp;

    int input;
    int numZeros;
    int runmax;
    int mag;
    int sign;
    int processed_size;
    int lmag;

    RFX_BITSTREAM bs;

    uint32 twoMs;

    rfx_bitstream_attach(bs, buffer, buffer_size);

    /* initialize the parameters */
    k = 1;
    kp = 1 << LSGR;
    krp = 1 << LSGR;

    /* process all the input coefficients */
    while (data_size > 0)
    {
        if (k)
        {
            /* RUN-LENGTH MODE */

            /* collect the run of zeros in the input stream */
            numZeros = 0;
            GetNextInput(input);
            while (input == 0 && data_size > 0)
            {
                numZeros++;
                GetNextInput(input);
            }

            /* emit output zeros */
            runmax = 1 << k;
            while (numZeros >= runmax)
            {
                OutputBit(1, 0); /* output a zero bit */
                numZeros -= runmax;
                UpdateParam(kp, UP_GR, k); /* update kp, k */
                runmax = 1 << k;
            }

            /* output a 1 to terminate runs */
            OutputBit(1, 1);

            /* output the remaining run length using k bits */
            OutputBits(k, numZeros);

            /* note: when we reach here and the last byte being encoded is 0, we still
               need to output the last two bits, otherwise mstsc will crash */

            /* encode the nonzero value using GR coding */
            mag = (input < 0 ? -input : input); /* absolute value of input coefficient */
            sign = (input < 0 ? 1 : 0);  /* sign of input coefficient */

            OutputBit(1, sign); /* output the sign bit */
            lmag = mag ? mag - 1 : 0;
            CodeGR(krp, lmag); /* output GR code for (mag - 1) */

            UpdateParam(kp, -DN_GR, k);
        }
        else
        {
            /* GOLOMB-RICE MODE */

            /* RLGR1 variant */

            /* convert input to (2*magnitude - sign), encode using GR code */
            GetNextInput(input);
            twoMs = Get2MagSign(input);
            CodeGR(krp, twoMs);

            /* update k, kp */
            /* NOTE: as of Aug 2011, the algorithm is still wrongly documented
               and the update direction is reversed */
            if (twoMs)
            {
                UpdateParam(kp, -DQ_GR, k);
            }
            else
            {
                UpdateParam(kp, UQ_GR, k);
            }

        }
    }

    processed_size = rfx_bitstream_get_processed_bytes(bs);

    return processed_size;
}
