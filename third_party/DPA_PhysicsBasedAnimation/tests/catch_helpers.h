#include "catch_amalgamated.hpp"

#include <string>
#include <iostream>

#include "Vector.h"
#include "partial_solvers.h"
#include "force_library.h"
#include "the_wheel.h"


#define REQUIRE_VECTOR_APPROX(actual, expected, eps) \
	do { \
		const pba::Vector& _actual = (actual); \
		const pba::Vector& _expected = (expected); \
		REQUIRE( Catch::Approx( _actual.X() ).margin(eps) == _expected.X() ); \
		REQUIRE( Catch::Approx( _actual.Y() ).margin(eps) == _expected.Y() ); \
		REQUIRE( Catch::Approx( _actual.Z() ).margin(eps) == _expected.Z() ); \
	} while(0)

namespace Catch {

template<>
struct StringMaker<pba::Vector>{
	static std::string convert(const pba::Vector& v){
		char buffer[100];
		std::snprintf(buffer, sizeof(buffer), "Vector(%.5f, %.5f, %.5f)", v.X(), v.Y(), v.Z());
		return std::string(buffer);
	}
};

} // end namespace Catch

namespace pba {


	

} // end namespace pba