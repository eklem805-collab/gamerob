#ifndef CD_FREESTANDING_MATH_H
#define CD_FREESTANDING_MATH_H
// Declarations only: the implementations live in libm.so on the device.
double sin(double), cos(double), tan(double);
double asin(double), acos(double), atan(double), atan2(double, double);
double sqrt(double), pow(double, double), fabs(double), fmod(double, double);
float sinf(float), cosf(float), tanf(float);
float asinf(float), acosf(float), atanf(float), atan2f(float, float);
float sqrtf(float), powf(float, float), fabsf(float), fmodf(float, float);
float expf(float), logf(float), exp2f(float), floorf(float), ceilf(float), roundf(float);
float fminf(float, float), fmaxf(float, float);
double exp(double), log(double), floor(double), ceil(double), round(double);
double fmin(double, double), fmax(double, double);
#define M_PI 3.14159265358979323846
#define HUGE_VALF (1.0f/0.0f)
#define INFINITY (1.0f/0.0f)
#define NAN (0.0f/0.0f)
#define INFINITY_D (1.0/0.0)
static inline int isnan(float x) { return x != x; }
static inline int isinf(float x) { return x > 3.4e38f || x < -3.4e38f; }
#endif
