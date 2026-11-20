/**
 * xrdp: A Remote Desktop Protocol server.
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
 * sample rate conversion of 16 bit stereo audio
 *
 * The sound servers exchange 44100 Hz audio with chansrv, but Opus only
 * runs at 8, 12, 16, 24 or 48 kHz. This converts between such rates with a
 * polyphase windowed-sinc low pass filter: for out_rate / in_rate = up / down,
 * output frame k lies at input position k * down / up, and each of the 'up'
 * phases has its own row of filter taps.
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <math.h>

#include "defines.h"
#include "os_calls.h"
#include "resample.h"

#define RS_TAPS 32 /* filter taps per output frame */
#define RS_MAX_PHASES 1000

struct resampler
{
    int up;
    int down;
    float *coef; /* up rows of RS_TAPS taps */
    /* input frames not consumed yet, starting with the filter history */
    short *in;
    int in_frames;
    int in_size; /* in frames */
    /* position of the next output frame in 'in', in 1 / up frames */
    int pos;
};

/*****************************************************************************/
/* modified Bessel function of the first kind, order 0, for the Kaiser window */
static double
bessel_i0(double x)
{
    double sum = 1.0;
    double term = 1.0;
    int k;

    for (k = 1; k < 32; k++)
    {
        term *= (x / (2 * k)) * (x / (2 * k));
        sum += term;
    }
    return sum;
}

/*****************************************************************************/
static int
gcd(int a, int b)
{
    int t;

    while (b != 0)
    {
        t = a % b;
        a = b;
        b = t;
    }
    return a;
}

/*****************************************************************************/
struct resampler *
resampler_create(int in_rate, int out_rate)
{
    struct resampler *rs;
    /* low pass at 21 kHz for 44.1 kHz: 95 % of the lower Nyquist
       frequency, Kaiser window beta 7, about 70 dB stop band */
    const double edge = MIN(in_rate, out_rate) * 21000.0 / 44100.0;
    const double cutoff = 2 * edge / in_rate;
    const double beta = 7.0;
    const double half = RS_TAPS / 2;
    double sum;
    double x;
    double t;
    double c;
    int phase;
    int tap;
    int g;

    if (in_rate <= 0 || out_rate <= 0)
    {
        return NULL;
    }
    g = gcd(in_rate, out_rate);
    if (out_rate / g > RS_MAX_PHASES)
    {
        return NULL;
    }
    rs = g_new0(struct resampler, 1);
    if (rs == NULL)
    {
        return NULL;
    }
    rs->up = out_rate / g;
    rs->down = in_rate / g;
    rs->coef = g_new0(float, rs->up * RS_TAPS);
    rs->in_size = RS_TAPS - 1 + 4096;
    rs->in = g_new0(short, rs->in_size * 2);
    if (rs->coef == NULL || rs->in == NULL)
    {
        resampler_delete(rs);
        return NULL;
    }

    for (phase = 0; phase < rs->up; phase++)
    {
        sum = 0;
        for (tap = 0; tap < RS_TAPS; tap++)
        {
            /* distance between this output frame and input frame 'tap' */
            x = (double) phase / rs->up + (half - 1) - tap;
            t = x / half;
            c = cutoff;
            if (x != 0)
            {
                c = sin(M_PI * cutoff * x) / (M_PI * x);
            }
            c *= bessel_i0(beta * sqrt(MAX(0.0, 1 - t * t))) / bessel_i0(beta);
            rs->coef[phase * RS_TAPS + tap] = c;
            sum += c;
        }
        /* unity gain at DC for every phase */
        for (tap = 0; tap < RS_TAPS; tap++)
        {
            rs->coef[phase * RS_TAPS + tap] /= sum;
        }
    }
    resampler_reset(rs);
    return rs;
}

/*****************************************************************************/
void
resampler_delete(struct resampler *rs)
{
    if (rs != NULL)
    {
        g_free(rs->coef);
        g_free(rs->in);
        g_free(rs);
    }
}

/*****************************************************************************/
void
resampler_reset(struct resampler *rs)
{
    /* start with silence as the filter history, so that a whole number of
       periods in gives exactly the matching number of frames out
       (e.g. 2646 frames in at 44100 Hz give 2880 out at 48000 Hz) */
    g_memset(rs->in, 0, (RS_TAPS - 1) * 4);
    rs->in_frames = RS_TAPS - 1;
    rs->pos = 0;
}

/*****************************************************************************/
int
resampler_process(struct resampler *rs, const short *in, int in_frames,
                  short *out, int max_out_frames)
{
    const short *src;
    const float *coef;
    short *bigger;
    float left;
    float right;
    int out_frames;
    int used;
    int index;
    int tap;

    if (in_frames > rs->in_size - rs->in_frames)
    {
        bigger = g_new(short, (rs->in_frames + in_frames) * 2);
        if (bigger == NULL)
        {
            in_frames = rs->in_size - rs->in_frames;
        }
        else
        {
            g_memcpy(bigger, rs->in, rs->in_frames * 4);
            g_free(rs->in);
            rs->in = bigger;
            rs->in_size = rs->in_frames + in_frames;
        }
    }
    g_memcpy(rs->in + rs->in_frames * 2, in, in_frames * 4);
    rs->in_frames += in_frames;

    out_frames = 0;
    while (out_frames < max_out_frames)
    {
        index = rs->pos / rs->up;
        if (index + RS_TAPS > rs->in_frames)
        {
            break;
        }
        src = rs->in + index * 2;
        coef = rs->coef + (rs->pos % rs->up) * RS_TAPS;
        left = 0;
        right = 0;
        for (tap = 0; tap < RS_TAPS; tap++)
        {
            left += coef[tap] * src[tap * 2];
            right += coef[tap] * src[tap * 2 + 1];
        }
        left = MAX(-32768.0f, MIN(32767.0f, left));
        right = MAX(-32768.0f, MIN(32767.0f, right));
        out[out_frames * 2] = (short) lrintf(left);
        out[out_frames * 2 + 1] = (short) lrintf(right);
        out_frames++;
        rs->pos += rs->down;
    }

    /* drop the input frames no output frame needs any more */
    used = MIN(rs->pos / rs->up, rs->in_frames);
    rs->in_frames -= used;
    g_memmove(rs->in, rs->in + used * 2, rs->in_frames * 4);
    rs->pos -= used * rs->up;
    return out_frames;
}
