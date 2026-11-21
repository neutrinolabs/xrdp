/* Fixed baselines for the optional OpenH264 UV conversion benchmark. */

#ifndef BENCH_OPENH264_REF_H
#define BENCH_OPENH264_REF_H

void
bench_openh264_scalar(const char *src, char *dst_u, char *dst_v, int width);
void
bench_openh264_previous(const char *src, char *dst_u, char *dst_v, int width);

#endif
