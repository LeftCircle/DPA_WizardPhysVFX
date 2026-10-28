#include "catch_helpers.h"
#include "dynamical_state_data.h"
#include "sph_kernel.h"
#include "partial_solvers.h"
#include "Vector.h"
#include "sph_solver.h"

using namespace pba;


auto create_uniform_dsd_sph_grid(int x_dim, int y_dim, int z_dim, double mass, double h) {
	auto dsd = create_dynamical_state_data();
	dsd->add_attribute<double>("h", DSAd("h", h));
	dsd->resize(x_dim * y_dim * z_dim);
	// create a uniform grid of particles
	for (int k = 0; k < z_dim; ++k) {
		for (int j = 0; j < y_dim; ++j) {
			for (int i = 0; i < x_dim; ++i) {
				size_t idx = k * x_dim * y_dim + j * x_dim + i;
				dsd->set_position(idx, Vector(i, j, k));
				dsd->set_mass(idx, mass);
				dsd->set_double_attribute("h", idx, h);
			}
		}
	}
	return dsd;
}



TEST_CASE("Test density is nonzero for particles"){
	// With a uniform grid of particles, the density of the center
	// particle should be nonzero when we use a partial solver to compute it

	int dims = 10;
	auto dsd = create_uniform_dsd_sph_grid(dims, dims, dims, 1.0, 2.0);
	auto center_idx = (dims / 2) * dims * dims + (dims / 2) * dims + (dims / 2);

	// Let's calculate the density of the center particle and assure it is nonzero
	std::vector<size_t> neighbor_indices;
	// just use all indices as neighbors atm
	for (int i = 0; i < dsd->n_particles(); i++) {
		neighbor_indices.push_back(i);
	}
	auto kernel = CubicSplineKernel3(2.0);

	double density = get_density_with_uniform_h(center_idx, span<const size_t>(neighbor_indices), dsd, kernel);
	REQUIRE(density > 0.0);
}

TEST_CASE("profile std::accumulate vs for loop "){
	int dims = 100;
	auto dsd = create_uniform_dsd_sph_grid(dims, dims, dims, 1.0, 2.0);
	auto center_idx = (dims / 2) * dims * dims + (dims / 2) * dims + (dims / 2);

	// Let's calculate the density of the center particle and assure it is nonzero
	std::vector<size_t> neighbor_indices;
	// just use all indices as neighbors atm
	for (int i = 0; i < dsd->n_particles(); i++) {
		neighbor_indices.push_back(i);
	}
	auto kernel = CubicSplineKernel3(2.0);

	// BENCHMARK("std::accumulate") {
	// 	return get_density_with_uniform_h(center_idx, span<const size_t>(neighbor_indices), dsd, kernel);
	// };
	// BENCHMARK("for loop") {
	// 	return get_density_with_uniform_h_silly_loop(center_idx, span<const size_t>(neighbor_indices), dsd, kernel);
	// };
}


TEST_CASE("Test SPH density is nearly constant with varrying h"){
	// With a uniform grid of particles, the density of the center
	// particle should remain basically the same when the size of 
	// the area of influence (h) of the kernal changes

	auto dsd = create_dynamical_state_data();
	double h = 0.25;
	int x_dim = 10;
	int y_dim = 10;
	int z_dim = 10;
	double mass = 1.0;

	dsd->resize(x_dim * y_dim * z_dim);
	// create a uniform grid of particles
	for (int k = 0; k < z_dim; ++k) {
		for (int j = 0; j < y_dim; ++j) {
			for (int i = 0; i < x_dim; ++i) {
				size_t idx = k * x_dim * y_dim + j * x_dim + i;
				dsd->set_position(idx, Vector(i, j, k));
				dsd->set_mass(idx, mass);
			}
		}
	}

	// Now use a partial solver to determine the density of the center particle
	auto center_idx = (z_dim / 2) * x_dim * y_dim + (y_dim / 2) * x_dim + (x_dim / 2);

}