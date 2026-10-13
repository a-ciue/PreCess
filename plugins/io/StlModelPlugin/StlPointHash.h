/**
 * @file StlPointHash.h
 * @brief STL 精确坐标焊接的内部哈希实现
 */
#ifndef STL_POINT_HASH_H
#define STL_POINT_HASH_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace systems::io::detail {

//! @brief 精确坐标哈希：正负零等价，并将坐标高位变化扩散到桶索引低位
struct StlPointHash {
    size_t operator()(const std::array<double, 3>& point) const noexcept
    {
        uint64_t seed = 0x9e3779b97f4a7c15ULL;
        for (double value : point) {
            if (value == 0.0) {
                value = 0.0; // 正负零数值相等，必须得到相同哈希
            }
            uint64_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            seed ^= bits + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        }

        // float32 扩为 double 后低 29 位为零；仅移位组合会让大量点落入
        // MSVC 的同一个低位掩码桶，须在组合后做全位扩散以避免线性查找。
        seed ^= seed >> 30;
        seed *= 0xbf58476d1ce4e5b9ULL;
        seed ^= seed >> 27;
        seed *= 0x94d049bb133111ebULL;
        seed ^= seed >> 31;
        return static_cast<size_t>(seed);
    }
};

} // namespace systems::io::detail

#endif // STL_POINT_HASH_H
