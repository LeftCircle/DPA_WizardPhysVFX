#ifndef _SHAPES_H
#define _SHAPES_H

#include "Vector.h"

namespace pba{

struct Triangle{
	Triangle(const Vector& a, const Vector& b, const Vector& c)
	: v0(a), v1(b), v2(c) {}
	Triangle() = default;
	

	void set(const Vector& a, const Vector& b, const Vector& c){
		v0 = a; v1 = b; v2 = c;	
	}
	Vector get_normal(){ return(v1 - v0) ^ (v2 - v0); };
	bool is_close(Vector p) {
		double dist_to_tri = (p - v0) * get_normal();
		return dist_to_tri < std::max(std::max((v0 - v1).magnitude(), (v2 - v1).magnitude()), (v0 - v2).magnitude());
	}

	double get_area() { return get_area(v0, v1, v2); }

	static double get_area(const Vector& a, const Vector& b, const Vector& c) {
		return ((b - a) ^ (c - a)).magnitude() * 0.5;
	}

	Vector v0;
	Vector v1;
	Vector v2;
};



} // end namespace pba


#endif