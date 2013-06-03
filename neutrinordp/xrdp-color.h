/**
 * FreeRDP: A Remote Desktop Protocol Server
 * freerdp wrapper
 *
 * Copyright 2011-2012 Jay Sorg
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

#ifndef __XRDP_COLOR_H
#define __XRDP_COLOR_H

char* APP_CC
convert_bitmap(int in_bpp, int out_bpp, char* bmpdata,
               int width, int height, int* palette);
int APP_CC
convert_color(int in_bpp, int out_bpp, int in_color, int* palette);

#endif
