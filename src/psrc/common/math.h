#ifndef PSRC_COMMON_MATH_H
#define PSRC_COMMON_MATH_H

#include "math_defs.h"

#include <math.h>

#define VEC3_CALCTRIG(r, rsin, rcos) do {\
    float rad[3] = {DEGTORAD_FLT(r[0]), DEGTORAD_FLT(r[1]), DEGTORAD_FLT(r[2])};\
    rsin[0] = sinf(rad[0]);\
    rsin[1] = sinf(rad[1]);\
    rsin[2] = sinf(rad[2]);\
    rcos[0] = cosf(rad[0]);\
    rcos[1] = cosf(rad[1]);\
    rcos[2] = cosf(rad[2]);\
} while (0)
#define MAT3_CREATEROTMAT(rsin, rcos, m) do {\
    ACCMAT(m, 0, 0) = -(rsin[0] * rsin[1] * rsin[2]) + rcos[2] * rcos[1];\
    ACCMAT(m, 0, 1) = rcos[0] * rsin[2];\
    ACCMAT(m, 0, 2) = rsin[0] * rcos[1] * rsin[2] + rsin[1] * rcos[2];\
    ACCMAT(m, 1, 0) = -(rcos[2] * rsin[0] * rsin[1]) - rcos[1] * rsin[2];\
    ACCMAT(m, 1, 1) = rcos[0] * rcos[2];\
    ACCMAT(m, 1, 2) = rcos[2] * rsin[0] * rcos[1] - rsin[1] * rsin[2];\
    ACCMAT(m, 2, 0) = -(rcos[0] * rsin[1]);\
    ACCMAT(m, 2, 1) = -rsin[0];\
    ACCMAT(m, 2, 2) = rcos[0] * rcos[1];\
} while (0)
#define MAT3_MUL_VEC3(m, v, o) do {\
    o[0] = ACCMAT(m, 0, 0) * v[0] + ACCMAT(m, 0, 1) * v[1] + ACCMAT(m, 0, 2) * v[2];\
    o[1] = ACCMAT(m, 1, 0) * v[0] + ACCMAT(m, 1, 1) * v[1] + ACCMAT(m, 1, 2) * v[2];\
    o[2] = ACCMAT(m, 2, 0) * v[0] + ACCMAT(m, 2, 1) * v[1] + ACCMAT(m, 2, 2) * v[2];\
} while (0)
#define MAT4X3_CREATETRANSMAT(t, rsin, rcos, s, m) do {\
    ACCMAT(m, 0, 0) = (-(rsin[0] * rsin[1] * rsin[2]) + rcos[2] * rcos[1]) * s[0];\
    ACCMAT(m, 0, 1) = rcos[0] * rsin[2] * s[1];\
    ACCMAT(m, 0, 2) = (rsin[0] * rcos[1] * rsin[2] + rsin[1] * rcos[2]) * s[2];\
    ACCMAT(m, 0, 3) = t[0];\
    ACCMAT(m, 1, 0) = (-(rcos[2] * rsin[0] * rsin[1]) - rcos[1] * rsin[2]) * s[0];\
    ACCMAT(m, 1, 1) = rcos[0] * rcos[2] * s[1];\
    ACCMAT(m, 1, 2) = (rcos[2] * rsin[0] * rcos[1] - rsin[1] * rsin[2]) * s[2];\
    ACCMAT(m, 1, 3) = t[1];\
    ACCMAT(m, 2, 0) = -(rcos[0] * rsin[1] * s[0]);\
    ACCMAT(m, 2, 1) = -(rsin[0] * s[1]);\
    ACCMAT(m, 2, 2) = rcos[0] * rcos[1] * s[2];\
    ACCMAT(m, 2, 3) = t[2];\
} while (0)
#define MAT4X3_MUL(a, b, o) do {\
    ACCMAT(o, 0, 0) = ACCMAT(a, 0, 0) * ACCMAT(b, 0, 0) + ACCMAT(a, 0, 1) * ACCMAT(b, 1, 0) + ACCMAT(a, 0, 2) * ACCMAT(b, 2, 0);\
    ACCMAT(o, 0, 1) = ACCMAT(a, 0, 0) * ACCMAT(b, 0, 1) + ACCMAT(a, 0, 1) * ACCMAT(b, 1, 1) + ACCMAT(a, 0, 2) * ACCMAT(b, 2, 1);\
    ACCMAT(o, 0, 2) = ACCMAT(a, 0, 0) * ACCMAT(b, 0, 2) + ACCMAT(a, 0, 1) * ACCMAT(b, 1, 2) + ACCMAT(a, 0, 2) * ACCMAT(b, 2, 2);\
    ACCMAT(o, 0, 3) = ACCMAT(a, 0, 0) * ACCMAT(b, 0, 3) + ACCMAT(a, 0, 1) * ACCMAT(b, 1, 3) + ACCMAT(a, 0, 2) * ACCMAT(b, 2, 3) + ACCMAT(a, 0, 3);\
    ACCMAT(o, 1, 0) = ACCMAT(a, 1, 0) * ACCMAT(b, 0, 0) + ACCMAT(a, 1, 1) * ACCMAT(b, 1, 0) + ACCMAT(a, 1, 2) * ACCMAT(b, 2, 0);\
    ACCMAT(o, 1, 1) = ACCMAT(a, 1, 0) * ACCMAT(b, 0, 1) + ACCMAT(a, 1, 1) * ACCMAT(b, 1, 1) + ACCMAT(a, 1, 2) * ACCMAT(b, 2, 1);\
    ACCMAT(o, 1, 2) = ACCMAT(a, 1, 0) * ACCMAT(b, 0, 2) + ACCMAT(a, 1, 1) * ACCMAT(b, 1, 2) + ACCMAT(a, 1, 2) * ACCMAT(b, 2, 2);\
    ACCMAT(o, 1, 3) = ACCMAT(a, 1, 0) * ACCMAT(b, 0, 3) + ACCMAT(a, 1, 1) * ACCMAT(b, 1, 3) + ACCMAT(a, 1, 2) * ACCMAT(b, 2, 3) + ACCMAT(a, 1, 3);\
    ACCMAT(o, 2, 0) = ACCMAT(a, 2, 0) * ACCMAT(b, 0, 0) + ACCMAT(a, 2, 1) * ACCMAT(b, 1, 0) + ACCMAT(a, 2, 2) * ACCMAT(b, 2, 0);\
    ACCMAT(o, 2, 1) = ACCMAT(a, 2, 0) * ACCMAT(b, 0, 1) + ACCMAT(a, 2, 1) * ACCMAT(b, 1, 1) + ACCMAT(a, 2, 2) * ACCMAT(b, 2, 1);\
    ACCMAT(o, 2, 2) = ACCMAT(a, 2, 0) * ACCMAT(b, 0, 2) + ACCMAT(a, 2, 1) * ACCMAT(b, 1, 2) + ACCMAT(a, 2, 2) * ACCMAT(b, 2, 2);\
    ACCMAT(o, 2, 3) = ACCMAT(a, 2, 0) * ACCMAT(b, 0, 3) + ACCMAT(a, 2, 1) * ACCMAT(b, 1, 3) + ACCMAT(a, 2, 2) * ACCMAT(b, 2, 3) + ACCMAT(a, 2, 3);\
} while (0)
#define MAT4X3_MUL_VEC3(m, v, o) do {\
    o[0] = ACCMAT(m, 0, 0) * v[0] + ACCMAT(m, 0, 1) * v[1] + ACCMAT(m, 0, 2) * v[2] + ACCMAT(m, 0, 3);\
    o[1] = ACCMAT(m, 1, 0) * v[0] + ACCMAT(m, 1, 1) * v[1] + ACCMAT(m, 1, 2) * v[2] + ACCMAT(m, 1, 3);\
    o[2] = ACCMAT(m, 2, 0) * v[0] + ACCMAT(m, 2, 1) * v[1] + ACCMAT(m, 2, 2) * v[2] + ACCMAT(m, 2, 3);\
} while (0)
#define MAT4X3_COPY(in, out) do {\
    ACCMAT(out, 0, 0) = ACCMAT(in, 0, 0);\
    ACCMAT(out, 0, 1) = ACCMAT(in, 0, 1);\
    ACCMAT(out, 0, 2) = ACCMAT(in, 0, 2);\
    ACCMAT(out, 0, 3) = ACCMAT(in, 0, 3);\
    ACCMAT(out, 1, 0) = ACCMAT(in, 1, 0);\
    ACCMAT(out, 1, 1) = ACCMAT(in, 1, 1);\
    ACCMAT(out, 1, 2) = ACCMAT(in, 1, 2);\
    ACCMAT(out, 1, 3) = ACCMAT(in, 1, 3);\
    ACCMAT(out, 2, 0) = ACCMAT(in, 2, 0);\
    ACCMAT(out, 2, 1) = ACCMAT(in, 2, 1);\
    ACCMAT(out, 2, 2) = ACCMAT(in, 2, 2);\
    ACCMAT(out, 2, 3) = ACCMAT(in, 2, 3);\
} while (0)

#define ACCMAT(n, r, c) n[r][c]
static inline void vec3_calctrig(const float r[restrict 3], float rsin[restrict 3], float rcos[restrict 3]) {
    VEC3_CALCTRIG(r, rsin, rcos);
}
static inline void mat3_createrotmat(const float rsin[restrict 3], const float rcos[restrict 3], float m[restrict 3][3]) {
    MAT3_CREATEROTMAT(rsin, rcos, m);
}
static inline void mat3_mul_vec3(float m[3][3], const float in[3], float out[3]) {
    float tmp[3];
    MAT3_MUL_VEC3(m, in, tmp);
    out[0] = tmp[0];
    out[1] = tmp[1];
    out[2] = tmp[2];
}
static inline void mat4x3_createtransmat(const float t[restrict 3], const float rsin[restrict 3], const float rcos[restrict 3], const float s[restrict 3], float m[restrict 3][4]) {
    MAT4X3_CREATETRANSMAT(t, rsin, rcos, s, m);
}
static inline void mat4x3_mul(float a[restrict 3][4], float b[3][4], float out[3][4]) {
    float tmp[3][4];
    MAT4X3_MUL(a, b, tmp);
    MAT4X3_COPY(tmp, out);
}
static inline void mat4x3_mul_vec3(float m[restrict 3][4], const float in[3], float out[3]) {
    float tmp[3];
    MAT4X3_MUL_VEC3(m, in, tmp);
    out[0] = tmp[0];
    out[1] = tmp[1];
    out[2] = tmp[2];
}
#undef ACCMAT
#define ACCMAT(n, r, c) n[c][r]
static inline void mat4x3cm_createtransmat(const float t[restrict 3], const float rsin[restrict 3], const float rcos[restrict 3], const float s[restrict 3], float m[restrict 4][3]) {
    MAT4X3_CREATETRANSMAT(t, rsin, rcos, s, m);
}
static inline void mat4x3cm_mul(float a[restrict 4][3], float b[4][3], float out[4][3]) {
    float tmp[4][3];
    MAT4X3_MUL(a, b, tmp);
    MAT4X3_COPY(tmp, out);
}
static inline void mat4x3cm_mul_vec3(float m[restrict 4][3], const float in[3], float out[3]) {
    float tmp[3];
    MAT4X3_MUL_VEC3(m, in, tmp);
    out[0] = tmp[0];
    out[1] = tmp[1];
    out[2] = tmp[2];
}
#undef ACCMAT

static inline float fwrap(float n, float d) {
    float tmp = n - (int)(n / d) * d;
    if (tmp < 0.0f) tmp += d;
    return tmp;
}

#endif
