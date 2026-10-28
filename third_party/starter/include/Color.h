//*******************************************************************
//
//   Color.h
//
// 4D color class in the namespace lux
//
//
//
//*******************************************************************

#ifndef __LUX_COLOR_H__
#define __LUX_COLOR_H__

#include <cstdio>
#include <cmath>

namespace lux
{

class Color
{
  public:

   Color(){ xyzw[0] = xyzw[1] = xyzw[2] = xyzw[3] = 0; }

   Color(const Color& v)
   { 
      xyzw[0] = v.xyzw[0];
      xyzw[1] = v.xyzw[1];
      xyzw[2] = v.xyzw[2]; 
      xyzw[3] = v.xyzw[3]; 
   }
   
   Color(const double a, const double b, const double c, const double d)
   {
      xyzw[0] = a;
      xyzw[1] = b;
      xyzw[2] = c; 
      xyzw[3] = d; 
   }

   ~Color(){}

   //!  Set all four components
   void set( const double vx, const double vy, const double vz, const double vw )
   {
      xyzw[0] = vx;
      xyzw[1] = vy;
      xyzw[2] = vz;
      xyzw[3] = vw;
   }

   //! Add two colors together
   const Color operator+        (const Color& v) const 
   { 
      return Color(xyzw[0]+v.xyzw[0], xyzw[1]+v.xyzw[1], xyzw[2]+v.xyzw[2], xyzw[3]+v.xyzw[3]); 
   }
  
   //! Subtract one color from another
   const Color operator-        (const Color& v) const
   { 
      return Color(xyzw[0]-v.xyzw[0], xyzw[1]-v.xyzw[1], xyzw[2]-v.xyzw[2], xyzw[3]-v.xyzw[3]); 
   }

   Color operator/        (const Color& v) const
   { return Color(xyzw[0]/v.xyzw[0], xyzw[1]/v.xyzw[1], xyzw[2]/v.xyzw[2], xyzw[3]/v.xyzw[3]); }

   //! component product
   Color operator*        (const Color& v) const  
   { return Color(xyzw[0]*v.xyzw[0] , xyzw[1]*v.xyzw[1] , xyzw[2]*v.xyzw[2] , xyzw[3]*v.xyzw[3]); }
  
   Color& operator=       (const Color& v)
   { xyzw[0] = v.xyzw[0]; xyzw[1] = v.xyzw[1]; xyzw[2] = v.xyzw[2]; xyzw[3]=v.xyzw[3]; return *this; }
  
   Color& operator+=      (const Color& v)
   { xyzw[0] += v.xyzw[0]; xyzw[1] += v.xyzw[1]; xyzw[2] += v.xyzw[2]; xyzw[3] += v.xyzw[3]; return *this; }
  
   Color& operator-=      (const Color& v)
   { xyzw[0] -= v.xyzw[0]; xyzw[1] -= v.xyzw[1]; xyzw[2] -= v.xyzw[2]; xyzw[3] -= v.xyzw[3]; return *this; }
  
   Color& operator/=      (const Color& v)
   { xyzw[0] /= v.xyzw[0]; xyzw[1] /= v.xyzw[1]; xyzw[2] /= v.xyzw[2]; xyzw[3] /= v.xyzw[3]; return *this; }
  
   Color& operator*=      (const Color& v)
   { xyzw[0] *= v.xyzw[0]; xyzw[1] *= v.xyzw[1]; xyzw[2] *= v.xyzw[2]; xyzw[3] *= v.xyzw[3]; return *this; }
  
   //! Add number to all colors
   const Color operator+        (const double& v) const 
   { 
      return Color(xyzw[0]+v, xyzw[1]+v, xyzw[2]+v, xyzw[3]+v); 
   }
  
   //! Subtract number to all colors
   const Color operator-        (const double& v) const 
   { 
      return Color(xyzw[0]-v, xyzw[1]-v, xyzw[2]-v, xyzw[3]-v); 
   }
  
   //! multiply number to all colors
   const Color operator*        (const double& v) const 
   { 
      return Color(xyzw[0]*v, xyzw[1]*v, xyzw[2]*v, xyzw[3]*v); 
   }
  
   //! divide number to all colors
   const Color operator/        (const double& v) const 
   { 
      return Color(xyzw[0]/v, xyzw[1]/v, xyzw[2]/v, xyzw[3]/v); 
   }

   Color& operator+=(const double& v)
   {
      xyzw[0] += v;
      xyzw[1] += v;
      xyzw[2] += v;
      xyzw[3] += v;
      return *this;
   }
 
   Color& operator-=(const double& v)
   {
      xyzw[0] -= v;
      xyzw[1] -= v;
      xyzw[2] -= v;
      xyzw[3] -= v;
      return *this;
   }
 
   Color& operator*=(const double& v)
   {
      xyzw[0] *= v;
      xyzw[1] *= v;
      xyzw[2] *= v;
      xyzw[3] *= v;
      return *this;
   }
 
   Color& operator/=(const double& v)
   {
      xyzw[0] /= v;
      xyzw[1] /= v;
      xyzw[2] /= v;
      xyzw[3] /= v;
      return *this;
   }
 
   const double& operator[] (const int v) const { return xyzw[v]; }
         double& operator[] (const int v)       { return xyzw[v]; }
   const double& operator() (const int v) const { return xyzw[v]; }

   const double X() const { return xyzw[0]; }
   const double Y() const { return xyzw[1]; }
   const double Z() const { return xyzw[2]; }
   const double W() const { return xyzw[3]; }

   const double red() const { return xyzw[0]; }
   const double green() const { return xyzw[1]; }
   const double blue() const { return xyzw[2]; }
   const double alpha() const { return xyzw[3]; }

//  Comparisons

   const bool operator==         (const Color& v) const
       { return ( xyzw[0]==v.xyzw[0] && xyzw[1]==v.xyzw[1] && xyzw[2]==v.xyzw[2] && xyzw[3] == v.xyzw[3] ); }
  
   const bool operator!=         (const Color& v) const
       { return ( xyzw[0]!=v.xyzw[0] || xyzw[1]!=v.xyzw[1] || xyzw[2]!=v.xyzw[2] || xyzw[3] != v.xyzw[3] ); }
 
   char *__str__() {
       static char tmp[1024];
       std::sprintf(tmp,"Color(%g,%g,%g,%g)", xyzw[0],xyzw[1],xyzw[2],xyzw[3]);
       return tmp;
   }


  private:
  double xyzw[4];
};


Color exp(const Color& c );
Color log(const Color& c );
Color pow(const Color& c, const double g );

Color operator+(const double& v, const Color& c );
Color operator-(const double& v, const Color& c );
Color operator*(const double& v, const Color& c );
Color operator/(const double& v, const Color& c );
Color operator-(const Color v);





}



#endif
