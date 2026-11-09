#pragma once

namespace lux{


template <typename T>
T lerp(T a, T b, T t){
   return std::clamp(a + t * (b - a), a, b);
};

} // end namespace lux