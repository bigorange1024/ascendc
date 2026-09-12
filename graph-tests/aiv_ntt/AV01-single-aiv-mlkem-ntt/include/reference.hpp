#pragma once
#include <array>
#include <cstdint>
namespace single_ntt {
using Polynomial = std::array<int32_t, 256>;
inline int32_t Canonical(int64_t x, int32_t q) {
    x %= q;
    return static_cast<int32_t>(x < 0 ? x + q : x);
}
inline int32_t Power(int32_t a, int exponent, int q) {
    int64_t result = 1;
    while (exponent) {
        if (exponent & 1)
            result = result * a % q;
        a = int64_t(a) * a % q;
        exponent >>= 1;
    }
    return static_cast<int32_t>(result);
}
inline unsigned Reverse(unsigned v, int bits) {
    unsigned result = 0;
    for (int i = 0; i < bits; ++i) {
        result = (result << 1) | (v & 1);
        v >>= 1;
    }
    return result;
}
// Independent direct evaluation, not a copy of the device butterfly algorithm.
// ML-KEM leaves even/odd pairs in 128 quadratic factors; it has seven layers.
inline Polynomial Oracle(const Polynomial &input, bool kem) {
    const int q = kem ? 3329 : 8380417;
    Polynomial output{};
    const int terms = kem ? 128 : 256;
    for (int i = 0; i < terms; ++i) {
        const int root = Power(kem ? 17 : 1753, 2 * Reverse(i, kem ? 7 : 8) + 1, q);
        int64_t power = 1, sum0 = 0, sum1 = 0;
        for (int j = 0; j < terms; ++j) {
            sum0 = (sum0 + power * input[kem ? 2 * j : j]) % q;
            if (kem)
                sum1 = (sum1 + power * input[2 * j + 1]) % q;
            power = power * root % q;
        }
        output[kem ? 2 * i : i] = Canonical(sum0, q);
        if (kem)
            output[2 * i + 1] = Canonical(sum1, q);
    }
    return output;
}
} // namespace single_ntt
