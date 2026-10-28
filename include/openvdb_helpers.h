#pragma once

#include <string>
#include <limits>
#include <openvdb/openvdb.h>
#include <openvdb/tools/ValueTransformer.h>
#include <openvdb/tools/MeshToVolume.h>
#include <tbb/parallel_for.h>
#include <cmath>

#include "field_interface.h"
#include "Vector.h"

namespace lux{

using ocoord = openvdb::Coord;
using coordbbox = openvdb::CoordBBox;

openvdb::CoordBBox world_space_to_bounds(const Vector& llc, const Vector& urc, const openvdb::FloatGrid::Ptr grid);
openvdb::CoordBBox world_space_to_bounds(const Vector& llc, const Vector& urc, float voxel_size);
openvdb::FloatGrid::Ptr create_float_grid(float voxel_size, float default_val = 0.0f);
openvdb::Vec3SGrid::Ptr create_color_grid(float voxel_size, openvdb::Vec3s def_val = openvdb::Vec3s(0, 0, 0));

openvdb::FloatGrid::Ptr obj_mesh_to_level_set_f(const std::string& obj_path, float voxel_size, float half_width, bool recenter = true);
VSPtr<float> obj_mesh_to_grid_field(const std::string& obj_path, float voxel_size, float half_width, bool recenter = true);

// TODO -> template and parallelyze all of it
GridPtr<float> stamp_isf_to_grid(
    float voxel_size,
    float def_val,
    const vspf& a,
    const coordbbox& bounds,
    std::string filename = ""
);

GridPtr<Color> stamp_color_to_grid(
    const vspc& a,
    const openvdb::CoordBBox& bounds,
    float voxel_size,
    std::string filename = ""
);

// Allows us to create float or Color grids based on 
// the template specialization in field_interface
template <typename T>
GridPtr<T> create_grid(
    float voxel_size,
    typename Grid<T>::ValueType background = typename Grid<T>::ValueType{}
){
    auto grid = Grid<T>::create(background);
    grid->setGridClass(openvdb::GridClass::GRID_FOG_VOLUME);
    grid->setTransform(openvdb::math::Transform::createLinearTransform(voxel_size));
    return grid;
}

template <typename GridType>
void save_to_disk(const GridType& grid, std::string& filename){
    openvdb::io::File file(filename);
    file.write({grid});
    file.close();
}


openvdb::FloatGrid::Ptr mesh_to_levelset(
    float voxel_size,
    std::vector<openvdb::Vec3s> points,
    std::vector<openvdb::Vec3I> triangles,
    std::vector<openvdb::Vec4I> quads,
    float half_width
);


template <typename T, typename Func, typename CondFunc>
void stamp_grid(typename GridTypes<T>::GridType grid, Func func, CondFunc condition_func, const openvdb::CoordBBox& bounds){
    auto voxel_size = (float)grid->transform().voxelSize().x();
    //printf("Voxel size is %g \n", voxel_size);
    auto accessor = grid->getAccessor();
    // now loop over each voxel in the AABB
    for (int z = bounds.min().z(); z <= bounds.max().z(); z++){
        for (int y = bounds.min().y(); y <= bounds.max().y(); y++){
            for (int x = bounds.min().x(); x <= bounds.max().x(); x++){
                auto pvdb = grid->indexToWorld(ocoord(x, y, z));
                Vector p = Vector(pvdb.x(), pvdb.y(), pvdb.z());
                auto val = func(p);
                if (!condition_func(val)) continue;
                accessor.setValue(ocoord(x, y, z), GridTypes<T>::to_grid(val));
            }
        }
    }
}

template <typename T, typename Func, typename CondFunc>
GridPtr<T> stamp_grid_parallel(
    float voxel_size,
    typename Grid<T>::ValueType default_value,
    Func func,
    CondFunc condition_func,
    const coordbbox& bounds,
    std::string filename = ""
){
    // let tbb craete some local grids
    tbb::enumerable_thread_specific<typename GridTypes<T>::GridType> thread_grids(
        [&](){return create_grid<T>(voxel_size, default_value); }
    );
    tbb::parallel_for(
        bounds,
        [&](const openvdb::CoordBBox& sub_bounds){
            auto local_grid = thread_grids.local();
            stamp_grid<T>(
                local_grid,
                func,
                condition_func,
                sub_bounds
            );
        }
    );
    
    auto result = create_grid<T>(voxel_size, default_value);
    for (const auto& local_grid : thread_grids) {
        result->tree().merge(local_grid->tree());
    }
    // TODO -> this should be it's own wrapper function 
    if (filename != ""){
        save_to_disk(result, filename);
    }
    return result;
}

template<typename Accessor, typename T>
void trilinear_deposit(
    Accessor& accessor,
    const openvdb::math::Transform& transform,
    const openvdb::Vec3s& worldPos,
    T val)
{
    const openvdb::Vec3d ijk = transform.worldToIndex(worldPos);

    const int i = static_cast<int>(std::floor(ijk.x()));
    const int j = static_cast<int>(std::floor(ijk.y()));
    const int k = static_cast<int>(std::floor(ijk.z()));

    const auto fx = ijk.x() - i;
    const auto fy = ijk.y() - j;
    const auto fz = ijk.z() - k;

    for (int dz = 0; dz <= 1; ++dz) {
        for (int dy = 0; dy <= 1; ++dy) {
            for (int dx = 0; dx <= 1; ++dx) {
                const auto weight =
                    (dx ? fx : 1.0 - fx) *
                    (dy ? fy : 1.0 - fy) *
                    (dz ? fz : 1.0 - fz);

                if (weight == 0.0) continue;

                const openvdb::Coord c(i + dx, j + dy, k + dz);
                accessor.setValueOn(c, accessor.getValue(c) + val * weight);
            }
        }
    }
}



} // end namespace lux




