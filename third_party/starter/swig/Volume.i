
%module bishop
%{
#include "Volume.h"
%}

//%include <boost_shared_ptr.i>
%include <std_shared_ptr.i>
%shared_ptr(ScalarFieldBase);
%shared_ptr(VectorFieldBase);
%shared_ptr(ColorFieldBase);



%include "Volume.h"
%template(ScalarVolume) lux::Volume<float>;
%template(VectorVolume) lux::Volume<lux::Vector>;
%template(ColorVolume) lux::Volume<lux::Color>;

%template(ScalarFieldBase)  std::shared_ptr<lux::Volume<float> >;
%template(VectorFieldBase)  std::shared_ptr<lux::Volume<Vector> >;
%template(ColorFieldBase)  std::shared_ptr<lux::Volume<Color> >;
