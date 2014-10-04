/*
Copyright 2014 Jay Sorg

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

amd64 asm files

*/

#ifndef __FUNCS_AMD64_H
#define __FUNCS_AMD64_H

int
cpuid_amd64(int eax_in, int ecx_in, int *eax, int *ebx, int *ecx, int *edx);
int
yv12_to_rgb32_amd64_sse2(unsigned char *yuvs, int width, int height, int *rgbs);
int
i420_to_rgb32_amd64_sse2(unsigned char *yuvs, int width, int height, int *rgbs);
int
yuy2_to_rgb32_amd64_sse2(unsigned char *yuvs, int width, int height, int *rgbs);
int
uyvy_to_rgb32_amd64_sse2(unsigned char *yuvs, int width, int height, int *rgbs);

#endif

