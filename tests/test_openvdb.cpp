#include <iostream>
#include <string>
#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include <openvdb/openvdb.h>
#include <openvdb/tools/LevelSetSphere.h> // replace with your own dependencies for generating the OpenVDB grid
#include <nanovdb/tools/CreateNanoGrid.h> // converter from OpenVDB to NanoVDB (includes NanoVDB.h and GridManager.h)
#include <nanovdb/io/IO.h>
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/VolumeToMesh.h>


#include "field_interface.h"
#include "openvdb_helpers.h"
#include "obj_reader.h"
#include "AABB.h"

namespace lux{

TEST_CASE("test openvdb works with hello world example"){
    // Hello world is here: https://www.openvdb.org/documentation/doxygen/codeExamples.html#sHelloWorld
        // Initialize the OpenVDB library.  This must be called at least
    // once per program and may safely be called multiple times.
    openvdb::initialize();
 
    // Create an empty floating-point grid with background value 0.
    openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create();
 
    std::cout << "Testing random access:" << std::endl;
 
    // Get an accessor for coordinate-based access to voxels.
    openvdb::FloatGrid::Accessor accessor = grid->getAccessor();
 
    // Define a coordinate with large signed indices.
    openvdb::Coord xyz(1000, -200000000, 30000000);
 
    // Set the voxel value at (1000, -200000000, 30000000) to 1.
    accessor.setValue(xyz, 1.0);
 
    // Verify that the voxel value at (1000, -200000000, 30000000) is 1.
    std::cout << "Grid" << xyz << " = " << accessor.getValue(xyz) << std::endl;
 
    // Reset the coordinates to those of a different voxel.
    xyz.reset(1000, 200000000, -30000000);
 
    // Verify that the voxel value at (1000, 200000000, -30000000) is
    // the background value, 0.
    std::cout << "Grid" << xyz << " = " << accessor.getValue(xyz) << std::endl;
 
    // Set the voxel value at (1000, 200000000, -30000000) to 2.
    accessor.setValue(xyz, 2.0);
 
    // Set the voxels at the two extremes of the available coordinate space.
    // For 32-bit signed coordinates these are (-2147483648, -2147483648, -2147483648)
    // and (2147483647, 2147483647, 2147483647).
    accessor.setValue(openvdb::Coord::min(), 3.0f);
    accessor.setValue(openvdb::Coord::max(), 4.0f);
 
    std::cout << "Testing sequential access:" << std::endl;
 
    // Print all active ("on") voxels by means of an iterator.
    for (openvdb::FloatGrid::ValueOnCIter iter = grid->cbeginValueOn(); iter; ++iter) {
        std::cout << "Grid" << iter.getCoord() << " = " << *iter << std::endl;
    }
    REQUIRE(true);
}


TEST_CASE("test nano vdb works"){
    try {
        // Create an OpenVDB grid (here a level set surface but replace this with your own code)
        auto srcGrid = openvdb::tools::createLevelSetSphere<openvdb::FloatGrid>(100.0f, openvdb::Vec3f(0.0f), 1.0f);
 
        // Convert the OpenVDB grid, srcGrid, into a NanoVDB grid handle.
        auto handle = nanovdb::tools::createNanoGrid(*srcGrid);
 
        // Define a (raw) pointer to the NanoVDB grid on the host. Note we match the value type of the srcGrid!
        auto* dstGrid = handle.grid<float>();
 
        if (!dstGrid)
            throw std::runtime_error("GridHandle does not contain a grid with value type float");
 
        // Get accessors for the two grids. Note that accessors only accelerate repeated access!
        auto dstAcc = dstGrid->getAccessor();
        auto srcAcc = srcGrid->getAccessor();
 
        // Access and print out a cross-section of the narrow-band level set from the two grids
        for (int i = 97; i < 104; ++i) {
            printf("(%3i,0,0) OpenVDB cpu: % -4.2f, NanoVDB cpu: % -4.2f\n", i, srcAcc.getValue(openvdb::Coord(i, 0, 0)), dstAcc.getValue(nanovdb::Coord(i, 0, 0)));
        }
 
        nanovdb::io::writeGrid("data/sphere.nvdb", handle); // Write the NanoVDB grid to file and throw if writing fails
    }
    catch (const std::exception& e) {
        std::cerr << "An exception occurred: \"" << e.what() << "\"" << std::endl;
    }
}


TEST_CASE("Test world space bounds to grid coords"){
    float radius = 1.5;
    float voxel_size = 0.1;
    float default_val = 100.0;
    auto grid = openvdb::FloatGrid::create(default_val);
    grid->setGridClass(openvdb::GridClass::GRID_FOG_VOLUME);
    grid->setTransform(openvdb::math::Transform::createLinearTransform(voxel_size));

    // Bounds should completely encompas the llc/urc
    Vector urc = Vector(1, 1, 1) * radius;
    Vector llc = -urc;
    openvdb::CoordBBox bounds = world_space_to_bounds(llc, urc, grid);


    int width = (int)((std::ceil((urc - llc).X() / voxel_size)));
    int height = (int)((std::ceil((urc - llc).Y() / voxel_size)));
    int depth = (int)((std::ceil((urc - llc).Z() / voxel_size)));
    width = width % 2 == 0 ? width : width + 1;
    height = height % 2 == 0 ? height : height + 1;
    depth = depth % 2 == 0 ? depth : depth + 1;

    auto expected_bounds = openvdb::CoordBBox(
        openvdb::Coord(-width / 2, -height / 2, -depth / 2),
        openvdb::Coord(width / 2, height / 2, depth / 2)
    );
    printf("bounds llc = %d, %d urc = %d, %d", -width / 2, -depth / 2, width / 2, depth / 2);

    REQUIRE(expected_bounds.getStart() == bounds.getStart());
    REQUIRE(expected_bounds.getEnd() == bounds.getEnd());
}

TEST_CASE("Test stamping IF into a grid"){
    float radius = 2.05;
    Vector center;
    auto sphere = isf_sphere(center, radius);

    // Now create the grid
    openvdb::initialize();

    float voxel_size = 0.1;
    float default_val = 100.0;
    auto grid = openvdb::FloatGrid::create(default_val);
    grid->setGridClass(openvdb::GridClass::GRID_FOG_VOLUME);
    grid->setTransform(openvdb::math::Transform::createLinearTransform(voxel_size));

    // Bounds should completely encompas the llc/urc
    Vector urc = Vector(1, 1, 1) * radius;
    Vector llc = -urc;
    openvdb::CoordBBox bounds = world_space_to_bounds(llc, urc, grid);


    int width = (int)((std::ceil((urc - llc).X() / voxel_size)));
    int height = (int)((std::ceil((urc - llc).Y() / voxel_size)));
    int depth = (int)((std::ceil((urc - llc).Z() / voxel_size)));
    width = width % 2 == 0 ? width : width + 1;
    height = height % 2 == 0 ? height : height + 1;
    depth = depth % 2 == 0 ? depth : depth + 1;

    // auto expected_bounds = openvdb::CoordBBox(
    //     openvdb::Coord(-width / 2, -height / 2, -depth / 2),
    //     openvdb::Coord(width / 2, height / 2, depth / 2)
    // );

    // REQUIRE(expected_bounds.getStart() == bounds.getStart());
    // REQUIRE(expected_bounds.getEnd() == bounds.getEnd());

    
    
    // stamp into level set
    stamp_grid<float>(grid, [sphere](const Vector& p){return sphere->eval(p); }, [](float val){return val < 0;}, bounds);
    
    
    // evaluate level set at some points
    openvdb::FloatGrid::Accessor accessor = grid->getAccessor();

    // The left most point on the x axis shouldn't have a value due to padding, but one beside it should
    REQUIRE(accessor.getValue(openvdb::Coord(-width / 2 - 1, 0, 0)) == default_val);
    REQUIRE(accessor.getValue(openvdb::Coord(-width / 2 + 1, 0, 0)) <= 0);

    REQUIRE(accessor.getValue(openvdb::Coord(0, 0, 0)) == -radius);
}

TEST_CASE("Test trilinear interpolation in eval"){
    // Create a GridField
    openvdb::initialize();

    float voxel_size = 1.0;
    float default_val = 0.0;
    auto grid = openvdb::FloatGrid::create(default_val);
    grid->setGridClass(openvdb::GridClass::GRID_FOG_VOLUME);
    grid->setTransform(openvdb::math::Transform::createLinearTransform(voxel_size));

    auto grid_field = make_grid_field<float>(grid);
    auto ac = grid->getAccessor();
    ac.setValue(ocoord(0, 1, 0), 10.0);
    ac.setValue(ocoord(1, 1, 0), 10.0);
    ac.setValue(ocoord(0, 1, 1), 10.0);
    ac.setValue(ocoord(1, 1, 1), 10.0);

    REQUIRE(grid_field->eval(Vector(0.5, 0.5, 0.5)) == 5.0);
}


TEST_CASE("Test create level set from obj"){
    openvdb::initialize();

    std::string obj_filepath = std::string(PBVFX_SOURCE_DIR) + "/third_party/starter/models/ajax/smallajax.obj";

    ObjReader<openvdb::Vec3s> objreader(obj_filepath);
    const auto& verts = objreader.get_verts();
    const auto& faces = objreader.get_faces();
    
    float voxelsize = 1.0f;
    float hw = 3.0f; // narrow band halfwidth in voxels

    auto grid = obj_mesh_to_level_set_f(obj_filepath, voxelsize, hw);
    openvdb::Vec3d large_val(99999999, 999999999, 999999999);

    auto sample_grid = [&grid](const openvdb::Vec3d& p){
        return openvdb::tools::BoxSampler::sample(
            grid->tree(), grid->worldToIndex(p)
        );
    };

    REQUIRE(sample_grid(large_val) == hw * voxelsize);
    REQUIRE(std::abs(sample_grid(verts[0])) < hw * voxelsize);
    REQUIRE(sample_grid(grid->worldToIndex({0, 0, 0})) > -hw * voxelsize);

}

TEST_CASE("Test boundbox iterator"){
    coordbbox bbox(ocoord(0, 0, 0), ocoord(3, 3, 3));
    auto iter = bbox.beginXYZ();
    while (iter != bbox.endXYZ()){
        const auto& coord = *iter; 
        std::cout << coord.x() << ", "
              << coord.y() << ", "
              << coord.z() << '\n';
        ++iter;
    }
    REQUIRE(true);
}

TEST_CASE("Test write density field to mesh"){
    auto isf = isf_icosahedron({});
    isf = scale_fixed(isf, {0.1, 0.1, 0.1});

    std::string outpath = std::string(PBVFX_SOURCE_DIR) + "/images/test_openvdb_write_to_geo_02.obj";

    float voxel_size = 0.01;
    float default_val = 999;
    auto bounds = world_space_to_bounds(Vector(-1, -1, -1), Vector(1, 1, 1), voxel_size);
    auto grid = stamp_isf_to_grid(voxel_size, default_val, isf, bounds);
    REQUIRE(grid->activeVoxelCount() > 0);

    std::vector<openvdb::Vec3s> points;
    std::vector<openvdb::Vec3I> triangles;
    std::vector<openvdb::Vec4I> quads;

    openvdb::tools::volumeToMesh(*grid, points, triangles, quads);

    std::ofstream output(outpath);

    for (const auto& point : points) {
        output << "v "
            << point.x() << " "
            << point.y() << " "
            << point.z() << "\n";
    }

    for (const auto& triangle : triangles) {
        output << "f "
            << triangle[0] + 1 << " "
            << triangle[1] + 1 << " "
            << triangle[2] + 1 << "\n";
    }

    for (const auto& quad : quads) {
        output << "f "
            << quad[0] + 1 << " "
            << quad[1] + 1 << " "
            << quad[2] + 1 << " "
            << quad[3] + 1 << "\n";
    }

    REQUIRE(points.size() > 0);
    REQUIRE(quads.size() > 0);

}


} // end namespace lux