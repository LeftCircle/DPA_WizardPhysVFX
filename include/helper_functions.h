#pragma once

namespace lux{


template <typename T, typename U>
T lerp(T a, T b, U t){
   return a + ((b - a) * t);
};


} // end namespace lux