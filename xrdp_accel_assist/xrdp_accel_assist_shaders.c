/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2022-2024
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

/* GLSL shaders
 * this file is not compiled directly, it is included in
 * xrdp_accel_assist_x11.c */

static const GLchar g_vs[] = "\
attribute vec4 position;\n\
void main(void)\n\
{\n\
    gl_Position = vec4(position.xy, 0.0, 1.0);\n\
}\n";

static const GLchar g_fs_copy[] = "\
uniform sampler2D tex;\n\
uniform vec2 tex_size;\n\
void main(void)\n\
{\n\
    gl_FragColor = texture2D(tex, gl_FragCoord.xy / tex_size);\n\
}\n";

static const GLchar g_fs_rgb_to_yuv420[] = "\
uniform sampler2D tex;\n\
uniform vec2 tex_size;\n\
uniform vec4 ymath;\n\
uniform vec4 umath;\n\
uniform vec4 vmath;\n\
void main(void)\n\
{\n\
    vec4 pix;\n\
    float x;\n\
    float y;\n\
    x = gl_FragCoord.x;\n\
    y = gl_FragCoord.y;\n\
    if (y < tex_size.y)\n\
    {\n\
        pix = texture2D(tex, vec2(x, y) / tex_size);\n\
        pix.a = 1.0;\n\
        pix = vec4(clamp(dot(ymath, pix), 0.0, 1.0), 0.0, 0.0, 1.0);\n\
        gl_FragColor = pix;\n\
    }\n\
    else\n\
    {\n\
        y = floor(y - tex_size.y) * 2.0 + 0.5;\n\
        if (mod(x, 2.0) < 1.0)\n\
        {\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix += texture2D(tex, vec2(x + 1.0, y) / tex_size);\n\
            pix += texture2D(tex, vec2(x, y + 1.0) / tex_size);\n\
            pix += texture2D(tex, vec2(x + 1.0, y + 1.0) / tex_size);\n\
            pix /= 4.0;\n\
            pix.a = 1.0;\n\
            pix = vec4(clamp(dot(umath, pix), 0.0, 1.0), 0.0, 0.0, 1.0);\n\
            gl_FragColor = pix;\n\
        }\n\
        else\n\
        {\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix += texture2D(tex, vec2(x - 1.0, y) / tex_size);\n\
            pix += texture2D(tex, vec2(x, y + 1.0) / tex_size);\n\
            pix += texture2D(tex, vec2(x - 1.0, y + 1.0) / tex_size);\n\
            pix /= 4.0;\n\
            pix.a = 1.0;\n\
            pix = vec4(clamp(dot(vmath, pix), 0.0, 1.0), 0.0, 0.0, 1.0);\n\
            gl_FragColor = pix;\n\
        }\n\
    }\n\
}\n";

/* UV plane only, U in red, V in green, for a GR88 target */
static const GLchar g_fs_rgb_to_uv420[] = "\
uniform sampler2D tex;\n\
uniform vec2 tex_size;\n\
uniform vec4 umath;\n\
uniform vec4 vmath;\n\
void main(void)\n\
{\n\
    vec4 pix;\n\
    float x;\n\
    float y;\n\
    x = floor(gl_FragCoord.x) * 2.0 + 0.5;\n\
    y = floor(gl_FragCoord.y) * 2.0 + 0.5;\n\
    pix = texture2D(tex, vec2(x, y) / tex_size);\n\
    pix += texture2D(tex, vec2(x + 1.0, y) / tex_size);\n\
    pix += texture2D(tex, vec2(x, y + 1.0) / tex_size);\n\
    pix += texture2D(tex, vec2(x + 1.0, y + 1.0) / tex_size);\n\
    pix /= 4.0;\n\
    pix.a = 1.0;\n\
    pix = vec4(dot(umath, pix), dot(vmath, pix), 0.0, 1.0);\n\
    gl_FragColor = clamp(pix, 0.0, 1.0);\n\
}\n";

static const GLchar g_fs_rgb_to_yuv422[] = "\
uniform sampler2D tex;\n\
uniform vec2 tex_size;\n\
uniform vec4 ymath;\n\
uniform vec4 umath;\n\
uniform vec4 vmath;\n\
void main(void)\n\
{\n\
    vec4 pix;\n\
    vec4 pix1;\n\
    vec4 pixs;\n\
    float x;\n\
    float y;\n\
    x = gl_FragCoord.x;\n\
    x = floor(x) * 2.0 + 0.5;\n\
    y = gl_FragCoord.y;\n\
    pix = texture2D(tex, vec2(x, y) / tex_size);\n\
    pix1 = texture2D(tex, vec2(x + 1.0, y) / tex_size);\n\
    pixs = (pix + pix1) / 2.0;\n\
    pix.a = 1.0;\n\
    pix1.a = 1.0;\n\
    pixs.a = 1.0;\n\
    pix.r = dot(ymath, pix);\n\
    pix.g = dot(umath, pixs);\n\
    pix.b = dot(ymath, pix1);\n\
    pix.a = dot(vmath, pixs);\n\
    gl_FragColor = clamp(pix, 0.0, 1.0);\n\
}\n";

static const GLchar g_fs_rgb_to_yuv444[] = "\
uniform sampler2D tex;\n\
uniform vec2 tex_size;\n\
uniform vec4 ymath;\n\
uniform vec4 umath;\n\
uniform vec4 vmath;\n\
void main(void)\n\
{\n\
    vec4 pix;\n\
    pix = texture2D(tex, gl_FragCoord.xy / tex_size);\n\
    pix.a = 1.0;\n\
    pix = vec4(dot(vmath, pix), dot(umath, pix), dot(ymath, pix), 1.0);\n\
    gl_FragColor = clamp(pix, 0.0, 1.0);\n\
}\n";

/*
RGB
    00 10 20 30 40 50 60 70 80 90 A0 B0 C0 D0 E0 F0
    01 11 21 31 41 51 61 71 81 91 A1 B1 C1 D1 E1 F1
    02 12 22 32 42 52 62 72 82 92 A2 B2 C2 D2 E2 F2
    03 13 23 33 43 53 63 73 83 93 A3 B3 C3 D3 E3 F3
    04 14 24 34 44 54 64 74 84 94 A4 B4 C4 D4 E4 F4
    05 15 25 35 45 55 65 75 85 95 A5 B5 C5 D5 E5 F5
    06 16 26 36 46 56 66 76 86 96 A6 B6 C6 D6 E6 F6
    07 17 27 37 47 57 67 77 87 97 A7 B7 C7 D7 E7 F7
    08 18 28 38 48 58 68 78 88 98 A8 B8 C8 D8 E8 F8
    09 19 29 39 49 59 69 79 89 99 A9 B9 C9 D9 E9 F9
    0A 1A 2A 3A 4A 5A 6A 7A 8A 9A AA BA CA DA EA FA
    0B 1B 2B 3B 4B 5B 6B 7B 8B 9B AB BB CB DB EB FB
    0C 1C 2C 3C 4C 5C 6C 7C 8C 9C AC BC CC DC EC FC
    0D 1D 2D 3D 4D 5D 6D 7D 8D 9D AD BD CD DD ED FD
    0E 1E 2E 3E 4E 5E 6E 7E 8E 9E AE BE CE DE EE FE
    0F 1F 2F 3F 4F 5F 6F 7F 8F 9F AF BF CF DF EF FF

MAIN VIEW - NV12

    /---------------------Y-----------------------\
    00 10 20 30 40 50 60 70 80 90 A0 B0 C0 D0 E0 F0
    01 11 21 31 41 51 61 71 81 91 A1 B1 C1 D1 E1 F1
    ...
    0F 1F 2F 3F 4F 5F 6F 7F 8F 9F AF BF CF DF EF FF

    /U /V /U /V /U /V /U /V /U /V /U /V /U /V /U /V
    00 00 20 20 40 40 60 60 80 80 A0 A0 C0 C0 E0 E0
    02 02 22 22 42 42 62 62 82 82 A2 A2 C2 C2 E2 E2
    ...
    0E 0E 2E 2E 4E 4E 6E 6E 8E 8E AE AE CE CE EE EE
*/
static const GLchar g_fs_rgb_to_yuv420_mv[] = "\
uniform sampler2D tex;\n\
uniform vec2 tex_size;\n\
uniform vec4 ymath;\n\
uniform vec4 umath;\n\
uniform vec4 vmath;\n\
void main(void)\n\
{\n\
    vec4 pix;\n\
    float x;\n\
    float y;\n\
    x = gl_FragCoord.x;\n\
    y = gl_FragCoord.y;\n\
    if (y < tex_size.y)\n\
    {\n\
        pix = texture2D(tex, vec2(x, y) / tex_size);\n\
        pix.a = 1.0;\n\
        pix = vec4(clamp(dot(ymath, pix), 0.0, 1.0), 0.0, 0.0, 1.0);\n\
        gl_FragColor = pix;\n\
    }\n\
    else\n\
    {\n\
        y = floor(y - tex_size.y) * 2.0 + 0.5;\n\
        if (mod(x, 2.0) < 1.0)\n\
        {\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix.a = 1.0;\n\
            pix = vec4(clamp(dot(umath, pix), 0.0, 1.0), 0.0, 0.0, 1.0);\n\
            gl_FragColor = pix;\n\
        }\n\
        else\n\
        {\n\
            pix = texture2D(tex, vec2(x - 1.0, y) / tex_size);\n\
            pix.a = 1.0;\n\
            pix = vec4(clamp(dot(vmath, pix), 0.0, 1.0), 0.0, 0.0, 1.0);\n\
            gl_FragColor = pix;\n\
        }\n\
    }\n\
}\n";

/*
AUXILIARY VIEW - NV12

    /---------------------U-----------------------\
    01 11 21 31 41 51 61 71 81 91 A1 B1 C1 D1 E1 F1
    03 13 23 33 43 53 63 73 83 93 A3 B3 C3 D3 E3 F4
    ...
    0F 1F 2F 3F 4F 5F 6F 7F 8F 9F AF BF CF DF EF FF
    /---------------------V-----------------------\
    01 11 21 31 41 51 61 71 81 91 A1 B1 C1 D1 E1 F1
    03 13 23 33 43 53 63 73 83 93 A3 B3 C3 D3 E3 F4
    ...
    0F 1F 2F 3F 4F 5F 6F 7F 8F 9F AF BF CF DF EF FF
    ... (8 LINES U, 8 LINES V, REPEAT)

    /U /V /U /V /U /V /U /V /U /V /U /V /U /V /U /V
    10 10 30 30 50 50 70 70 90 90 B0 B0 D0 D0 F0 F0
    12 12 32 32 52 52 72 72 92 92 B2 B2 D2 D2 F2 F2
    ...
    1E 1E 3E 3E 5E 5E 7E 7E 9E 9E BE BE DE DE FE FE
*/
static const GLchar g_fs_rgb_to_yuv420_av[] = "\
uniform sampler2D tex;\n\
uniform vec2 tex_size;\n\
uniform vec4 umath;\n\
uniform vec4 vmath;\n\
void main(void)\n\
{\n\
    vec4 pix;\n\
    float x;\n\
    float y;\n\
    float y1;\n\
    x = gl_FragCoord.x;\n\
    y = gl_FragCoord.y;\n\
    if (y < tex_size.y)\n\
    {\n\
        y1 = mod(y, 16.0);\n\
        if (y1 < 8.0)\n\
        {\n\
            y = floor(y / 16.0) * 8.0 + y1;\n\
            y = floor(y) * 2.0 + 1.5;\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix.a = 1.0;\n\
            pix = vec4(clamp(dot(umath, pix), 0.0, 1.0), 0.0, 0.0, 1.0);\n\
            gl_FragColor = pix;\n\
        }\n\
        else\n\
        {\n\
            y = floor(y / 16.0) * 8.0 + (y1 - 8.0);\n\
            y = floor(y) * 2.0 + 1.5;\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix.a = 1.0;\n\
            pix = vec4(clamp(dot(vmath, pix), 0.0, 1.0), 0.0, 0.0, 1.0);\n\
            gl_FragColor = pix;\n\
        }\n\
    }\n\
    else\n\
    {\n\
        y = floor(y - tex_size.y) * 2.0 + 0.5;\n\
        if (mod(x, 2.0) < 1.0)\n\
        {\n\
            pix = texture2D(tex, vec2(x + 1.0, y) / tex_size);\n\
            pix.a = 1.0;\n\
            pix = vec4(clamp(dot(umath, pix), 0.0, 1.0), 0.0, 0.0, 1.0);\n\
            gl_FragColor = pix;\n\
        }\n\
        else\n\
        {\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix.a = 1.0;\n\
            pix = vec4(clamp(dot(vmath, pix), 0.0, 1.0), 0.0, 0.0, 1.0);\n\
            gl_FragColor = pix;\n\
        }\n\
    }\n\
}\n";

/*
AUXILIARY VIEW V2 - NV12

    /----------U----------\ /----------V----------\
    10 30 50 70 90 B0 D0 F0 10 30 50 70 90 B0 D0 F0
    11 31 51 71 91 B1 D1 F1 11 31 51 71 91 B1 D1 F1
    ...
    1F 3F 5F 7F 9F BF DF FF 1F 3F 5F 7F 9F BF DF FF

    /----------U----------\ /----------V----------\
    01 21 41 61 81 A1 C1 E1 01 21 41 61 81 A1 C1 E1
    03 23 43 63 83 A3 C3 E3 03 23 43 63 83 A3 C3 E3
    ...
    0F 2F 4F 6F 8F AF CF EF 0F 2F 4F 6F 8F AF CF EF
*/
static const GLchar g_fs_rgb_to_yuv420_av_v2[] = "\
uniform sampler2D tex;\n\
uniform vec2 tex_size;\n\
uniform vec4 umath;\n\
uniform vec4 vmath;\n\
void main(void)\n\
{\n\
    vec4 pix;\n\
    float x;\n\
    float y;\n\
    float x1;\n\
    x = gl_FragCoord.x;\n\
    y = gl_FragCoord.y;\n\
    x1 = tex_size.x / 2.0;\n\
    if (y < tex_size.y)\n\
    {\n\
        if (x < x1)\n\
        {\n\
            x = floor(x) * 2.0 + 1.5;\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix.a = 1.0;\n\
            pix = vec4(clamp(dot(umath, pix), 0.0, 1.0), 0.0, 0.0, 1.0);\n\
            gl_FragColor = pix;\n\
        }\n\
        else\n\
        {\n\
            x = floor(x - x1) * 2.0 + 1.5;\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix.a = 1.0;\n\
            pix = vec4(clamp(dot(vmath, pix), 0.0, 1.0), 0.0, 0.0, 1.0);\n\
            gl_FragColor = pix;\n\
        }\n\
    }\n\
    else\n\
    {\n\
        y = floor(y - tex_size.y) * 2.0 + 1.5;\n\
        if (x < x1)\n\
        {\n\
            x = floor(x) * 2.0 + 0.5;\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix.a = 1.0;\n\
            pix = vec4(clamp(dot(umath, pix), 0.0, 1.0), 0.0, 0.0, 1.0);\n\
            gl_FragColor = pix;\n\
        }\n\
        else\n\
        {\n\
            x = floor(x - x1) * 2.0 + 0.5;\n\
            pix = texture2D(tex, vec2(x, y) / tex_size);\n\
            pix.a = 1.0;\n\
            pix = vec4(clamp(dot(vmath, pix), 0.0, 1.0), 0.0, 0.0, 1.0);\n\
            gl_FragColor = pix;\n\
        }\n\
    }\n\
}\n";
