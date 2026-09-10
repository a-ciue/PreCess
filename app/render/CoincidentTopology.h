#ifndef COINCIDENTTOPOLOGY_H
#define COINCIDENTTOPOLOGY_H

namespace highlight {
#ifdef __EMSCRIPTEN__
// GLES3/WebGL2 下深度分辨率低，-1/65000 的窗口深度裕量不足 1 LSB，所以高亮会被盖住
constexpr double POINT_UNITS = -6;
constexpr double LINE_UNITS = -6;
constexpr double POLYGON_UNITS = -6;
constexpr double SOLID_UNITS = -6;
#else
constexpr double POINT_UNITS = -6;
constexpr double LINE_UNITS = -4;
constexpr double POLYGON_UNITS = -1;
constexpr double SOLID_UNITS = -0.5;
#endif
}

#endif // COINCIDENTTOPOLOGY_H