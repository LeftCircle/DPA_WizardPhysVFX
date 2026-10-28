//*******************************************************************
//
//   Color.C
//
// 4D color class in the namespace lux
//
//
//
//*******************************************************************

#include "Color.h"

using namespace lux;


Color lux::exp(const Color& c )
{ 
   return Color( std::exp(c[0]), std::exp(c[1]), std::exp(c[2]), std::exp(c[3]) ); 
}

Color lux::log(const Color& c )
{ 
   return Color( std::log(c[0]), std::log(c[1]), std::log(c[2]), std::log(c[3]) ); 
}

Color lux::pow(const Color& c, const double g )
{ 
   return Color( std::pow(c[0],g), std::pow(c[1],g), std::pow(c[2],g), std::pow(c[3],g) ); 
}

Color lux::operator+(const double& v, const Color& c )
{
   return Color( v+c[0], v+c[1], v+c[2], v+c[3] );
}

Color lux::operator-(const double& v, const Color& c )
{
   return Color( v-c[0], v-c[1], v-c[2], v-c[3] );
}

Color lux::operator*(const double& v, const Color& c )
{
   return Color( v*c[0], v*c[1], v*c[2], v*c[3] );
}

Color lux::operator/(const double& v, const Color& c )
{
   return Color( v/c[0], v/c[1], v/c[2], v/c[3] );
}


Color lux::operator-(const Color v)
{
   return v*(-1.0);
}


