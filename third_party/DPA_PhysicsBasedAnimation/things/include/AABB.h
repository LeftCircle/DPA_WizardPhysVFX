#ifndef _AABB_H
#define _AABB_H

#include "Vector.h"

namespace pba{

template<typename Vec3>
class AABB {
public:
	AABB() : llc(0, 0, 0), urc(0, 0, 0) {}
	AABB(const Vec3& lower_left, const Vec3& upper_right)
	: llc(lower_left), urc(upper_right) {}

	const Vec3& lower_left() const { return llc; }
	const Vec3& upper_right() const { return urc; }
	bool contains(const Vec3& point) const {
		return (point.X() >= llc.X() && point.X() <= urc.X() &&
				point.Y() >= llc.Y() && point.Y() <= urc.Y() &&
				point.Z() >= llc.Z() && point.Z() <= urc.Z());
	}
	const Vec3 size() const {return urc - llc; }
	void expand_to_include(const Vec3& point) {
		llc[0] = std::min(llc.X(), point.X());
		llc[1] = std::min(llc.Y(), point.Y());
		llc[2] = std::min(llc.Z(), point.Z());
		urc[0] = std::max(urc.X(), point.X());
		urc[1] = std::max(urc.Y(), point.Y());
		urc[2] = std::max(urc.Z(), point.Z());
	}

private:
	Vec3 llc;
	Vec3 urc;
};

} // end namespace pba
#endif