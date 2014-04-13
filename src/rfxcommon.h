/**
 * RFX codec
 *
 * Copyright 2014 Jay Sorg <jay.sorg@gmail.com>
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

#ifndef __RFXCOMMON_H
#define __RFXCOMMON_H

#define MIN(_val1, _val2) (_val1) < (_val2) ? (_val1) : (_val2)
#define MAX(_val1, _val2) (_val1) > (_val2) ? (_val1) : (_val2)
#define MINMAX(_v, _l, _h) ((_v) < (_l) ? (_l) : ((_v) > (_h) ? (_h) : (_v)))

typedef signed char sint8;
typedef unsigned char uint8;
typedef signed short sint16;
typedef unsigned short uint16;
typedef signed int sint32;
typedef unsigned int uint32;

struct _STREAM
{
    uint8 *data;
    uint8 *p;
    int size;
};
typedef struct _STREAM STREAM;

#if defined(__x86__) || defined(__x86_64__) || \
    defined(__AMD64__) || defined(_M_IX86) || \
    defined(__i386__)
#define stream_read_uint8(_s, _v) do { _v = ((uint8*)((_s)->p))[0]; (_s)->p += 1; } while (0)
#define stream_read_uint16(_s, _v) do { _v = ((uint16*)((_s)->p))[0]; (_s)->p += 2; } while (0)
#define stream_read_uint32(_s, _v) do { _v = ((uint32*)((_s)->p))[0]; (_s)->p += 4; } while (0)
#define stream_write_uint8(_s, _v) do { ((uint8*)((_s)->p))[0] = _v; (_s)->p += 1; } while (0)
#define stream_write_uint16(_s, _v) do { ((uint16*)((_s)->p))[0] = _v; (_s)->p += 2; } while (0)
#define stream_write_uint32(_s, _v) do { ((uint32*)((_s)->p))[0] = _v; (_s)->p += 4; } while (0)
#else
#warning todo
#endif

#define stream_seek(_s, _n) (_s)->p += _n
#define stream_seek_uint8(_s) (_s)->p += 1
#define stream_seek_uint16(_s) (_s)->p += 2
#define stream_seek_uint32(_s) (_s)->p += 4

#define stream_get_pos(_s) ((int)((_s)->p - (_s)->data))
#define stream_get_tail(_s) (_s)->p
#define stream_get_left(_s) ((_s)->size - ((_s)->p - (_s)->data))
#define stream_set_pos(_s, _m) (_s)->p = (_s)->data + (_m)

#define xnew(_type) (_type *) calloc(1, sizeof(_type))

#endif
