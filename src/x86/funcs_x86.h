/*
Copyright 2014-2015 Jay Sorg

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

x86 asm files

*/

#ifndef __FUNCS_X86_H
#define __FUNCS_X86_H

int
cpuid_x86(int eax_in, int ecx_in, int *eax, int *ebx, int *ecx, int *edx);

int
rfxcodec_encode_dwt_shift_x86_sse2(const char *qtable,
                                   unsigned char *data,
                                   short *dwt_buffer1,
                                   short *dwt_buffer);
int
rfxcodec_encode_dwt_shift_x86_sse41(const char *qtable,
                                    unsigned char *data,
                                    short *dwt_buffer1,
                                    short *dwt_buffer);

int
rfxcodec_encode_diff_rlgr1_x86_sse2(short *co,
                                    void *dst, int dst_bytes);
int
rfxcodec_encode_diff_rlgr3_x86_sse2(short *co,
                                    void *dst, int dst_bytes);

int
rfxcodec_decode_rlgr1_diff_x86_sse2(void *data, int data_bytes,
                                    short *out_data);
int
rfxcodec_decode_rlgr3_diff_x86_sse2(void *data, int data_bytes,
                                    short *out_data);
int
rfxcodec_decode_shift_idwt_x86_sse2(const char *qtable, short *src, short *dst);
int
rfxcodec_decode_yuv2rgb_x86_sse2(short *ydata, short *udata, short *vdata,
                                 unsigned int *rgbdata, int stride);
int
rfxcodec_decode_yuva2argb_x86_sse2(short *ydata, short *udata,
                                   short *vdata, char *adata,
                                   unsigned int *rgbdata, int stride);

#endif

