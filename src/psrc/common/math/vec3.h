#ifndef PSRC_COMMON_MATH_VEC3_H
#define PSRC_COMMON_MATH_VEC3_H

#include "util.h"

#include <math.h>

static inline void vec3_calctrig(const float rot[3], float sinout[3], float cosout[3]) {
    float rad[3] = {-DEGTORAD_FLT(rot[0]), DEGTORAD_FLT(rot[1]), -DEGTORAD_FLT(rot[2])};
    sinout[0] = sinf(rad[0]);
    sinout[1] = sinf(rad[1]);
    sinout[2] = sinf(rad[2]);
    cosout[0] = cosf(rad[0]);
    cosout[1] = cosf(rad[1]);
    cosout[2] = cosf(rad[2]);
}

static inline void vec3_trigrotate(const float in[3], const float sinin[3], const float cosin[3], float out[3]) {
    float tmp[3];
    float mul[3][3];
    mul[0][0] = sinin[0] * sinin[1] * sinin[2] + cosin[1] * cosin[2];
    mul[0][1] = cosin[0] * -sinin[2];
    mul[0][2] = sinin[0] * cosin[1] * sinin[2] + sinin[1] * cosin[2];
    mul[1][0] = sinin[0] * sinin[1] * cosin[2] + cosin[1] * sinin[2];
    mul[1][1] = cosin[0] * cosin[2];
    mul[1][2] = -sinin[0] * cosin[1] * cosin[2] + sinin[1] * sinin[2];
    mul[2][0] = cosin[0] * -sinin[1];
    mul[2][1] = sinin[0];
    mul[2][2] = cosin[0] * cosin[1];
    tmp[0] = in[0] * mul[0][0] + in[1] * mul[0][1] + in[2] * mul[0][2];
    tmp[1] = in[0] * mul[1][0] + in[1] * mul[1][1] + in[2] * mul[1][2];
    tmp[2] = in[0] * mul[2][0] + in[1] * mul[2][1] + in[2] * mul[2][2];
    out[0] = tmp[0];
    out[1] = tmp[1];
    out[2] = tmp[2];
}

static inline void vec3_rotate(const float in[3], const float rot[3], float out[3]) {
    float tmpsin[3], tmpcos[3];
    vec3_calctrig(rot, tmpsin, tmpcos);
    vec3_trigrotate(in, tmpsin, tmpcos, out);
}

#endif
