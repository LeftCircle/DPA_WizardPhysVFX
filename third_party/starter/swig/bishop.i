
%module bishop 



%include <std_string.i>
//%include <boost_shared_ptr.i>
%include <std_shared_ptr.i>

%include "std_vector.i"
%template(StringArray) std::vector<std::string>;
%include "Vector.i"
%include "Matrix.i"
%include "Camera.i"
%include "Color.i"
%include "Volume.i"
%include "ProgressMeter.i"
%include "ImplicitVolumeShapes.i"
%include "ImplicitVectorShapes.i"
%include "ImplicitColors.i"
%include "Field.i"
%include "LinearAlgebra.i"

