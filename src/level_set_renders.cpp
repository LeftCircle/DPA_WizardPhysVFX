#include "level_set_renders.h"
#include "AABB.h"

namespace lux{

RenderInfo bunny_info(){
    float voxel_size = 0.001;
    float half_width = 10;
    std::string obj_filepath = "/home/left/programming/DPA_PhysicsBasedVFX/third_party/starter/models/bunny_fixed.obj";
    ObjReader<Vector> reader(obj_filepath);
    
    printf("n tris = %g", (float)reader.get_faces().size());
    std::cout << obj_filepath<< std::endl;
    
    auto bunny2 = obj_mesh_to_level_set_f(obj_filepath, voxel_size, half_width);
    bunny2->transform().postScale(27);
    auto bunny_bounds = bunny2->evalActiveVoxelBoundingBox();
    auto bbs = bunny_bounds.getStart();
    auto bbe = bunny_bounds.getEnd();
    printf("bunny_bounds llc %g %g %g urc %i %i %i\n", bbs.x() * voxel_size, bbs.y() * voxel_size, bbs.z() * voxel_size, bbe.x(), bbs.y(), bbs.z());
    auto bunny = make_grid_field<float>(bunny2);
    //bunny = clamp_fixed(bunny, -1, 1);
    bunny = bunny * make_constant(100.0f);
    return RenderInfo(bunny, -mask(bunny), make_constant(Color(1, 1, 1, 0)), 3.5, 1.0);
}

void render_bunny(const std::string& path, const std::string& name){
    auto ri = bunny_info();
    float color_vs = 0.15;
    auto col_bounds = world_space_to_bounds({-3, -3, -3}, {3, 3, 3}, color_vs);
    ri.color = make_grid_field<Color>(stamp_color_to_grid(ri.color, col_bounds, color_vs));
    render_turnable(path, name, ri.density, ri.masked_density, ri.color, ri.sm_kappa, ri.kappa);
}

RenderInfo bust_info(){
    float n_voxel_in_largest_dim = 2000;
    float half_width = 3;
    std::string obj_filepath = "/home/left/programming/DPA_PhysicsBasedVFX/third_party/starter/models/ajax/smallajax.obj";
    ObjReader<Vector> reader(obj_filepath);
    pba::AABB<Vector> aabb;
    auto verts = reader.get_verts();
    for (const auto& vert : verts){
        aabb.expand_to_include(vert);
    }
    auto diag = aabb.size();
    auto max_dim = std::max(std::max(diag.x(), diag.y()), diag.z());
    float voxel_size = max_dim / n_voxel_in_largest_dim;
    
    printf("n tris = %g", (float)reader.get_faces().size());
    std::cout << obj_filepath<< std::endl;
    
    auto model2 = obj_mesh_to_level_set_f(obj_filepath, voxel_size, half_width);
    model2->transform().postScale(0.11);
    
    
    //model2->transform().postTranslate({0, 0.5, 0});
    
    
    auto model_bounds = model2->evalActiveVoxelBoundingBox();
    auto bbs = model_bounds.getStart();
    auto bbe = model_bounds.getEnd();
    voxel_size = model2->voxelSize().x();
    printf("model_bounds llc %g %g %g urc %i %i %i\n", bbs.x() * voxel_size, bbs.y() * voxel_size, bbs.z() * voxel_size, bbe.x(), bbs.y(), bbs.z());
    openvdb::tools::foreach(model2->beginValueOn(), [](const openvdb::FloatGrid::ValueOnIter& iter){
        iter.setValue(std::clamp(iter.getValue(), -1.0f, 1.0f));
    });
    auto model = make_grid_field<float>(model2);
    return RenderInfo(model, -mask(model), make_constant(Color(1, 1, 1, 0)), 30.5, 1.5);
}

void render_bust(const std::string& path, const std::string& name){
   
    auto ri = bust_info();
    render_turnable(
        path,
        name,
        ri.density,
        ri.masked_density,
        ri.color,
        ri.sm_kappa,
        ri.kappa
    );
}

RenderInfo bunnybust_info(){
    auto bun = bunny_info();
    auto ajax = bust_info();
    auto plane = make_plane({}, {0, 1, 0});
    plane = rotate_fixed(plane, Vector(1, 0, 0), DEGTORAD(26.6));
    plane = rotate_fixed(plane, {0, 0, 1}, DEGTORAD(16.5));
    plane = translate_fixed(plane, {0, 0.78, 0});

    auto plane_bun = make_plane({}, {0, 1, 0});
    plane_bun = rotate_fixed(plane_bun, Vector(0, 0, 1), DEGTORAD(-27.3));
    plane_bun = translate_fixed(plane_bun, {0, 1.35, 0});
    //bun.density = cutout(bun.density, plane_bun);

    auto bun_sphere = isf_sphere({0.1, 2.6, 0.204}, 2.0f);
    
    bun.density = rotate_fixed(bun.density, {1, 0, 0}, DEGTORAD(34.2));
    bun.density = rotate_fixed(bun.density, {0, 1, 0}, DEGTORAD(0.8));
    bun.density = rotate_fixed(bun.density, {0, 0, 1}, DEGTORAD(48.4));
    bun.density = translate_fixed(bun.density, {0.06, 0.28, 0.41});
    bun.density = translate_fixed(bun.density, {0, 0, -0.2});
    
    bun.density = intersection(bun.density, bun_sphere);
    
    ajax.density = cutout(ajax.density, -plane);
    ajax.density = ajax.density * make_constant(2.25f);
    bun.density = cutout(bun.density, plane);

    auto comined = std::make_shared<const std::vector<vspf>>(std::initializer_list<vspf>{-ajax.density, -bun.density});
    auto ajaxbun = -blinn_blend(comined, 0.3, 1.0);
    ajaxbun = ajaxbun * make_constant(4.5f);
    auto vs = 0.005;
    auto bounds = world_space_to_bounds({-3, -3, -3}, {3, 3, 3}, vs);
    ajaxbun = make_grid_field<float>(stamp_isf_to_grid(vs, 0.0001f, ajaxbun, bounds));

    return RenderInfo(
        ajaxbun,
        -mask(ajaxbun),
        make_constant(Color(1, 1, 1, 0)),
        4.0, 
        1.0
    );
}

void render_bunny_in_ajax(const std::string& path, const std::string& name){
    auto ba = bunnybust_info();

    render_turnable(
        path,
        name,
        ba.density,
        ba.masked_density,
        ba.color,
        4.0,
        1.0
    );
}

void render_humanoid_with_bunnybust(const std::string& path, const std::string& name){
    auto [humanoid, cf] = combine_human_and_staff(0);
    float limit = 4.1;
    float density_vs = 0.004;
    float color_vs = 0.0075;
    //float density_vs = limit * 2.0 / 500.0;
    //float color_vs = limit * 2 / 300.0;
    printf("Stamping\n");
    //printf("Grid evals = %i", (int)std::powf(limit * 2.0, 3));


    auto ba = bunnybust_info();
    ba.density = scale_fixed(ba.density, Vector(0.25, 0.25, 0.25));
    ba.density = translate_fixed(ba.density, Vector(1, -0.8, 1));

    cf = make_constant(Color(1, 1, 1, 0)) * mask(ba.density) + cf * mask(-ba.density);
    //cf = make_constant(Color(1, 1, 1, 0)) * mask(ba.density) + cf * mask(humanoid);
    humanoid = union_fields(humanoid, ba.density);
    
    //humanoid = humanoid * make_constant(0.5f);

    auto dens_bounds = world_space_to_bounds({-limit, -limit, -limit}, {limit, limit, limit}, density_vs);
    auto col_bounds = world_space_to_bounds({-limit, -limit, -limit}, {limit, limit, limit}, color_vs);
    
    auto start_bake = std::chrono::high_resolution_clock::now();
    humanoid = make_grid_field<float>(
        stamp_isf_to_grid(density_vs, 0.0, humanoid, dens_bounds, "images/humanoid_dens_stamp.vdb")
    );
    auto end_bake = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_bake = end_bake - start_bake;
    std::cout << "Baking took " << elapsed_bake.count() << " seconds\n";
    
    auto start_bake_color = std::chrono::high_resolution_clock::now();
    cf = make_grid_field<Color>(stamp_color_to_grid(cf, col_bounds, color_vs, "images/humanoid_col_stamp.vdb"));
    auto end_bake_color = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_bake_color = end_bake_color - start_bake_color;
    std::cout << "Baking color took " << elapsed_bake_color.count() << " seconds\n";
    //humanoid = clamp_fixed(humanoid, -1.0, 1.0);
     
    render_turnable(
        path,
        name,
        humanoid,
        -mask(humanoid),
        cf,
        60.0,
        2.0
    );
}

void render_turnable(
    const std::string& path,
    const std::string& name,
    const vspf& density, 
    const vspf& masked_density,
    const vspc& color,
    float sm_kappa,
    float kappa,
    float t
){
    // RENDER SETTINGS
    int n_images = 120;
    float cam_distance = 10;
    float near = 6;
    float far = 15;
    int width = 1920;
    int height = 1080;
    
    float min_ds = 0.00005;
    //float min_ds = (far - near) / 750;
    float max_ds = min_ds * 6;
    //float kappa = 1.0;
    
    // RENDER OPTIMIZATION SETTINGS
    //float mesh_grid_voxelsize = 0.01;
    float model_halfwidth = 4;
    float mesh_grid_voxelsize = 0.005;//model_halfwidth * 2 / 750; // was 750 for humanoid
    float sdf_voxel_size = 0.01;
    float sdf_half_width = 5;

    // SHADOW MAP SETTINGS
    float shadow_map_voxel_size = 0.0035;
    //float shadow_map_voxel_size = 4.1 * 2 / 300;
    //float shadow_map_voxel_size = 4.1 * 2 / 100;
    
    std:: cout << "sm voxel size = " << shadow_map_voxel_size << std::endl;
    //float sm_kappa = 3.5;
    float sm_stepsize = 0.0005;

    RayMarcher rm;
    
    rm.set_min_ds(min_ds);
    rm.set_max_ds(max_ds);
    rm.set_snear(near);
    rm.set_sfar(far);
    rm.set_exticntion_coefficient(kappa);
    rm.set_Tmin(0.001);
    Camera cam;


    // ------------------------------------------------------------------
    // Adding the level set to the raymarcher
    // ------------------------------------------------------------------
    float default_val = 999;
    std::cout << "stamping" << std::endl;

    float mhw = model_halfwidth;
    auto model_bounds = world_space_to_bounds(Vector(-mhw, -mhw, -mhw), Vector(mhw, mhw, mhw), mesh_grid_voxelsize);
    //stamp_isf_to_grid(grid, dilation(density, mesh_grid_voxelsize / 2.0), model_bounds);
    auto grid = stamp_isf_to_grid(mesh_grid_voxelsize, default_val, density, model_bounds);


    std::vector<openvdb::Vec3s> points;
    std::vector<openvdb::Vec3I> triangles;
    std::vector<openvdb::Vec4I> quads;

    openvdb::tools::volumeToMesh(*grid, points, triangles, quads);

    std::ofstream output(path + name + ".obj");

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

    std::cout << "obj written" << std::endl;

    auto transform = openvdb::math::Transform::createLinearTransform(sdf_voxel_size);
    float hw = sdf_half_width;
    printf("Making level set\n");
    auto levelset = openvdb::tools::meshToLevelSet<openvdb::FloatGrid>(*transform, points, triangles, quads, hw);
    auto levelset_bounds = levelset->evalActiveVoxelBoundingBox();
    printf("Level set made\n");
    rm.add_levelset(levelset);

    
    // ------------------------------------------------------------------
    // creating lights
    // ------------------------------------------------------------------

    auto const_col = make_constant(Color(1, 1, 1, 0));


    // TO DO -> better to add colors to the rm and have it create the shadow map (maybe)
    
    float sm_voxelsize = shadow_map_voxel_size;
    
    //auto bounds = world_space_to_bounds({-mhw, -mhw, -mhw}, {mhw, mhw, mhw}, sm_voxelsize);
    auto bounds = world_space_to_bounds(
        Vector(levelset->indexToWorld(levelset_bounds.getStart())),
        Vector(levelset->indexToWorld(levelset_bounds.getEnd())),
        sm_voxelsize
    );
    bounds.expand(3);
    auto diag = bounds.getStart() - bounds.getEnd();
    auto max_xy = std::max(std::abs(diag.x()), std::abs(diag.y()));
    auto max = std::max(max_xy, std::abs(diag.z()));
    float bounds_hw = (float)max / 2.0f * sm_voxelsize;
    std::cout << "bounds hw " << bounds_hw << " hw = " << max << std::endl;   

    
    // ------------------------------------------------------------------
    // Rendering the image
    // ------------------------------------------------------------------
    
    float fps = 24;
    float dt = 1.0 / fps;
    for (int i = 0; i < n_images; i++){

        // ------------------------------------------------------------------
        // RShadow maps
        // ------------------------------------------------------------------
        rm.clear_shadow_maps();
        PointLight key(Color(1.0, 0.5, 0.5, 0), rotation(Vector(0, bounds_hw, bounds_hw), Vector(0, 1, 0), DEGTORAD(360.0 / n_images * i)));
        PointLight fill(Color(0.5, 1.0, 0.5, 0), rotation(Vector(0, -bounds_hw, 0), Vector(0, 1, 0), DEGTORAD(360.0 / n_images * i)));
        PointLight rim(Color(0.5, 0.5, 1.0, 0), rotation(Vector(0, 0, -bounds_hw), Vector(0, 1, 0), DEGTORAD(360.0 / n_images * i)));
        
        auto start_bake = std::chrono::high_resolution_clock::now();

        auto shaddow_map = make_deep_shadow_map_parallel(density, sm_voxelsize, bounds, key, sm_stepsize, sm_kappa);
        auto sm2 = make_deep_shadow_map(density, sm_voxelsize, bounds, fill, sm_stepsize, sm_kappa * 0.2);
        auto sm3 = make_deep_shadow_map(density, sm_voxelsize, bounds, rim, sm_stepsize, sm_kappa * 0.4);
        rm.add_shadow_map(shaddow_map, key.color);
        rm.add_shadow_map(sm2, fill.color);
        rm.add_shadow_map(sm3, rim.color);
        auto end_bake = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_bake = end_bake - start_bake;
        std::cout << "Baking took " << elapsed_bake.count() << " seconds\n";

        // A constant shadow map for testing if needed
        // rm.add_shadow_map(make_constant<float>(1.0f), Color(1, 1, 1, 0));
    
        // ------------------------------------------------------------------
        // Render maps
        // ------------------------------------------------------------------
        
        ImageData render_img(width, height, 4);
        Vector eye = Vector(0, 0, cam_distance);
        Vector view = Vector(0, 0, -1);
        eye = rotation(eye, Vector(0, 1, 0), DEGTORAD(360.0 / n_images * i));
        view = rotation(view, Vector(0, 1, 0), DEGTORAD(360.0 / n_images * i));
        cam.setEyeViewUp(eye, view, Vector(0, 1, 0));
        
        
        auto start_time = std::chrono::high_resolution_clock::now();
        rm.ray_march_image(cam, render_img, density, color);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end_time - start_time;
        std::cout << "Raymarching took " << elapsed.count() << " seconds\n";
        std::cout << "Total " << elapsed.count() + elapsed_bake.count() << " seconds\n";
        
        
        std::string frame_filename = path + name + "." + StringFuncs::get_zero_padded_number_string(i, 4) + ".exr";
        render_img.oiio_write_to(frame_filename);
        std::cout << "Wrote to " << frame_filename << std::endl;
        t += dt;
    }
}

void cpu_render_frame(
    const RenderSettings& rs,
    const GridPtr<float> dens_grid,
    const GridPtr<Color> col_grid,
    const std::vector<GridPtr<float>>& shadow_grids,
    const std::vector<PointLight>& lights,
    const Camera& cam,
    const std::string& filename,
    const std::string& label
){
    if (shadow_grids.size() != lights.size()) {
        throw std::invalid_argument(
            "Expected one shadow grid per light");
    }

    RayMarcher rm;
    rm.set_min_ds(rs.min_step);
    rm.set_max_ds(rs.max_step);
    rm.set_snear(rs.near_dist);
    rm.set_sfar(rs.far_dist);
    rm.set_exticntion_coefficient(rs.kappa);
    rm.set_Tmin(0.001f); // Match the value used in render_turnable.

    auto density_field = make_grid_field<float>(dens_grid);
    auto color_field = make_grid_field<Color>(col_grid);


    // ------------------------------------------------------------------
    // Adding the level set to the raymarcher
    // ------------------------------------------------------------------
    float default_val = 999;
    std::cout << "stamping" << std::endl;

    std::vector<openvdb::Vec3s> points;
    std::vector<openvdb::Vec3I> triangles;
    std::vector<openvdb::Vec4I> quads;

    openvdb::tools::volumeToMesh(*dens_grid, points, triangles, quads);

    // std::ofstream output(path + name + ".obj");

    // for (const auto& point : points) {
    //     output << "v "
    //         << point.x() << " "
    //         << point.y() << " "
    //         << point.z() << "\n";
    // }

    // for (const auto& triangle : triangles) {
    //     output << "f "
    //         << triangle[0] + 1 << " "
    //         << triangle[1] + 1 << " "
    //         << triangle[2] + 1 << "\n";
    // }

    // for (const auto& quad : quads) {
    //     output << "f "
    //         << quad[0] + 1 << " "
    //         << quad[1] + 1 << " "
    //         << quad[2] + 1 << " "
    //         << quad[3] + 1 << "\n";
    // }

    // std::cout << "obj written" << std::endl;

    auto transform = openvdb::math::Transform::createLinearTransform(rs.get_density_vs());
    float hw = rs.model_halfsize;
    printf("Making level set\n");
    auto levelset = openvdb::tools::meshToLevelSet<openvdb::FloatGrid>(*transform, points, triangles, quads, hw);
    auto levelset_bounds = levelset->evalActiveVoxelBoundingBox();
    printf("Level set made\n");
    rm.add_levelset(levelset);



    // Keep the field wrappers alive for the duration of the render.
    std::vector<vspf> shadow_fields;
    shadow_fields.reserve(shadow_grids.size());

    for (std::size_t i = 0; i < shadow_grids.size(); ++i) {
        shadow_fields.push_back(make_deep_shadow_map(shadow_grids[i]));
        rm.add_shadow_map(shadow_fields.back(), lights[i].color);
    }

    ImageData image(rs.width, rs.height, 4);
    rm.ray_march_image(cam, image, density_field, color_field);
    image.oiio_write_to(filename, label);

    std::cout << "Wrote to " << filename << '\n';
}


} // end namespace lux