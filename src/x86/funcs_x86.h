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

x86 asm files

*/

#ifndef __FUNCS_X86_H
#define __FUNCS_X86_H

int
cpuid_x86(int eax_in, int ecx_in, int *eax, int *ebx, int *ecx, int *edx);
int
dwt_shift_x86_sse2(const int *quantization_values, uint8 *data,
                   sint16 *dwt_buffer1, sint16 *dwt_buffer);
int
diff_rlgr1_x86(sint16 *co, int num_co, uint8 *dst, int dst_bytes);
int
diff_rlgr3_x86(sint16 *co, int num_co, uint8 *dst, int dst_bytes);

#endif

