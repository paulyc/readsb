// Part of readsb, a Mode-S/ADSB/TIS message decoder.
//
// convert.c: support for various IQ -> magnitude conversions
//
// Copyright (c) 2019 Michael Wolf <michael@mictronics.de>
//
// This code is based on a detached fork of dump1090-fa.
//
// Copyright (c) 2015 Oliver Jowett <oliver@mutability.co.uk>
//
// This file is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// This file is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "readsb.h"
#define UC8_ONLY 1

struct converter_state {
    mag_t dc_a;
    mag_t dc_b;
    mag_t z1_I;
    mag_t z1_Q;
};

static void convert_uc8(void *iq_data,
        mag_t *mag_data,
        unsigned nsamples,
        struct converter_state *state,
        double *out_mean_level,
        double *out_mean_power,
        int dcfilter) {
    uint8_t *in = iq_data;
    mag_t z1_I = state->z1_I;
    mag_t z1_Q = state->z1_Q;
    const mag_t dc_a = state->dc_a;
    const mag_t dc_b = state->dc_b;

    unsigned i;
    uint8_t I, Q;
    float fI, fQ, magsq;
    float sum_level = 0, sum_power = 0;

    for (i = 0; i < nsamples; ++i) {
        I = *in++;
        Q = *in++;
        int sI = I - 128, sQ = Q - 128;
        fI = sI > 0 ? sI / 127.0 : sI / 128.0;
        fQ = sQ > 0 ? sQ / 127.0 : sQ / 128.0;

        // DC block
        z1_I = fI * dc_a + z1_I * dc_b;
        z1_Q = fQ * dc_a + z1_Q * dc_b;
        if (dcfilter) {
            fI -= z1_I;
            fQ -= z1_Q;
        }

        magsq = fI * fI + fQ * fQ;
        if (magsq > 1.0)
            magsq = 1.0;

        mag_t mag = sqrt(magsq);
        sum_power += magsq;
        sum_level += mag;
        *mag_data++ = mag;
    }

    state->z1_I = z1_I;
    state->z1_Q = z1_Q;

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}

static void convert_uc8_generic(void *iq_data,
        mag_t *mag_data,
        unsigned nsamples,
        struct converter_state *state,
        double *out_mean_level,
        double *out_mean_power) {
    return convert_uc8(iq_data, mag_data, nsamples, state, out_mean_level, out_mean_power, 1);
}

static void convert_uc8_nodc(void *iq_data,
        mag_t *mag_data,
        unsigned nsamples,
        struct converter_state *state,
        double *out_mean_level,
        double *out_mean_power) {
    return convert_uc8(iq_data, mag_data, nsamples, state, out_mean_level, out_mean_power, 0);
}


#if !UC8_ONLY
static void convert_sc16_generic(void *iq_data,
        uint16_t *mag_data,
        unsigned nsamples,
        struct converter_state *state,
        double *out_mean_level,
        double *out_mean_power) {
    uint16_t *in = iq_data;
    float z1_I = state->z1_I;
    float z1_Q = state->z1_Q;
    const float dc_a = state->dc_a;
    const float dc_b = state->dc_b;

    unsigned i;
    int16_t I, Q;
    float fI, fQ, magsq;
    float sum_level = 0, sum_power = 0;

    for (i = 0; i < nsamples; ++i) {
        I = (int16_t) le16toh(*in++);
        Q = (int16_t) le16toh(*in++);
        fI = I / 32768.0f;
        fQ = Q / 32768.0f;

        // DC block
        z1_I = fI * dc_a + z1_I * dc_b;
        z1_Q = fQ * dc_a + z1_Q * dc_b;
        fI -= z1_I;
        fQ -= z1_Q;

        magsq = fI * fI + fQ * fQ;
        if (magsq > 1)
            magsq = 1;

        float mag = sqrtf(magsq);
        sum_power += magsq;
        sum_level += mag;
        *mag_data++ = (uint16_t) (mag * 65535.0f + 0.5f);
    }

    state->z1_I = z1_I;
    state->z1_Q = z1_Q;

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}

static void convert_sc16_nodc(void *iq_data,
        uint16_t *mag_data,
        unsigned nsamples,
        struct converter_state *state,
        double *out_mean_level,
        double *out_mean_power) {
    MODES_NOTUSED(state);

    uint16_t *in = iq_data;

    unsigned i;
    int16_t I, Q;
    float fI, fQ, magsq;
    float sum_level = 0, sum_power = 0;

    for (i = 0; i < nsamples; ++i) {
        I = (int16_t) le16toh(*in++);
        Q = (int16_t) le16toh(*in++);
        fI = I / 32768.0f;
        fQ = Q / 32768.0f;

        magsq = fI * fI + fQ * fQ;
        if (magsq > 1)
            magsq = 1;

        float mag = sqrtf(magsq);
        sum_power += magsq;
        sum_level += mag;
        *mag_data++ = (uint16_t) (mag * 65535.0f + 0.5f);
    }

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}

// SC16Q11_TABLE_BITS controls the size of the lookup table
// for SC16Q11 data. The size of the table is 2 * (1 << (2*BITS))
// bytes. Reducing the number of bits reduces precision but
// can run substantially faster by staying in cache.
// See convert_benchmark.c for some numbers.

// Leaving SC16QQ_TABLE_BITS undefined will disable the table lookup and always use
// the floating-point path, which may be faster on some systems

#if defined(SC16Q11_TABLE_BITS)

#define USE_BITS SC16Q11_TABLE_BITS
#define LOSE_BITS (11 - SC16Q11_TABLE_BITS)

static uint16_t *sc16q11_lookup;

static bool init_sc16q11_lookup() {
    if (sc16q11_lookup)
        return true;

    sc16q11_lookup = malloc(sizeof (uint16_t) * (1 << (USE_BITS * 2)));
    if (!sc16q11_lookup) {
        fprintf(stderr, "can't allocate SC16Q11 conversion lookup table\n");
        return false;
    }

    for (int i = 0; i < 2048; i += (1 << LOSE_BITS)) {
        for (int q = 0; q < 2048; q += (1 << LOSE_BITS)) {
            float fI = i / 2048.0, fQ = q / 2048.0;
            float magsq = fI * fI + fQ * fQ;
            if (magsq > 1)
                magsq = 1;
            float mag = sqrtf(magsq);

            unsigned index = ((i >> LOSE_BITS) << USE_BITS) | (q >> LOSE_BITS);
            sc16q11_lookup[index] = (uint16_t) (mag * 65535.0f + 0.5f);
        }
    }

    return true;
}

static void convert_sc16q11_table(void *iq_data,
        uint16_t *mag_data,
        unsigned nsamples,
        struct converter_state *state,
        double *out_mean_level,
        double *out_mean_power) {
    uint16_t *in = iq_data;
    unsigned i;
    uint16_t I, Q;
    uint64_t sum_level = 0;
    uint64_t sum_power = 0;
    uint16_t mag;

    MODES_NOTUSED(state);

    for (i = 0; i < nsamples; ++i) {
        I = abs((int16_t) le16toh(*in++)) & 2047;
        Q = abs((int16_t) le16toh(*in++)) & 2047;
        mag = sc16q11_lookup[((I >> LOSE_BITS) << USE_BITS) | (Q >> LOSE_BITS)];
        *mag_data++ = mag;
        sum_level += mag;
        sum_power += (uint32_t) mag * (uint32_t) mag;
    }

    if (out_mean_level) {
        *out_mean_level = sum_level / 65536.0 / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / 65535.0 / 65535.0 / nsamples;
    }
}

#else /* ! defined(SC16Q11_TABLE_BITS) */

static void convert_sc16q11_nodc(void *iq_data,
        uint16_t *mag_data,
        unsigned nsamples,
        struct converter_state *state,
        double *out_mean_level,
        double *out_mean_power) {
    MODES_NOTUSED(state);

    uint16_t *in = iq_data;

    unsigned i;
    int16_t I, Q;
    float fI, fQ, magsq;
    float sum_level = 0, sum_power = 0;

    for (i = 0; i < nsamples; ++i) {
        I = (int16_t) le16toh(*in++);
        Q = (int16_t) le16toh(*in++);
        fI = I / 2048.0f;
        fQ = Q / 2048.0f;

        magsq = fI * fI + fQ * fQ;
        if (magsq > 1)
            magsq = 1;

        float mag = sqrtf(magsq);
        sum_power += magsq;
        sum_level += mag;
        *mag_data++ = (uint16_t) (mag * 65535.0f + 0.5f);
    }

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}

#endif /* defined(SC16Q11_TABLE_BITS) */

static void convert_sc16q11_generic(void *iq_data,
        uint16_t *mag_data,
        unsigned nsamples,
        struct converter_state *state,
        double *out_mean_level,
        double *out_mean_power) {
    uint16_t *in = iq_data;
    float z1_I = state->z1_I;
    float z1_Q = state->z1_Q;
    const float dc_a = state->dc_a;
    const float dc_b = state->dc_b;

    unsigned i;
    int16_t I, Q;
    float fI, fQ, magsq;
    float sum_level = 0, sum_power = 0;

    for (i = 0; i < nsamples; ++i) {
        I = (int16_t) le16toh(*in++);
        Q = (int16_t) le16toh(*in++);
        fI = I / 2048.0f;
        fQ = Q / 2048.0f;

        // DC block
        z1_I = fI * dc_a + z1_I * dc_b;
        z1_Q = fQ * dc_a + z1_Q * dc_b;
        fI -= z1_I;
        fQ -= z1_Q;

        magsq = fI * fI + fQ * fQ;
        if (magsq > 1)
            magsq = 1;

        float mag = sqrtf(magsq);
        sum_power += magsq;
        sum_level += mag;
        *mag_data++ = (uint16_t) (mag * 65535.0f + 0.5f);
    }

    state->z1_I = z1_I;
    state->z1_Q = z1_Q;

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}
#endif

static struct {
    input_format_t format;
    int can_filter_dc;
    iq_convert_fn fn;
    const char *description;
    bool(*init)();
} converters_table[] = {
    // In order of preference
    { INPUT_UC8, 1, convert_uc8_generic, "UC8, float path", NULL},
    { INPUT_UC8, 0, convert_uc8_nodc, "UC8, no dc filter float path", NULL},
#if !UC8_ONLY
    { INPUT_SC16, 0, convert_sc16_nodc, "SC16, float path, no DC", NULL},
    { INPUT_SC16, 1, convert_sc16_generic, "SC16, float path", NULL},
#if defined(SC16Q11_TABLE_BITS)
    { INPUT_SC16Q11, 0, convert_sc16q11_table, "SC16Q11, integer/table path", init_sc16q11_lookup},
#else
    { INPUT_SC16Q11, 0, convert_sc16q11_nodc, "SC16Q11, float path, no DC", NULL},
#endif
    { INPUT_SC16Q11, 1, convert_sc16q11_generic, "SC16Q11, float path", NULL},
#endif /* UC8_ONLY */
    { 0, 0, NULL, NULL, NULL}
};

iq_convert_fn init_converter(input_format_t format,
        double sample_rate,
        int filter_dc,
        struct converter_state **out_state) {
    int i;

    for (i = 0; converters_table[i].fn; ++i) {
        if (converters_table[i].format != format)
            continue;
        if (filter_dc && !converters_table[i].can_filter_dc)
            continue;
        break;
    }

    if (!converters_table[i].fn) {
        fprintf(stderr, "no suitable converter for format=%d dc=%d\n",
                format, filter_dc);
        return NULL;
    }

    if (converters_table[i].init) {
        if (!converters_table[i].init())
            return NULL;
    }

    *out_state = malloc(sizeof (struct converter_state));
    if (! *out_state) {
        fprintf(stderr, "can't allocate converter state\n");
        return NULL;
    }

    (*out_state)->z1_I = 0;
    (*out_state)->z1_Q = 0;

    if (filter_dc) {
        // init DC block @ 1Hz
        (*out_state)->dc_b = exp(-2.0 * M_PI * 1.0 / sample_rate);
        (*out_state)->dc_a = 1.0 - (*out_state)->dc_b;
    } else {
        // if the converter does filtering, make sure it has no effect
        (*out_state)->dc_b = 1.0;
        (*out_state)->dc_a = 0.0;
    }

    return converters_table[i].fn;
}

void cleanup_converter(struct converter_state *state) {
    free(state);
#if !UC8_ONLY
#if defined(SC16Q11_TABLE_BITS)
    free(sc16q11_lookup);
#endif
#endif
}
