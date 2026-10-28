#include "force_library.h"
#include <boost/range/adaptor/transformed.hpp>

using namespace pba;


void reduce_indexed_force_into(idx_force_vec& idx_force, span<Vector> results){
	for (auto& idx_f : idx_force){
		results[idx_f.idx] += idx_f.force;
	}
}

void SimpleGravityForce::compute(DSD_sp dsd, const double dt) const {
	const size_t n = dsd->n_particles();
	auto acc = dsd->get_vector_attribute_span("acceleration");
	std::for_each(acc.begin(), acc.end(),
		[g=_gravity](Vector& a){ a += g; }
	);
}

void SPHPressureForce::compute_sph(SPHData_sp sph_data, const double dt) const {
	const size_t n = sph_data->n_particles();
	const auto densities = sph_data->get_double_attribute_span("density");
	#pragma omp parallel for
	for( size_t i=0; i<n; i++ ){
		Vector pressure_force = _occupancy_volume->accumulate_neighbor_cells(
			i,
			sph_data->get_position(i),
			Vector(0.0, 0.0, 0.0),
			[&sph_data, &densities, this](size_t idx, Vector acc, const std::vector<size_t>& neighbor_indices){
				const auto neighbor_indices_span(neighbor_indices);
				return acc + _get_pressure_with_uniform_h(
					idx,
					neighbor_indices_span,
					sph_data,
					densities);
			}
		);
		sph_data->set_acceleration(i, sph_data->get_acceleration(i) - pressure_force / sph_data->get_mass(i));
	}
}

Vector SPHPressureForce::_get_pressure_with_uniform_h(
		size_t i,
		const span<const size_t>& neighbor_indices,
		const SPHData_sp sph_data,
		const span<const double>& densities
	) const
{
	const Vector& pos_i = sph_data->get_position(i);
	const auto dens_i = densities[i];
	const auto p_at_i = _get_pressure_at_density(dens_i, sph_data->rest_pressure(), sph_data->rest_density(), sph_data->gamma());
	const auto d_i_sq = dens_i * dens_i;
	Vector pressure_force(0.0, 0.0, 0.0);
	for (size_t j : neighbor_indices) {
		const Vector& pos_j = sph_data->get_position(j);
		double distance = (pos_i - pos_j).magnitude();
		Vector grad_ab = _kernel->gradient(distance, pos_i - pos_j);
		const auto dens_j = densities[j];
		const auto p_at_j = _get_pressure_at_density(dens_j, sph_data->rest_pressure(), sph_data->rest_density(), sph_data->gamma());
		const auto d_j_sq = dens_j * dens_j;
		pressure_force += sph_data->get_mass(j) * (p_at_i / d_i_sq + p_at_j / d_j_sq) * grad_ab;
	}
	return pressure_force;
}

double SPHPressureForce::_get_pressure_at_density(double density, double rest_pressure, double rest_density, double gamma) const {
	double p = rest_pressure * (pow(density / rest_density, gamma) - 1.0);
	if (p < 0){
		//printf("Negative pressure\n");
		p *= 0.001;
	}
 	return p;
}

void SPHViscosityForce::compute_sph(SPHData_sp sph_data, const double dt) const {
	const size_t n = sph_data->n_particles();
	#pragma omp parallel for
	for( size_t i=0; i<n; i++ ){
		Vector viscosity_force = _occupancy_volume->accumulate_neighbor_cells(
			i,
			sph_data->get_position(i),
			Vector(0.0, 0.0, 0.0),
			[&sph_data, this](size_t idx, Vector acc, const std::vector<size_t>& neighbor_indices){
				//return acc + _doyub_viscocity_for_neighbors(idx, neighbor_indices, sph_data);
				return acc + _compute_viscosity_force_for_neighbors(idx, neighbor_indices, sph_data);
			}
		);
		viscosity_force /= sph_data->get_mass(i);
		sph_data->set_acceleration(i, sph_data->get_acceleration(i) - viscosity_force );
	}
}

Vector SPHViscosityForce::_compute_viscosity_force_for_neighbors(size_t i, const span<const size_t>& neighbor_indices, const SPHData_sp sph_data) const {
	const auto densities = sph_data->get_double_attribute_span("density");
	const auto one_over_rd = 1.0 / sph_data->rest_density();
	return -std::accumulate(neighbor_indices.begin(), neighbor_indices.end(), Vector(0.0, 0.0, 0.0), 
		[&sph_data, i, &densities, one_over_rd, this](Vector acc, size_t j){
			const auto& b_pos = sph_data->get_position(j);
			double c_a = _avg_speed_of_sound(densities[i], one_over_rd, sph_data->rest_pressure(), sph_data->gamma());
			double c_b = _avg_speed_of_sound(densities[j], one_over_rd, sph_data->rest_pressure(), sph_data->gamma());
			double c_ab = (c_a + c_b) / 2.0; // why not divide by 2??
			double mu_ab = _mu_ab(sph_data->get_velocity(i), sph_data->get_velocity(j), sph_data->get_position(i), b_pos, sph_data->h(), sph_data->viscosity_epsilon());
			double pi_ab = _pi_ab(c_ab, mu_ab, densities[i], densities[j], sph_data->viscosity_alpha(), sph_data->viscosity_beta());
			Vector ab = sph_data->get_position(i) - b_pos;
			double distance = ab.magnitude();
			if (ab.magnitude() > 0.0)  ab.normalize();
			//Vector thing_to_acc = sph_data->get_mass(j) * pi_ab * _kernel->gradient(distance, ab);
			//Vector kernel_grad = _kernel->gradient(distance, ab);
			//Vector kernel_grad = _kernel->gradient(distance, ab);
			return acc + sph_data->get_mass(j) * pi_ab * _kernel->gradient(distance, ab);
		}
	);
}

Vector SPHViscosityForce::_doyub_viscocity_for_neighbors(
		size_t i, 
		const span<const size_t>& neighbor_indices,
		const SPHData_sp sph_data) const
{
	const auto& d = sph_data->get_double_attribute_span("density");
	return std::accumulate(neighbor_indices.begin(), neighbor_indices.end(), Vector(0, 0, 0),
		[&sph_data, i, &d, this](Vector acc, size_t j){
			double dist = (sph_data->get_position(i) - sph_data->get_position(j)).magnitude();
			acc += sph_data->viscosity_beta() * sph_data->get_mass(i) * sph_data->get_mass(j) *
				(sph_data->get_velocity(i) - sph_data->get_velocity(j)) / d[j] * 
				_kernel->second_derivative(dist);
			return acc;
		}
	);
}

double SPHViscosityForce::_avg_speed_of_sound(double density, double one_over_rest_density, double rest_pressure, double gamma) const noexcept {
	return std::sqrt(gamma * one_over_rest_density * rest_pressure * pow(density * one_over_rest_density, gamma - 1));
}

double SPHViscosityForce::_mu_ab(const Vector& vel_a, const Vector& vel_b, const Vector& pos_a, const Vector& pos_b, double h, double epsilon) const {
	Vector vel_diff = vel_a - vel_b;
	Vector pos_diff = pos_a - pos_b;
	double mag_pos_diff_sq = pos_diff * pos_diff;
	double dot = vel_diff * pos_diff;
	return (dot * h) / (mag_pos_diff_sq + epsilon * h * h);
}

double SPHViscosityForce::_pi_ab(double c_ab, double mu_ab, double density_a, double density_b, double alpha, double beta) const {
	return (-alpha * c_ab * mu_ab + beta * mu_ab * mu_ab) / (density_a + density_b);
}

idx_force_vec UniformStrutForce::compute_acceleration(DSD_scp dsd) const {
	idx_force_vec forces;
	std::shared_ptr<const SoftBody> sb = std::dynamic_pointer_cast<const SoftBody>(dsd);
	std::vector<IndexedForce> idx_force;
	if (!sb) return idx_force;

	span<const Vector> positions = sb->get_vector_attribute_span("positions");
	span<const Vector> vels = sb->get_vector_attribute_span("velocities");
	const size_t n_edges = sb->edges.size();
	//#pragma omp parallel for
	for (size_t i = 0; i < n_edges; i++){
		Vector acc = sb->edges[i].get_acceleration_on_a(positions, vels, _spring_force, _friction);
		auto idxs = sb->edges[i].get_indices();
		forces.push_back({idxs.first, acc / sb->get_mass(idxs.first)});
		forces.push_back({idxs.second, -acc / sb->get_mass(idxs.second)});
	};
	return forces;
}

void StrutForce::_compute(std::shared_ptr<SoftBody> sb, const double dt) const{
	span<const Vector> positions = sb->get_vector_attribute_span("positions");
	span<const Vector> vels = sb->get_vector_attribute_span("velocities");
	const size_t n_edges = sb->edges.size();
	#pragma omp parallel for
	for (size_t i = 0; i < n_edges; i++){
		sb->edges[i].compute(positions, vels, sb->edges[i].get_k(), sb->edges[i].get_friction());
	}
	// Now we have to write the forces in. Can't be parallel b/c edges point to multiple points
	for (auto& edge : sb->edges){
		std::pair<size_t, size_t> idxs = edge.get_indices();
		sb->set_acceleration(idxs.first, sb->get_acceleration(idxs.first) + edge.get_force_on_a() / sb->get_mass(idxs.first));
		sb->set_acceleration(idxs.second, sb->get_acceleration(idxs.second) - edge.get_force_on_a() / sb->get_mass(idxs.second));
	}
}

void UniformStrutForce::_compute(std::shared_ptr<SoftBody> sb, const double dt) const {
	// For each edge, determine spring force and friction based on position, rest length, and velocity
	span<const Vector> positions = sb->get_vector_attribute_span("positions");
	span<const Vector> vels = sb->get_vector_attribute_span("velocities");
	const size_t n_edges = sb->edges.size();
	#pragma omp parallel for
	for (size_t i = 0; i < n_edges; i++){
		sb->edges[i].compute(positions, vels, _spring_force, _friction);
	}
	// Now we have to write the forces in. Can't be parallel b/c edges point to multiple points
	for (auto& edge : sb->edges){
		std::pair<size_t, size_t> idxs = edge.get_indices();
		sb->set_acceleration(idxs.first, sb->get_acceleration(idxs.first) + edge.get_force_on_a() / sb->get_mass(idxs.first));
		sb->set_acceleration(idxs.second, sb->get_acceleration(idxs.second) - edge.get_force_on_a() / sb->get_mass(idxs.second));
	}
}

void PciSPHPressureForce::compute_sph(SPHData_sp dsd, const double dt) const {
	const size_t n = dsd->n_particles();
	double target_dens = dsd->rest_density();

	auto m = dsd->get_float_attribute_span("mass");
	auto p = dsd->get_vector_attribute_span("positions");
	auto v = dsd->get_vector_attribute_span("velocities");
	auto new_v = dsd->get_vector_attribute_span("new_velocities");
	auto new_p = dsd->get_vector_attribute_span("new_positions");
	auto p_forces = dsd->get_vector_attribute_span("pressure_force");
	auto pressure_d = dsd->get_double_attribute_span("pressures");
	auto a = dsd->get_vector_attribute_span("acceleration");
	auto ds = dsd->get_double_attribute_span("predicted_density");
	const auto& densities = dsd->get_double_attribute_span("density");
	auto dens_error = dsd->get_double_attribute_span("density_error");

	const double delta = dsd->rest_pressure();//_accumulate_delta(dsd, dt);
	auto kernel = _kernel;
	
	// init buffers;
	#pragma omp parallel for
	for (size_t i = 0; i < n; i++){
		p_forces[i] = Vector(0, 0, 0);
		//pressure_d[i] = _get_pressure_at_density(densities[i], dsd->rest_pressure(), dsd->rest_density(), dsd->gamma());
		pressure_d[i] = 0.0;
	}

	for (unsigned int k = 0; k < 50; k++){
		// predict vel and position
		#pragma omp parallel for
		for (size_t i = 0; i < n; i++){
			new_v[i] = v[i] + dt * (a[i] + p_forces[i] / m[i]);
			new_p[i] = p[i] + dt * new_v[i]; 
		}
		_occupancy_volume->populate(new_p, [](std::vector<size_t>& cell, size_t i){ cell.push_back(i); });
		// resolve collision
		//_collision_handler->handle_collisions_no_start_pos_update(dsd, "new_positions", "new_velocities", dt);
		// Might have to update the occupancy volume here?
		
		// compute pressure from density error
		#pragma omp parallel for
		for (size_t i = 0; i < n; i++){
			double weight_sum = _occupancy_volume->accumulate_neighbor_cells(
				i,
				new_p[i],
				0.0,
				[&new_p, kernel](size_t index, double weight_sum, const std::vector<size_t>& n_idxs){
					for (size_t j : n_idxs){
						double dist = (new_p[index] - new_p[j]).magnitude();
						weight_sum += (*kernel)(dist);
					}  
					return weight_sum;
				}
			);
			double dens = m[i] * weight_sum;
			double dens_error_i = dens - target_dens;
			double pressure = delta * dens_error_i;

			pressure_d[i] += pressure;
			ds[i] = dens;
			dens_error[i] = dens_error_i;
		}
		
		// Now have to accumulate pressure gradient forces??
		_accumulate_pressure_gradient_force(dsd, p, ds, pressure_d, p_forces);
		// compute max density error
		double max_density_error = *std::max_element(dens_error.begin(), dens_error.end(),
			 [](double a, double b){ return std::max(std::abs(a), std::abs(b)); }
		);
		max_density_error = std::abs(max_density_error);
		// double max_density_error = 0.0;
		// for (size_t i = 0; i < n; i++){
		// 	max_density_error = std::max(std::abs(dens_error[i]), std::abs(max_density_error));
		// }

		//printf("Max density error %f\n", max_density_error);

		double density_error_ratio = max_density_error / target_dens;
		if (std::abs(density_error_ratio) < _max_density_error_ratio){
			printf("k = %zu  %f\n", k, (float)k);
			break;
		}
	}

	// Accumulate pressure force
	#pragma omp parallel for
	for (size_t i = 0; i < n; i++){
		a[i] += p_forces[i] / dsd->get_mass(i);
		//Vector pforce = p_forces[i] / dsd->get_mass(i);
		//printf("pressure force = %f %f %f \n", pforce.X(), pforce.Y(), pforce.Z());
	}
}

void PciSPHPressureForce::_accumulate_pressure_gradient_force(
	SPHData_sp dsd,
	span<Vector>& pos,
	span<double>& densities,
	span<double>& pressures,
	span<Vector>& pressure_forces) const
{
	const double msq = dsd->get_mass(0) * dsd->get_mass(0);
	#pragma omp parallel for
	for (size_t i = 0; i < dsd->n_particles(); i++){
		Vector p_force = _occupancy_volume->accumulate_neighbor_cells(
			i, dsd->get_position(i), Vector(0, 0, 0), 
			[&dsd, &pressure_forces, msq, &pressures, &densities, this](size_t index, Vector p_sum, const std::vector<size_t>& n_idxs){
				//printf("n neighbors = %zu\n", n_idxs.size());
				for (size_t j : n_idxs){
					Vector vec = dsd->get_position(index) - dsd->get_position(j);
					double dist = vec.magnitude();
					if (dist > 0.0){
						Vector dir = vec / dist;
						p_sum -= ((msq *
							(pressures[index] / (densities[index] * densities[j])) + 
							(pressures[j] / (densities[index] + densities[j]))) *
							_kernel->gradient(dist, dir)
						);
					}
				}
				return p_sum;
			}
		);
		pressure_forces[i] += p_force;
	}
}

double PciSPHPressureForce::_accumulate_delta(SPHData_sp dsd, const double dt) const {
	const double h = dsd->h();
	const double h_over_2 = h * 0.5;
	std::vector<Vector> points(9);
	// get the 9 points. 8 for edges of bounding box, and one in center
	points[0] = Vector(0, 0, 0);
	points[1] = Vector(h_over_2, h_over_2, h_over_2);
	points[2] = Vector(h_over_2, h_over_2, -h_over_2);
	points[3] = Vector(h_over_2, -h_over_2, h_over_2);
	points[4] = Vector(-h_over_2, h_over_2, h_over_2);
	points[5] = Vector(h_over_2, -h_over_2, -h_over_2);
	points[6] = Vector(-h_over_2, h_over_2, -h_over_2);
	points[7] = Vector(-h_over_2, -h_over_2, h_over_2);
	points[8] = Vector(-h_over_2, -h_over_2, -h_over_2);

	double denom = 0;
	Vector denom1(0, 0, 0);
	double denom2 = 0;
	for (size_t i = 0; i < 9; i++){
		const Vector& point = points[i];
		double dsq = point * point;
		if (dsq < h * h){
			double d = std::sqrt(dsq);
			Vector direction = (d > 0.0) ? point / d : Vector(0, 0, 0);
			Vector gradWij = _kernel->gradient(d, direction);
			denom1 += gradWij;
			denom2 += gradWij * gradWij;
		}
	}
	denom += -denom1 * denom1 - denom2;
	return (std::abs(denom) > 0) ? -1.0 / (_compute_beta(dsd, dt) * denom) : 0;
}

double PciSPHPressureForce::_compute_beta(SPHData_sp dsd, const double dt) const {
	double dtm_over_p = dt * dsd->get_mass(0) / dsd->rest_density();
	return 2.0 * dtm_over_p * dtm_over_p;
}

void SoftTriangleForce::compute(
	DSD_sp dsd, 
	const double dt
) const {
	auto soft_body_sp = std::dynamic_pointer_cast<SoftBody>(dsd);
	if (!soft_body_sp) throw std::runtime_error("Strut forces require a soft body");
	compute(dsd, dt, span(soft_body_sp->soft_triangles.data(), soft_body_sp->soft_triangles.size()));
}

void SoftTriangleForce::compute(
	DSD_sp dsd,
	const double dt,
	span<const SoftTriangle> soft_triangles) const
{
	auto deltas = compute_acceleration(dsd, soft_triangles);
	
	// This part could be parallelized as well
	for (size_t i = 0; i < dsd->n_particles(); i++){
		dsd->set_acceleration(i, dsd->a(i) + deltas[i]);
	}
}

std::vector<Vector> SoftTriangleForce::compute_acceleration(
	const DSD_scp dsd,
	span<const SoftTriangle> soft_triangles
) const{
	auto positions = dsd->get_vector_attribute_span("positions");
	// Assuming that all of the particles are part of the soft triangles 
	std::vector<Vector> deltas(dsd->n_particles(), Vector(0, 0, 0));

	// could parallelize here
	// auto contributions = soft_triangles 
	// 	| boost::adaptors::transformed(
	// 		[&](const SoftTriangle& tri) -> TriForces {
	// 			return compute_soft_tri_forces(positions, tri);
	// 	}
	// );
	
	// for (const auto& tri_forces : contributions){
	// 	for (const auto& f : tri_forces){
	// 		deltas[f.idx] += f.force;
	// 	}
	// }
	for (const SoftTriangle& tri : soft_triangles) {
		const TriForces tri_forces =
			compute_soft_tri_forces(positions, tri);

		for (const auto& force : tri_forces) {
			deltas[force.idx] += force.force;
		}
	}
	return std::move(deltas);
}

TriForces SoftTriangleForce::compute_soft_tri_forces(
	span<const Vector> positions,
	const SoftTriangle& tri
) const {
	const Vector& p0 = positions[tri[0]];
    const Vector& p1 = positions[tri[1]];
    const Vector& p2 = positions[tri[2]];

    const Vector d0 = (p0 - 0.5 * (p1 + p2));
    const Vector d1 = (p1 - 0.5 * (p0 + p2));
    const Vector d2 = (p2 - 0.5 * (p0 + p1));

    const double area = 0.5 * ((p1 - p0) ^ (p2 - p0)).magnitude();
    const double force_mag = tri.k() * (1.0 - area / tri.rest_area());

    return {{
        {tri[0], force_mag * d0},
        {tri[1], force_mag * d1},
        {tri[2], force_mag * d2},
    }};
}