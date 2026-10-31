#include "deep_shadow_map.h"
#include <openvdb/tools/ValueTransformer.h>


namespace lux{

GridPtr<float> make_dsm_grid_in_regions(
    const vspf density_field,
    float voxel_size,
    const coordbbox& bounds,
    const PointLight& light,
    float step_size,
    float kappa,
    const std::vector<ShadowRegion>& regions
){
    // create the floatgrid based on the inputs
    auto stamp_func = [density_field](const Vector& p){ return density_field->eval(p); };
    auto cond = [](const float val){ return val < 0; };
    
    auto grid = stamp_grid_parallel<float>(voxel_size, 0.00001f, stamp_func, cond, bounds);

    // Grid should now have active cells can do a parallel for each now
    auto eval_func = [density_field, kappa](const Vector& p){ 
        auto val = density_field->eval(p) * kappa;
        return val < 0 ? val : 0;
    };

    auto region_union = regions;
    if (region_union.empty()) {
        region_union.emplace_back(Vector(grid->indexToWorld(bounds.min())), Vector(grid->indexToWorld(bounds.max())));
    }
    auto op = [&grid, eval_func, &light, &step_size, &region_union](
        const openvdb::FloatGrid::ValueOnIter& iter
    ){
        Vector start_vec(grid->indexToWorld(iter.getCoord()));
        auto val = accumulate_in_regions(start_vec, light.position, step_size, eval_func, region_union);
        iter.setValue(val);
    };

    // Now run the computation
    openvdb::tools::foreach(grid->beginValueOn(), op);

   return grid;
}

GridPtr<float> make_dsm_grid(
    const vspf density_field, float voxel_size, const coordbbox& bounds,
    const PointLight& light, float step_size, float kappa
) {
    return make_dsm_grid_in_regions(density_field, voxel_size, bounds, light,
                                    step_size, kappa, {});
}

vspf make_deep_shadow_map(
    vspf density_field,
    float voxel_size,
    const coordbbox& bounds,
    const PointLight& light,
    float step_size,
    float kappa
){
    return make_deep_shadow_map_parallel(density_field, voxel_size, bounds, light, step_size, kappa);
}

vspf make_deep_shadow_map(const GridPtr<float> grid){
    return exp(make_grid_field<float>(grid));
}

vspf make_deep_shadow_map_parallel(
    const vspf density_field,
    float voxel_size,
    const coordbbox& bounds,
    const PointLight& light,
    float step_size,
    float kappa
){
    auto grid = make_dsm_grid(
        density_field,
        voxel_size,
        bounds,
        light,
        step_size,
        kappa
    );

    // transmissivty of shadowgrid
    return make_deep_shadow_map(grid);
}



} // end namespace lux



