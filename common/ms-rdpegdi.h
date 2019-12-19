/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * MS-RDPEGDI : Definitions from [MS-RDPEGDI]
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
 *
 * References to MS-RDPEGDI are currently correct for v20180912 of that
 * document
 */

#if !defined(MS_RDPEGDI_H)
#define MS_RDPEGDI_H

/* Drawing Order: controlFlags (2.2.2.2.1, 2.2.2.2.1.1.2) */
#define TS_STANDARD                     0x01
#define TS_SECONDARY                    0x02
#define TS_BOUNDS                       0x04
#define TS_TYPE_CHANGE                  0x08
#define TS_DELTA_COORDINATES            0x10
#define TS_ZERO_BOUNDS_DELTAS           0x20
#define TS_ZERO_FIELD_BYTE_BIT0         0x40
#define TS_ZERO_FIELD_BYTE_BIT1         0x80

/* Drawing Order: orderType (2.2.2.2.1.1.2) */
/* Should be renamed */
#define RDP_ORDER_DESTBLT   0 /* TS_ENC_DSTBLT_ORDER */
#define RDP_ORDER_PATBLT    1
#define RDP_ORDER_SCREENBLT 2
#define RDP_ORDER_LINE      9
#define RDP_ORDER_RECT      10
#define RDP_ORDER_DESKSAVE  11
#define RDP_ORDER_MEMBLT    13
#define RDP_ORDER_TRIBLT    14
#define RDP_ORDER_POLYLINE  22
#define RDP_ORDER_TEXT2     27
#define RDP_ORDER_COMPOSITE 37 /* 0x25  - not defined in RDPEGDI */

/* Secondary Drawing Order Header: orderType (2.2.2.2.1.2.1.1) */
#define TS_CACHE_BITMAP_UNCOMPRESSED        0x00
#define TS_CACHE_COLOR_TABLE                0x01
#define TS_CACHE_BITMAP_COMPRESSED          0x02
#define TS_CACHE_GLYPH                      0x03
#define TS_CACHE_BITMAP_UNCOMPRESSED_REV2   0x04
#define TS_CACHE_BITMAP_COMPRESSED_REV2     0x05
#define TS_CACHE_BRUSH                      0x07
#define TS_CACHE_BITMAP_COMPRESSED_REV3     0x08

#endif /* MS_RDPEGDI_H */
