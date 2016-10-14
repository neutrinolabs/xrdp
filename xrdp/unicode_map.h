/*
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Ricardo Duarte 2016
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
 */

/* map for unicode to rdp scancodes */
struct unicodecodepair
{
    tui8 scancode;
    tui8 shift;
};

static struct unicodecodepair u_map[] =
{
    { 0 , 0 }, { 0 , 0 }, { 0 , 0 }, { 0 , 0 }, { 0 , 0 }, /* 0 - 4 */
    { 0 , 0 }, { 0 , 0 }, { 0 , 0 }, { 14 , 1 }, { 15 , 0 }, /* 5 - 9 */
    { 0 , 0 }, { 0 , 0 }, { 0 , 0 }, { 28 , 1 }, { 0 , 0 }, /* 10 - 14 */
    { 15 , 1 }, { 0 , 0 }, { 0 , 0 }, { 0 , 0 }, { 0 , 0 }, /* 15 - 19 */
    { 0 , 0 }, { 0 , 0 }, { 0 , 0 }, { 0 , 0 }, { 0 , 0 }, /* 20 - 24 */
    { 0 , 0 }, { 0 , 0 }, { 1 , 1 }, { 0 , 0 }, { 0 , 0 }, /* 25 - 29 */
    { 0 , 0 }, { 0 , 0 }, { 57 , 1 }, { 2 , 1 }, { 40 , 1 }, /* 30 - 34 */
    { 4 , 1 }, { 5 , 1 }, { 6 , 1 }, { 8 , 1 }, { 40 , 0 }, /* 35 - 39 */
    { 10 , 1 }, { 11 , 1 }, { 9 , 1 }, { 13 , 1 }, { 51 , 0 }, /* 40 - 44 */
    { 74 , 1 }, { 83 , 1 }, { 53 , 0 }, { 82 , 1 }, { 79 , 1 }, /* 45 - 49 */
    { 80 , 1 }, { 81 , 1 }, { 75 , 1 }, { 76 , 1 }, { 77 , 1 }, /* 50 - 54 */
    { 8 , 0 }, { 9 , 0 }, { 10 , 0 }, { 0 , 0 }, { 39 , 0 }, /* 55 - 59 */
    { 51 , 1 }, { 13 , 0 }, { 52 , 1 }, { 53 , 1 }, { 3 , 1 }, /* 60 - 64 */
    { 30 , 1 }, { 48 , 1 }, { 46 , 1 }, { 32 , 1 }, { 18 , 1 }, /* 65 - 69 */
    { 33 , 1 }, { 34 , 1 }, { 35 , 1 }, { 23 , 1 }, { 36 , 1 }, /* 70 - 74 */
    { 37 , 1 }, { 38 , 1 }, { 50 , 1 }, { 49 , 1 }, { 24 , 1 }, /* 75 - 79 */
    { 25 , 1 }, { 16 , 1 }, { 19 , 1 }, { 31 , 1 }, { 20 , 1 }, /* 80 - 84 */
    { 22 , 1 }, { 47 , 1 }, { 17 , 1 }, { 45 , 1 }, { 21 , 1 }, /* 85 - 89 */
    { 44 , 1 }, { 66 , 1 }, { 43 , 0 }, { 68 , 1 }, { 7 , 1 }, /* 90 - 94 */
    { 12 , 1 }, { 41 , 0 }, { 30 , 0 }, { 48 , 0 }, { 46 , 0 }, /* 95 - 99 */
    { 32 , 0 }, { 18 , 0 }, { 33 , 0 }, { 34 , 0 }, { 35 , 0 }, /* 100 - 104 */
    { 23 , 0 }, { 36 , 0 }, { 37 , 0 }, { 38 , 0 }, { 50 , 0 }, /* 105 - 109 */
    { 49 , 0 }, { 24 , 0 }, { 25 , 0 }, { 16 , 0 }, { 19 , 0 }, /* 110 - 114 */
    { 31 , 0 }, { 20 , 0 }, { 22 , 0 }, { 47 , 0 }, { 17 , 0 }, /* 115 - 119 */
    { 45 , 0 }, { 21 , 0 }, { 44 , 0 }, { 26 , 1 }, { 43 , 1 }, /* 120 - 124 */
    { 27 , 1 }, { 41 , 1 }, { 0 , 0 }, { 0 , 0 }, { 0 , 0 }, /* 125 - 129 */
    { 0 , 0 }, { 0 , 0 }, { 0 , 0 }, { 87 , 0 }, { 88 , 0 }, /* 130 - 134 */
    { 87 , 1 }, { 88 , 1 } /* 135 - 136 */
};
