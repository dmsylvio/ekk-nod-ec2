#ifndef EKK_LIBRARY_MATH_H
#define EKK_LIBRARY_MATH_H

#include <stdint.h>
#include <string.h>
#include <float.h>
#ifndef _KERNEL_MODE
#include <cmath>
#endif

typedef float matrix3x4_t[3][4];

typedef struct {
	float x, y;
} vec2 ;

typedef struct {
	float x, y, z;
} vec3 ;

typedef struct {
	long x, y;
} vec2_i ;

struct view_matrix_t {
	float* operator[ ]( int index ) {
		return matrix[ index ];
	}

	float matrix[ 4 ][ 4 ];
};

#define X_PI 3.14159f 
#define X_DIV 57.29577f
#define qabs(x) ((x) < 0 ? -(x) : (x))
#define qmax(a, b) (((a) > (b)) ? (a) : (b))
#define qmin(a, b) (((a) < (b)) ? (a) : (b))

namespace math
{
	inline bool finite(float x) { return x == x && x <= FLT_MAX && x >= -FLT_MAX; }
	inline float qsqrt(float x)
	{
		static_assert(sizeof(float) == sizeof(uint32_t) && FLT_RADIX == 2 && FLT_MANT_DIG == 24,
		              "qsqrt requires binary32 floats");
		if (x == 0 || x > FLT_MAX) return x;
		if (x < 0 || x != x) { uint32_t bits = 0x7FC00000; float nan; memcpy(&nan, &bits, 4); return nan; }
		bool subnormal = x < FLT_MIN;
		float scaled = subnormal ? x * 16777216.0f : x;
		uint32_t bits = 0; memcpy(&bits, &scaled, 4);
		bits = (bits >> 1) + 0x1FC00000U;
		float seed = 0; memcpy(&seed, &bits, 4);
		double guess = seed;
		for (int i = 0; i < 6; ++i) guess = 0.5 * (guess + double(scaled) / guess);
		return (float)(subnormal ? guess * 0.000244140625 : guess);
	}


	inline float qclamp(float x, float min, float max)
	{
		if (x < min) x = min;
		if (x > max) x = max;
		return x;
	}

	inline float qatan2(float y, float x)
	{
		if (x == 0 && y == 0) return 0;
		float t0, t1, t3, t4;
		t3 = qabs(x);
		t1 = qabs(y);
		t0 = qmax(t3, t1);
		t1 = qmin(t3, t1);
		t3 = 1 / t0;
		t3 = t1 * t3;

		t4 = t3 * t3;
		t0 = -0.0134804f;
		t0 = t0 * t4 + 0.05747f;
		t0 = t0 * t4 - 0.12123f;
		t0 = t0 * t4 + 0.19563f;
		t0 = t0 * t4 - 0.33299f;
		t0 = t0 * t4 + 0.99999f;
		t3 = t0 * t3;

		t3 = (qabs(y) > qabs(x)) ? 1.57079f - t3 : t3;
		t3 = (x < 0) ? X_PI - t3 : t3;
		t3 = (y < 0) ? -t3 : t3;

		return t3;
	}

	inline float qatan(float x)
	{
		return qatan2(x, 1);
	}

	inline float qacos(float x)
	{
		x = qclamp(x, -1.0f, 1.0f);
		int negate = (x < 0);
		x = qabs(x);
		float ret = -0.01872f;
		ret = ret * x;
		ret = ret + 0.07426f;
		ret = ret * x;
		ret = ret - 0.21211f;
		ret = ret * x;
		ret = ret + 1.57072f;
		ret = ret * qsqrt(1.0f - x);
		ret = ret - 2 * negate * ret;
		return negate * X_PI + ret;
	}


	#define RAD2DEG(x) ((float)(x) * (float)(180.f / 3.14159265358979323846f))
	#define DEG2RAD(x) ((float)(x) * (float)(3.14159265358979323846f / 180.f))
	inline double qpow(double a, double b)
	{
#ifndef _KERNEL_MODE
		return std::pow(a, b);
#else
		// Freestanding kernel builds support bounded integral exponents only.
		if (b != b || b < -1024 || b > 1024 || b != (int)b) return qsqrt(-1);
		int exponent = (int)b; bool inverse = exponent < 0;
		if (inverse) exponent = -exponent;
		double result = 1;
		while (exponent) { if (exponent & 1) result *= a; a *= a; exponent >>= 1; }
		return inverse ? (result == 0 ? qsqrt(-1) : 1 / result) : result;
#endif
	}

	inline double qfact(double x)
	{
		if (x != x || x < 0 || x > 170 || x != (int)x) return qsqrt(-1);
		double ret = 1;
		for (int i = 1; i <= x; i++)
			ret *= i;
		return ret;
	}

	inline double qsin(double x)
	{
#ifndef _KERNEL_MODE
		return std::sin(x);
#else
		if (x != x || x < -1e12 || x > 1e12) return qsqrt(-1);
		const double period = 6.2831853071795864769;
		x -= (long long)(x / period) * period;
		if (x > period / 2) x -= period;
		if (x < -period / 2) x += period;
		double y = x;
		double s = -1;
		for (int i = 3; i <= 100; i += 2) {
			y += s * (qpow(x, (float)i) / qfact(i));
			s *= -1;
		}
		return y;
#endif
	}

	inline double qcos(double x)
	{
#ifndef _KERNEL_MODE
		return std::cos(x);
#else
		if (x != x || x < -1e12 || x > 1e12) return qsqrt(-1);
		const double period = 6.2831853071795864769;
		x -= (long long)(x / period) * period;
		if (x > period / 2) x -= period;
		if (x < -period / 2) x += period;
		double y = 1;
		double s = -1;
		for (int i = 2; i <= 100; i += 2) {
			y += s * (qpow(x, (float)i) / qfact(i));
			s *= -1;
		}
		return y;
#endif
	}

	inline double qtan(double x)
	{
		return (qsin(x) / qcos(x));
	}

	inline float qfloor(float x)
	{
		if (!finite(x) || x >= 2147483648.0f || x <= -2147483648.0f) return x;
		int truncated = (int)x;
		return (float)(truncated - (x < (float)truncated ? 1 : 0));
	}

	inline float qfmodf(float a, float b)
	{
		if (!finite(a) || !finite(b) || b == 0) return 0;
		return (a - b * qfloor(a / b));
	}

	inline void sincos(float radians, float* sine, float* cosine)
	{
		*sine = (float)qsin(radians);
		*cosine = (float)qcos(radians);
	}

	inline void angle_vec(vec3 angles, vec3* forward)
	{
		float sp, sy, cp, cy;
		sincos(DEG2RAD(angles.x), &sp, &cp);
		sincos(DEG2RAD(angles.y), &sy, &cy);
		forward->x = cp * cy;
		forward->y = cp * sy;
		forward->z = -sp;
	}

	inline float vec_dot(vec3 v0, vec3 v1)
	{
		return (v0.x * v1.x + v0.y * v1.y + v0.z * v1.z);
	}

	inline float vec_length(vec3 v)
	{
		return (v.x * v.x + v.y * v.y + v.z * v.z);
	}

	inline void vec_clamp(vec3* v)
	{
		if (v->x > 89.0f && v->x <= 180.0f) {
			v->x = 89.0f;
		}
		if (v->x > 180.0f) {
			v->x = v->x - 360.0f;
		}
		if (v->x < -89.0f) {
			v->x = -89.0f;
		}
		v->y = qfmodf(v->y + 180, 360) - 180;
		v->z = 0;
	}

	inline void vec_angles(vec3 forward, vec3* angles)
	{
		float tmp, yaw, pitch;

		if (forward.y == 0.f && forward.x == 0.f) {
			yaw = 0;
			if (forward.z > 0) {
				pitch = 270;
			}
			else {
				pitch = 90.f;
			}
		}
		else {
			yaw = (float)(qatan2(forward.y, forward.x) * 180.f / 3.14159265358979323846f);
			if (yaw < 0) {
				yaw += 360.f;
			}
			tmp = (float)qsqrt(forward.x * forward.x + forward.y * forward.y);
			pitch = (float)(qatan2(-forward.z, tmp) * 180.f / 3.14159265358979323846f);
			if (pitch < 0) {
				pitch += 360.f;
			}
		}
		angles->x = pitch;
		angles->y = yaw;
		angles->z = 0.f;
	}

	inline void vec_normalize(vec3* vec)
	{
		float radius;
		radius = 1.f / (float)(qsqrt(vec->x * vec->x + vec->y * vec->y + vec->z * vec->z) + 1.192092896e-07f);
		vec->x *= radius, vec->y *= radius, vec->z *= radius;
	}

	inline float vec_length_sqrt(vec3 p0)
	{
		return (float)qsqrt(p0.x * p0.x + p0.y * p0.y + p0.z * p0.z);
	}

	inline vec3 vec_sub(vec3 p0, vec3 p1)
	{
		vec3 r;
		r.x = p0.x - p1.x;
		r.y = p0.y - p1.y;
		r.z = p0.z - p1.z;
		return r;
	}

	inline vec3 vec_add(vec3 p0, vec3 p1)
	{
		vec3 r;
		r.x = p0.x + p1.x;
		r.y = p0.y + p1.y;
		r.z = p0.z + p1.z;
		return r;
	}

	inline float vec_distance(vec3 p0, vec3 p1)
	{
		return vec_length_sqrt(vec_sub(p0, p1));
	}

	inline float get_fov_distance(vec3 vangle, vec3 angle, float distance)
	{
		vec3 a0, a1;
		angle_vec(vangle, &a0);
		angle_vec(angle, &a1);
		float norm = qsqrt(vec_length(a0)) * qsqrt(vec_length(a1));
		if (!finite(norm) || norm <= 0) return 0;
		return RAD2DEG(qacos(vec_dot(a0, a1) / norm) * distance);
	}

	inline float get_fov(vec2 scrangles, vec3 aimangles)
	{
		vec3 delta;

		delta.x = aimangles.x - scrangles.x;
		delta.y = aimangles.y - scrangles.y;

		if (delta.x > 180)
			delta.x = 360 - delta.x;
		if (delta.x < 0)
			delta.x = -delta.x;

		delta.y = qfmodf(delta.y + 180, 360) - 180;
		if (delta.y < 0)
			delta.y = -delta.y;

		return qsqrt( (float)(qpow(delta.x, 2.0) + qpow(delta.y, 2.0)) );
	}

	inline vec3 vec_delta(vec3 p0, vec3 p1)
	{
		vec3  d;
		float l;
		d = vec_sub(p0, p1);
		l = (float)vec_length_sqrt(d);
		if (!finite(l) || l <= 0) return vec3{};
		d.x /= l; d.y /= l; d.z /= l;
		return d;
	}


	inline vec3 vec_transform(vec3 p0, matrix3x4_t p1)
	{
		vec3 v;

		v.x = (p0.x * p1[0][0] + p0.y * p1[0][1] + p0.z * p1[0][2]) + p1[0][3];
		v.y = (p0.x * p1[1][0] + p0.y * p1[1][1] + p0.z * p1[1][2]) + p1[1][3];
		v.z = (p0.x * p1[2][0] + p0.y * p1[2][1] + p0.z * p1[2][2]) + p1[2][3];
		return v;
	}

	inline vec3 vec_atd(vec3 vangle)
	{
		double y[2], p[2];
		vangle.x *= (3.14159265358979323846f / 180.f);
		vangle.y *= (3.14159265358979323846f / 180.f);
		y[0]     = qsin(vangle.y), y[1] = qcos(vangle.y);
		p[0]     = qsin(vangle.x), p[1] = qcos(vangle.x);
		vangle.x = (float)(p[1] * y[1]);
		vangle.y = (float)(p[1] * y[0]);
		vangle.z = (float)-p[0];
		return vangle;
	}

	inline bool vec_min_max(vec3 eye, vec3 dir, vec3 min, vec3 max, float radius)
	{
	    vec3     delta;
	    int      i;
	    vec3     q;
	    float    v;


	    //
	    // original maths by superdoc1234
	    //
	    delta = vec_delta(max, min);
	    for ( i = 0; i < vec_distance(min, max); i++ ) {
		q.x = min.x + delta.x * (float)i - eye.x;
		q.y = min.y + delta.y * (float)i - eye.y;
		q.z = min.z + delta.z * (float)i - eye.z;
		if ((v = vec_dot(q, dir)) < 1.0f) {
		    return 0;
		}
		v = radius * radius - (vec_length(q) - v * v);

		if ( v <= -100.f ) {
		    return 0;
		}
		if (v >= 1.19209290E-07F) {
		    return 1;
		}
	    }
	    return 0;
	}

	inline vec3 CalcAngle(vec3 src, vec3 dst)
	{
		vec3 angle;

		vec3 delta = vec_sub(src, dst);

		float hyp = qsqrt(delta.x * delta.x + delta.y * delta.y);

		angle.x = qatan(delta.z / hyp) * (float)(180.0 / X_PI);
		angle.y = qatan(delta.y / delta.x) * (float)(180.0 / X_PI);
		angle.z = 0;

		if (delta.x >= 0.0)
			angle.y += 180.0f;

		return angle;
	}

	inline bool world_to_screen( const vec2& screen_size, const vec3& pos, vec3& out, view_matrix_t matrix )
	{
		out.x = matrix[ 0 ][ 0 ] * pos.x + matrix[ 0 ][ 1 ] * pos.y + matrix[ 0 ][ 2 ] * pos.z + matrix[ 0 ][ 3 ];
		out.y = matrix[ 1 ][ 0 ] * pos.x + matrix[ 1 ][ 1 ] * pos.y + matrix[ 1 ][ 2 ] * pos.z + matrix[ 1 ][ 3 ];

		float w = matrix[ 3 ][ 0 ] * pos.x + matrix[ 3 ][ 1 ] * pos.y + matrix[ 3 ][ 2 ] * pos.z + matrix[ 3 ][ 3 ];

		if (!finite(w) || !finite(out.x) || !finite(out.y) ||
		    !finite(screen_size.x) || !finite(screen_size.y) ||
		    screen_size.x <= 0 || screen_size.y <= 0 || w < 0.01f)
			return false;

		float inv_w = 1.f / w;
		out.x *= inv_w;
		out.y *= inv_w;

		float x = screen_size.x * .5f;
		float y = screen_size.y * .5f;

		x += 0.5f * out.x * screen_size.x + 0.5f;
		y -= 0.5f * out.y * screen_size.y + 0.5f;

		if (!finite(x) || !finite(y)) return false;
		out.x = x;
		out.y = y;

		return true;
	}

}

#endif /* math.h */

