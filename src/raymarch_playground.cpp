#include "raymarch_playground.h"
#include "openvdb_helpers.h"


#include "string_funcs.h"

#include <openvdb/openvdb.h>
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/VolumeToMesh.h>

#include "obj_reader.h"

namespace lux{

#define RADTODEG(radians) ((radians) * 180.0f / 3.14159265358979)
#define DEGTORAD(degrees) ((degrees * 3.14159265358979 / 180.0))

using vecsptr = std::shared_ptr<const std::vector<vspf>>;




void raymarch_things(const std::string& filename, float t){
    float cam_distance = 10;
    //float scene_width = 5;
    //float near = cam_distance - scene_width / 2.0;
    //float far = near + scene_width;
    float near = 5;
    float far = 15;
    RayMarcher rm;
    float min_ds = (far - near) / 100000;
    float max_ds = min_ds * 3.65;
    rm.set_min_ds(min_ds);
    rm.set_max_ds(max_ds);
    rm.set_snear(near);
    rm.set_sfar(far);
    float kappa = 3.0;
    rm.set_exticntion_coefficient(kappa);
    rm.set_Tmin(0.001);
    Camera cam;

    // float radius = 1.5;
    // //auto sphere = isf_sphere(Vector(), radius);
    // auto torus = isf_torus(Vector(), radius, radius / 3.0, Vector(0, 0, -1));
    // auto torus_b = isf_torus(Vector(), radius, radius / 3.0, Vector(0, 1, 0));
    // auto torus_c = isf_torus(Vector(), radius, radius / 3.0, Vector(1, 0, 0));


    // auto col = make_constant(Color(0, 0, 0, 0));
    // auto torus_col = make_constant(comfy_colors::green_mountain);
    
    
    // float voxel_size = 0.1;
    // float default_val = voxel_size;
    // auto grid = openvdb::FloatGrid::create(default_val);
    // grid->setGridClass(openvdb::GridClass::GRID_FOG_VOLUME);
    // grid->setTransform(openvdb::math::Transform::createLinearTransform(voxel_size));
    // auto bounds = world_space_to_bounds(Vector(-radius, -radius, -radius) * 1.5, Vector(radius, radius, radius) * 1.5, grid);
    // printf("bounds llc = %d, %d, %d", bounds.getStart().x(), bounds.getStart().y(), bounds.getStart().z());
    
    // stamp_grid<float>(grid, [torus](const Vector& p){return torus->eval(p); }, [](float val){ return val < 0; }, bounds);
    // torus = make_grid_field<float>(grid);
    
    
    // // auto col_grid = openvdb::Vec3SGrid::create();
    // // col_grid->setGridClass(openvdb::GridClass::GRID_FOG_VOLUME);
    // // col_grid->setTransform(openvdb::math::Transform::createLinearTransform(voxel_size));
    // // stamp_grid<Color>(col_grid, [col](const Vector& p){ return col->eval(p); }, [](const Color& val){return true; }, bounds);
    
    // //col = make_grid_field<Color>(col_grid);

    // auto gb = openvdb::FloatGrid::create(default_val);
    // gb->setGridClass(openvdb::GridClass::GRID_FOG_VOLUME);
    // gb->setTransform(openvdb::math::Transform::createLinearTransform(voxel_size));
    // stamp_grid<float>(gb, [torus_b](const Vector& p){return torus_b->eval(p); }, [](float val){ return val < 0; }, bounds);
    // torus_b = make_grid_field<float>(gb);
    // col = torus_col * mask(torus) + col * mask(-torus);
    // auto col_b = make_constant(comfy_colors::purpple_eastside);
    // col = col_b * mask(torus_b) + col * mask(-torus_b);
    // torus = union_fields(torus, torus_b);
    
    
    // //torus = make_grid_field<float>(torus);

    // torus = union_fields(torus, torus_c);
    // auto col_c = make_constant(comfy_colors::rose);
    // col = col_c * mask(torus_c) + col * mask(-torus_c);



    std::string obj_filepath = "/home/left/programming/DPA_PhysicsBasedVFX/third_party/starter/models/bunny_fixed.obj";
    ObjReader<Vector> reader(obj_filepath);
    printf("n tris = %g", (float)reader.get_faces().size());
    std::cout << obj_filepath<< std::endl;
    float voxel_size = 0.001;
    float half_width = 3;
    //auto bunny = obj_mesh_to_grid_field(obj_filepath, voxel_size, half_width);
    auto bunny2 = obj_mesh_to_level_set_f(obj_filepath, voxel_size, half_width);
    bunny2->transform().postScale(30);
    auto bunny_bounds = bunny2->evalActiveVoxelBoundingBox();
    auto bbs = bunny_bounds.getStart();
    auto bbe = bunny_bounds.getEnd();
    printf("bunny_bounds llc %i %i %i urc %i %i %i\n", bbs.x(), bbs.y(), bbs.z(), bbe.x(), bbs.y(), bbs.z());
    auto bunny = make_grid_field<float>(bunny2);

    // Now let's try making a mesh from the bunny, then passing that into the raymarcher
    //float voxel_size = 0.01;
    float default_val = 999;
    float vs = 0.25;
    auto mesh_bounds = world_space_to_bounds(Vector(-3, -3, -3), Vector(3, 3, 3), vs);
    auto grid = stamp_isf_to_grid(vs, default_val, dilation(bunny, 0.25 / 4.0), mesh_bounds);

    std::vector<openvdb::Vec3s> points;
    std::vector<openvdb::Vec3I> triangles;
    std::vector<openvdb::Vec4I> quads;

    openvdb::tools::volumeToMesh(*grid, points, triangles, quads);

    auto transform = openvdb::math::Transform::createLinearTransform(0.1);
    float hw = 3;
    printf("Making level set\n");
    auto levelset = openvdb::tools::meshToLevelSet<openvdb::FloatGrid>(*transform, points, triangles, quads, hw);
    printf("Level set made\n");
    rm.add_levelset(levelset);


    printf("n mesh points = %zu \n", points.size());
    std::vector<Vector> rt_points;
    rt_points.resize(points.size());
    std::transform(points.begin(), points.end(), rt_points.begin(), [](const auto& p){ return Vector(p); });
    
    //bunny = scale_fixed(bunny, {20, 20, 20});

    //bunny = bunny * make_constant(100.0f);
    bunny = -mask(bunny);

    // auto sphere = isf_sphere({}, 2);
    // auto sphere_bounds = world_space_to_bounds({-3, -3, -3}, {3, 3, 3}, 0.01);
    // auto grid_sphere = stamp_isf_to_grid(sphere, sphere_bounds, 0.01, 0.0);
    // //sphere = sphere * make_constant(3.0f);
    auto const_col = make_constant(Color(1, 1, 1, 0));


    // TO DO -> better to add colors to the rm and have it create the shadow map (maybe)
    PointLight key(Color(1.0, 0.00, 0.00, 0), Vector(0, 3, 3));
    PointLight fill(Color(0.00, 1.0, 0.00, 0), Vector(0, -3, 0));
    PointLight rim(Color(0.00, 0.00, 1.0, 0), Vector(0, 0, -3));

    float sm_voxelsize = bunny2->transform().voxelSize().x();
    printf("shadow map voxelsize = %g\n", sm_voxelsize);
    float sm_kappa = 0.5;
    float sm_stepsize = 0.01;
    auto bounds = world_space_to_bounds({-3, -3, -3}, {3, 3, 3}, sm_voxelsize);
    bounds = world_space_to_bounds(Vector(bunny2->indexToWorld(bunny_bounds.getStart())), Vector(bunny2->indexToWorld(bunny_bounds.getEnd())), sm_voxelsize);
    
    
    auto shaddow_map = make_deep_shadow_map_parallel(bunny, sm_voxelsize, bounds, key, sm_stepsize, sm_kappa);
    auto sm2 = make_deep_shadow_map(bunny, sm_voxelsize, bounds, fill, sm_stepsize, sm_kappa);
    auto sm3 = make_deep_shadow_map(bunny, sm_voxelsize, bounds, rim, sm_stepsize, sm_kappa);
    rm.add_shadow_map(shaddow_map, key.color);
    rm.add_shadow_map(sm2, fill.color);
    rm.add_shadow_map(sm3, rim.color);
    


    rm.add_shadow_map(make_constant<float>(1.0f), Color(1, 0, 0, 0));
    ImageData render_img(1920 / 4, 1080 / 4, 4);
    Vector eye = Vector(0, 0, cam_distance);
    Vector view = Vector(0, 0, -1);
    eye = rotation(eye, Vector(0, 1, 0), DEGTORAD(360.0 / 1 * 0));
    view = rotation(view, Vector(0, 1, 0), DEGTORAD(360.0 / 1 * 0));
    cam.setEyeViewUp(eye, view, Vector(0, 1, 0));
    auto start_time = std::chrono::high_resolution_clock::now();
    //rm.ray_march_image(cam, render_img, bunny, const_col, triangles, quads, rt_points);
    rm.ray_march_image(cam, render_img, bunny, const_col);
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    std::cout << "Raymarching took " << elapsed.count() << " seconds\n";
    std::string frame_filename = filename;
    render_img.oiio_write_to(frame_filename);
    std::cout << "density at origin: "
          << bunny->eval(Vector(0, 0.0, 0))
          << '\n';
}

}
//} // end namespace lux