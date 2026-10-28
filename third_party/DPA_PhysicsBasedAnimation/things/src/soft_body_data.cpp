#include "soft_body_data.h"

using namespace pba;

void SoftBody::connect_all_particles_together() noexcept{
    connect_all_particles_in_range(0, _n_particles);
};

void SoftBody::connect_all_particles_in_range(size_t start_idx, size_t end_idx){
    if (start_idx < 0 || start_idx > _n_particles || end_idx > _n_particles){
        throw(std::runtime_error("Indexing out of bounds when connecting soft body\n"));
    }
    span<const Vector> positions = get_vector_attribute_span("positions");
    for (size_t i = start_idx; i < end_idx; i++){
        for (size_t j = i + 1; j < end_idx; j++){
            edges.emplace_back(i, j, (positions[i] - positions[j]).magnitude());
        }        
    }
}


