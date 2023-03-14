/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * MS-RDPEDISP : Definitions from [MS-RDPEDISP]
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
 * References to MS-RDPEDISP are currently correct for v20201030 of that
 * document
 */

#if !defined(MS_RDPEDISP_H)
#define MS_RDPEDISP_H

/* Display Control Messages: Display Virtual Channel Extension (2.2.2) */
#define DISPLAYCONTROL_PDU_TYPE_MONITOR_LAYOUT             0x00000002
#define DISPLAYCONTROL_PDU_TYPE_CAPS                       0x00000005

/* Display Control Monitor Layout (2.2.2.2.1) */
#define DISPLAYCONTROL_MONITOR_PRIMARY                     0x00000001
#define CLIENT_MONITOR_DATA_MINIMUM_VIRTUAL_MONITOR_WIDTH  0xC8
#define CLIENT_MONITOR_DATA_MINIMUM_VIRTUAL_MONITOR_HEIGHT 0xC8
#define CLIENT_MONITOR_DATA_MAXIMUM_VIRTUAL_MONITOR_WIDTH  0x2000
#define CLIENT_MONITOR_DATA_MAXIMUM_VIRTUAL_MONITOR_HEIGHT 0x2000

#define ORIENTATION_LANDSCAPE                              0
#define ORIENTATION_PORTRAIT                               90
#define ORIENTATION_LANDSCAPE_FLIPPED                      180
#define ORIENTATION_PORTRAIT_FLIPPED                       270

/* Display Control Monitor Layout (2.2.2.2.1) */
#define DISPLAYCONTROL_MONITOR_PRIMARY                     0x00000001

#endif /* MS_RDPEDISP_H */
