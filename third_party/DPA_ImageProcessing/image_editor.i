/* SWIG interface file for image_editor Python bindings */
%module image_editor

%{
/* Include the headers in the wrapper code */
#include "include/image_data.h"
#include "include/image_data_modifier.h"
#include "include/stencil.h"
#include "include/lut.h"
#include "include/stats.h"
#include "include/IFSFunction.h"
%}

/* Enable C++ exception handling */
%include "std_string.i"
%include "std_vector.i"
%include "std_shared_ptr.i"
%include "exception.i"

/* Declare shared_ptr types before class definitions */
%shared_ptr(ImageData)

/* Template instantiations for std::vector types used */
%template(FloatVector) std::vector<float>;
%template(StringVector) std::vector<std::string>;
%template(SizeTVector) std::vector<size_t>;
%template(ImageDataVector) std::vector<ImageData>;

/* Ignore problematic members that don't need Python exposure */
%ignore ImageData::gl_draw_pixels;
%ignore ImageData::transform_all_values_by;

/* Exception handling for C++ exceptions */
%exception {
    try {
        $action
    } catch (const std::exception& e) {
        SWIG_exception(SWIG_RuntimeError, e.what());
    }
}

/* Include the header files to generate wrappers */
%include "include/image_data.h"
%include "include/stencil.h"
%include "include/image_data_modifier.h"
