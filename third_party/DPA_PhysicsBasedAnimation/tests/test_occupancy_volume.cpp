#include "catch_helpers.h"

#include <vector>
#include <span>

#include "AABB.h"
#include "Vector.h"
#include "occupancy_volume.h"

using namespace pba;

TEST_CASE("Test Occupancy Volume initialization") {
	// An occupancy volume is given an AABB and a cell size. 
	// It should then divide the AABB up into a grid of cell size h. 
	// If the AABB dimensions are not divisible by h, then the grid should
	// encompase all of the AABB plus some extra space on the right and top sides.
	Vector llc(0.0, 0.0, 0.0);
	Vector urc(5.0, 9.3, 13.7);
	AABB aabb(llc, urc);
	double h = 0.66;
	OccupancyVolume<std::vector<size_t>> vol(aabb, h);
	Vector dims = vol.get_dimensions();
	REQUIRE(dims.X() * h >= urc.X() - llc.X());
	REQUIRE(dims.Y() * h >= urc.Y() - llc.Y());
	REQUIRE(dims.Z() * h >= urc.Z() - llc.Z());
	REQUIRE((dims.X() - 1) * h < urc.X() - llc.X());
	REQUIRE((dims.Y() - 1) * h < urc.Y() - llc.Y());
	REQUIRE((dims.Z() - 1) * h < urc.Z() - llc.Z());
}


TEST_CASE("Test populate occupancy volume"){
	// Given an input of positions, we should populate the index of the position into 
	// each cell of the occupancy volume that it overlaps with. 
	const int n_pos = 5 * 5 * 5 * 3; // a 5x5x5 grid of points with 3 particles each ideally.
	const double h = 1.0;
	Vector llc(0.0, 0.0, 0.0);
	Vector urc(5.0, 5.0, 5.0);
	AABB aabb(llc, urc);
	OccupancyVolume<std::vector<size_t>> vol(aabb, h);
	
	REQUIRE(vol.get_dimensions() == Vector(5, 5, 5));

	std::vector<Vector> positions(n_pos);
	for (int k = 0; k < 5; k++) {
		for (int j = 0; j < 5; j++) {
			for (int i = 0; i < 5; i++) {
				for (int p = 0; p < 3; p++) {
					int idx = (i + 5 * (j + 5 * k)) * 3 + p;
					positions[idx] = Vector(i + 0.1 * p, j + 0.1 * p, k + 0.1 * p);
				}
			}
		}
	}

	// the volume should only take a span into the std::vector of positions
	vol.populate(positions, [](std::vector<size_t>& cell, size_t idx){
		cell.push_back(idx);
	});


	//Now we should check that each cell of the occupancy volume has the correct indices in it.
	for (int k = 0; k < 5; k++) {
		for (int j = 0; j < 5; j++) {
			for (int i = 0; i < 5; i++) {
				const auto& cell = vol.get_cell(i, j, k);
				REQUIRE(cell.size() == 3);
			}
		}
	}
}

