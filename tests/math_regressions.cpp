#include <cmath>
#include <cassert>
#include <limits>
#include "../library/math.h"
int main() {
    const float cases[] = {0, 1, 2, 4, 1e-30f, std::numeric_limits<float>::denorm_min(), FLT_MIN, FLT_MAX};
    for (float value : cases) {
        double expected = std::sqrt(double(value));
        double actual = math::qsqrt(value);
        assert(value ? std::abs(actual - expected) <= expected * 1e-6 : actual == 0);
    }
    assert(std::isnan(math::qsqrt(-1)));
    assert(math::qfloor(-2) == -2 && math::qfloor(-2.5f) == -3);
    assert(math::qfloor(2.5f) == 2 && std::isinf(math::qfloor(INFINITY)));
    assert(math::qatan2(0, 0) == 0);
    assert(math::qpow(2, -1) == 0.5);
#ifndef _KERNEL_MODE
    assert(math::qpow(4, 0.5) == 2);
#else
    assert(std::isnan(math::qpow(4, 0.5)));
#endif
    assert(math::qacos(1.001f) == 0);
    assert(std::abs(math::qsin(10000) - std::sin(10000.0)) < 1e-8);
    assert(std::abs(math::qcos(-10000) - std::cos(-10000.0)) < 1e-8);
    assert(std::isnan(math::qfact(171)));
    vec3 zero = math::vec_delta({1, 1, 1}, {1, 1, 1});
    assert(zero.x == 0 && zero.y == 0 && zero.z == 0);
    view_matrix_t matrix{}; vec3 out{};
    matrix.matrix[3][3] = NAN;
    assert(!math::world_to_screen({1920, 1080}, {0, 0, 0}, out, matrix));
}
