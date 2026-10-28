
#include "Vector.h"
#include "Camera.h"
#include <iostream>
using namespace lux;

Camera::Camera()
{
   setEyeViewUp( Vector(0,0,1), Vector(0,0,-1), Vector(0,1,0) );
   setFov( 60.0 );
   setAspectRatio( 16.0/9.0 );
   setNearPlane(0.0);
   setFarPlane(1.0e6);
}

void Camera::setEyeViewUp( const Vector& eye, const Vector& view, const Vector& up )
{
   position = eye;
   axis_view = view.unitvector();
   axis_up = ( up - (up*axis_view) * axis_view ).unitvector();
   axis_right = (axis_view^axis_up).unitvector();
}

// view direction of a pixel at the fractional position x,y.
// Nominally 0 <= x <= 1 and 0 <= y <= 1 for the primary fov,
// but the values can extend beyond that
const Vector Camera::view( const double x, const double y ) const
{
   double xx = (2.0*x-1.0)*htanfov;
   double yy = (2.0*y-1.0)*vtanfov;
   return (axis_up * yy + axis_right * xx + axis_view).unitvector();
}

void Camera::XY( const Vector& P, double& x, double& y ) const
{
   Vector PP = P-position;
   PP /= axis_view*PP;
   PP -= axis_view;
   x = PP*axis_right;
   y = PP*axis_up;
}

void Camera::XYZ( const Vector& P, double& x, double& y, double& z ) const
{
   Vector PP = P-position;
   z = PP*axis_view;
   if( z < 0 )
   {
      x = y = z = 0;
      return;
   }
   x = (PP*axis_right)/z;
   y = (PP*axis_up)/z;
   x = (x/htanfov + 1.0)/2.0;
   y = (y/vtanfov + 1.0)/2.0;
   z = (PP.magnitude()-nearPlane())/(farPlane()-nearPlane());
}

void Camera::setFov( const double fov )
{
   FOV = fov;
   htanfov = tan( FOV*0.5*M_PI/180.0 );
   vtanfov = htanfov/aspect_ratio;
}

void Camera::setAspectRatio( const double ar )
{
   aspect_ratio = ar;
   vtanfov = htanfov/aspect_ratio;
   // vtanfov = tan( FOV / ar * 0.5 * M_PI / 180);
   // printf("vtanfov post aspect ratio set = %f\n", vtanfov);
   
   // auto vFOV = FOV / aspect_ratio;
   // auto vtanfov_new = tan( vFOV * 0.5 * M_PI / 180);
   // std::cout << "pre " << vtanfov_new << std::endl;
   // auto tanfov_ov_vtanfov = htanfov / vtanfov;
   // auto tanfov_ov_vtanfov_new = htanfov / vtanfov_new;
   // auto expected = aspect_ratio;

   // printf("vFOV = %f, FOV / VOF = %f \n", vFOV, FOV / vFOV);
   // printf("htanfov / vtanfov = %f: htanfov / vtanfov_new = %f: ar = %f\n", tanfov_ov_vtanfov, tanfov_ov_vtanfov_new, expected);
}

char * Camera::__str__() {
       static char tmp[1024];
       std::sprintf(tmp,"Camera( eye->Vector(%g,%g,%g), view->Vector(%g,%g,%g), up->Vector(%g,%g,%g), fov->%g, aspect->%g, near->%g, far->%g )", position.X(), position.Y(), position.Z(), axis_view.X(), axis_view.Y(), axis_view.Z(), axis_up.X(), axis_up.Y(), axis_up.Z(), FOV, aspect_ratio, near, far );
       return tmp;
}


bool Camera::isVisible( const Vector& P ) const
{
   Vector PP = P-position;
   double z = PP*axis_view;
   if( z <= 0 ){ return false; }
   double x = (PP*axis_right)/z;
   x = (x/htanfov + 1.0)/2.0;
   if( x < 0.0 ){ return false; }
   if( x > 1.0 ){ return false; }

   double y = (PP*axis_up)/z;
   y = (y/vtanfov + 1.0)/2.0;
   if( y < 0.0 ){ return false; }
   if( y > 1.0 ){ return false; }

   return true;
}
