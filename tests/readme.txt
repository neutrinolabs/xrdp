
this directory contains different tests

OpenH264 UV conversion performance
---------------------------------

After configuring with --enable-tests --enable-openh264 and building the
repository, run this optional benchmark:

    make -C tests/xrdp benchmark-openh264

It is not run by make check and has no machine-dependent performance pass/fail
threshold. It compares the linked production conversion helper with two fixed
baselines: the original scalar loop, and the first SSE2 implementation (32-byte
blocks with a scalar tail). Without SSE2, the latter baseline uses its scalar
fallback. All implementations use the configured compiler optimization flags;
baselines and the driver are separate translation units.

Each case first checks U/V output, row padding, and allocation guards. It then
warms and calibrates each implementation to batches lasting at least 20 ms, and
reports medians from 11 batches with alternating execution order. The current
implementation also reports its minimum and maximum batch averages. Times are
microseconds per frame's chroma plane, and dimensions are luma pixels. Cases
include small rectangles, odd widths, HD/4K rows, and offset/padded buffers.

For correctness checks without timing:

    make -C tests/xrdp bench_openh264
    tests/xrdp/bench_openh264 --verify-only

For reproducible timing, stop other CPU-heavy work, use the same compiler flags
for every implementation, and repeat runs. On Linux, optionally pin the process
to one CPU permitted by your environment, for example:

    taskset -c <cpu-number> tests/xrdp/bench_openh264

This benchmark reuses frame buffers. Its speedups apply to UV splitting, not
complete OpenH264 encoding or an RDP session; larger streaming working sets may
behave differently. Do not compare sanitizer/debug builds with optimized builds.
