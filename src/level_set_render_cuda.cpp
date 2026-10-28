#include "level_set_render_cuda.h"
#include <nanovdb/tools/CreateNanoGrid.h>

#include <cuda_runtime.h>
#include <nanovdb/tools/CreateNanoGrid.h>
#include <nanovdb/cuda/DeviceBuffer.h>

#include <stdexcept>
#include <vector>
#include <cmath>
#include <functional>
#include <utility>

#include "profile_helpers.h"
#include "wisps.h"

namespace lux{

#define DEGTORAD(degrees) ((degrees * 3.14159265358979 / 180.0))

// -------------------------------------------------------------
// A cuda demo
// -------------------------------------------------------------

void profile_cuda(){
    RenderSettings rs;
    rs.shadow_step = 0.001;
    rs.set_shadow_vs(0.025);
    rs.min_step = 0.0005;
    rs.max_step = rs.min_step * 5;

    // Defining the density and color
    auto bunn = bunny_info();
    auto dens = bunn.density;
    auto dens_grid = stamp_isf_to_grid(rs.get_density_vs(), 0.00001f, dens, rs.get_density_bounds());
    auto col_grid = stamp_color_to_grid(bunn.color, rs.get_color_bounds(), rs.get_color_vs());

    std::vector<PointLight> lights = {PointLight({1, 0, 0, 0}, {-3, 0, 0})};
    cudaStream_t stream{};
    CUDA_CHECK(cudaStreamCreate(&stream));

    // cpu shadow map
    auto sm_grid = make_dsm_grid(dens, rs.get_shadow_vs(), rs.get_shadow_bounds(), lights[0], rs.shadow_step, lights[0].kappa);
    std::vector<GridPtr<float>> dsms = {sm_grid};
    Camera cam;
    Vector eye = Vector(0, 0, 10);
    Vector view = Vector(0, 0, -1);
    cam.setEyeViewUp(eye, view, Vector(0, 1, 0));

    time_func("cuda render", [&] {
        cuda_render_frame_with_dsm(
            rs, dens_grid, col_grid, dsms, lights, cam, "");
    });

    time_func("cpu render", [&] {
        cpu_render_frame(
            rs, dens_grid, col_grid, dsms, lights, cam, "");
    });

    // time_func("cuda render", cuda_render_frame_with_dsm, rs, dens_grid, col_grid, dsms, lights, cam, "");
    // time_func("cpu render", cpu_render_frame, rs, dens_grid, col_grid, dsms, lights, cam, "");

}

void cuda_render_tests(const std::string& filename){
    // Just keep as default for now
    float radius = 1.75;
    RenderSettings rs;
    rs.shadow_step = 0.005;
    rs.model_halfsize = 1.75;
    rs.set_density_vs(rs.model_halfsize * 2.0 / 500);
    rs.set_shadow_vs(0.01);
    rs.shadow_kappa = 10;
    rs.kappa = 5;

    // Defining the density and color
    auto bunn = bunny_info();
    auto dens = bunn.density;

    //auto noise = make_fspn(0.5, 3, {}, 2, 1.5);
    // dens = isf_sphere({}, radius);
    //dens = make_pyro(dens, 3, 2, {}, 1, 0, 3.0, 0.3, 15);

    // Wisps
    size_t n_dots = 1e6;
    float dot_dens = -1.0;
    float clump = 1.0;
    float pscale = 1.0;
    FSVals fspn1(0.01, 2, {}, 0.1, 0);
    FSVals fspn2(0.01, 2, {}, 0.1, 0);
    Vector displacement = {1, 1, 1};
    Vector location = {};
    CoordinateSys axis;
    int id = 1;
    auto wisp_generator = wisp_uniform(
        n_dots,
        dot_dens,
        clump,
        pscale,
        fspn1,
        fspn2,
        displacement,
        location,
        axis,
        id,
        {1, 1, 1, 0},
        -4,
        4
    );
    float scale = 3.0;
    auto dens_grid = create_grid<float>(rs.get_density_vs(), 0.0001f);
    wisp_generator.stamp_wisp_dots<float>(dens_grid);


    //dens = dens + (make_fspn(3, 3, {}, 3, 1.5) * mask(dens));
    std::cout << "stamping" << std::endl;
    //GridPtr<float> dens_grid = time_func("dens stamping", stamp_isf_to_grid, rs.get_density_vs(), 0.00001f, dens, rs.get_density_bounds(), "");
    dens = make_grid_field<float>(dens_grid);
    //dens = dens * make_constant<float>(100);
    std::cout << "stamp_done" << std::endl;
    bunn.color = make_constant<Color>(Color(1, 1, 1.0, 0));
    auto col_grid = stamp_color_to_grid(make_constant<Color>({1, 1, 1, 0}), rs.get_color_bounds(), rs.get_color_vs());
    Camera cam;
    Vector eye = Vector(0, 0, 10);
    Vector view = Vector(0, 0, -1);
    cam.setEyeViewUp(eye, view, Vector(0, 1, 0));
    // PointLight key(Color(1.0, 0.001, 0.001, 0), {-1.1 * rs.model_halfsize, 0, 0}, rs.shadow_kappa);
    // PointLight rim(Color(0.001, 1.0, 0.001, 0), {1.1 * rs.model_halfsize, 0, 0}, rs.shadow_kappa * 0.4);
    // PointLight fill(Color(0.001, 0.001, 1.0, 0), {0, 0, -1.1 * rs.model_halfsize}, rs.shadow_kappa * 0.2);

    auto hw = rs.model_halfsize;
    auto angle = 0;
    PointLight key(Color(1.0, 0.001, 0.001, 0), rotation(Vector(0, 1.5 * hw, 1.5 * hw), Vector(0, 1, 0), angle), rs.shadow_kappa);
    PointLight fill(Color(0.001, 1.0, 0.001, 0), rotation(Vector(0, -1.5 * hw, -hw / 2.0), Vector(0, 1, 0), angle), rs.shadow_kappa * 0.4);
    PointLight rim(Color(0.001, 0.001, 1.0, 0), rotation(Vector(0, 0, -1.5 * hw), Vector(0, 1, 0), angle), rs.shadow_kappa * 0.1);

    std::vector<PointLight> lights = {key, rim, fill};
    auto key_grid = time_func("First shadow", make_dsm_grid, dens, rs.get_shadow_vs(), rs.get_shadow_bounds(), lights[0], rs.shadow_step, lights[0].kappa);
    auto rim_grid = make_dsm_grid(dens, rs.get_shadow_vs(), rs.get_shadow_bounds(), lights[1], rs.shadow_step, lights[1].kappa * 0.4);
    auto fill_grid = make_dsm_grid(dens, rs.get_shadow_vs(), rs.get_shadow_bounds(), lights[2], rs.shadow_step, lights[2].kappa * 0.2);
   

    std::vector<GridPtr<float>> dsms = {key_grid, rim_grid, fill_grid};

    cuda_render_frame_with_dsm(rs, dens_grid, col_grid, dsms, lights, cam, filename);
}

void render_wedges(
    std::string path,
    std::string name
){
    float radius = 2.0;
    int frames_per_param = 100;
    
    auto smoothstep = [](float x) -> float {
        x = std::clamp(x, 0.0f, 1.0f);
        return x * x * ( 3.0 - 2.0 * x);
    };
    auto lerp = [](float a, float b, float t){
        return a + t * (b - a);
    };
    auto log_lerp = [lerp](float a, float b, float t){
        return std::expf(lerp(std::log(a), std::log(b), t));
    };
    using InterpFunc = std::function<float(float, float, float)>;

    int rotation_frames = 48;
    
    int density_voxels_per_side = 550;
    int shadow_voxels_per_side = 300;
    
    // octave, fade, roughness, freq, fjump
    std::vector<float> mins = {1, 0.01, 0.00, 0.01, 1e-6};
    std::vector<float> maxes = {8, 4, 1, 5, 3};
    std::vector<float> defaults = {2.5, 1.5, 0.5, 2, 0};
    std::vector<InterpFunc> interps = {lerp, lerp, lerp, lerp, lerp};
    std::vector<std::string> names = {"octave", "fade", "roughness", "freq", "fjump"};
    
    int total_frames = frames_per_param * mins.size();
    RenderSettings rs;

    rs.near_dist = rs.cam_dist - radius * 1.95;
    rs.far_dist = rs.near_dist + rs.model_halfsize * 2.75;
    rs.shadow_kappa = 5.0;
    rs.model_halfsize = 2.1;
    rs.set_shadow_vs(2.0 * rs.model_halfsize / (float)shadow_voxels_per_side);
    rs.set_density_vs(2.0 * rs.model_halfsize / (float)density_voxels_per_side);
    rs.min_step = 0.0005;
    rs.shadow_step = 0.0025;
    const float dens_mult = 10.0;
    Camera cam;
    Vector eye = Vector(0, 0, 10);
    Vector view = Vector(0, 0, -1);
    cam.setEyeViewUp(eye, view, Vector(0, 1, 0));
    const auto hw = rs.model_halfsize;

    //for (int i = 0; i < mins.size(); i++){
    auto col_grid = stamp_color_to_grid(make_constant<Color>(Color(1, 1, 1, 0)), rs.get_color_bounds(), rs.get_color_vs());
    for (int frame = 0; frame < total_frames; frame++) {
        const size_t param = frame / frames_per_param; // 0..4
        const int frame_in_segment = frame % frames_per_param;
        const float t = (float)(frame_in_segment) / (float)(frames_per_param - 1);
        const float eased_t = smoothstep(t);

        auto values = defaults;
        values[param] = interps[param](mins[param], maxes[param], eased_t);
        for (const auto& name : names){
            std::cout << name << " ";
        }
        std::cout << std::endl;
        for (const auto& val : values){
            std::cout << val << " ";
        }
        std:: cout << std::endl;
        auto fractal = make_fspn(
            values[2],
            values[0],
            {},
            values[3],
            values[4] 
        );

        auto fade_field = make_fade_factor(
            {},
            values[1],
            radius
        );

        float angle = DEGTORAD(360.0 / rotation_frames * frame);
        PointLight key(Color(1.0, 0.001, 0.001, 0), rotation(Vector(0, 1.5 * hw, 1.5 * hw), Vector(0, 1, 0), angle), rs.shadow_kappa);
        PointLight fill(Color(0.001, 1.0, 0.001, 0), rotation(Vector(0, -1.5 * hw, 0), Vector(0, 1, 0), angle), rs.shadow_kappa * 0.4);
        PointLight rim(Color(0.001, 0.001, 1.0, 0), rotation(Vector(0, 0, -1.5 * hw), Vector(0, 1, 0), angle), rs.shadow_kappa * 0.1);
        std::vector<PointLight> lights = {key, fill, rim};
        

        Vector eye = Vector(0, 0, 10);
        Vector view = Vector(0, 0, -1);
        eye = rotation(eye, Vector(0, 1, 0), angle);
        view = rotation(view, Vector(0, 1, 0), angle);
        cam.setEyeViewUp(eye, view, Vector(0, 1, 0));

        auto density_field = fractal * fade_field * make_constant(dens_mult);
        auto dens_grid = stamp_isf_to_grid(rs.get_density_vs(), 0.00001f, density_field, rs.get_density_bounds());
        
        auto key_grid = make_dsm_grid(density_field, rs.get_shadow_vs(), rs.get_shadow_bounds(), lights[0], rs.shadow_step, lights[0].kappa);
        auto fill_grid = make_dsm_grid(density_field, rs.get_shadow_vs(), rs.get_shadow_bounds(), lights[1], rs.shadow_step, lights[1].kappa);
        auto rim_grid = make_dsm_grid(density_field, rs.get_shadow_vs(), rs.get_shadow_bounds(), lights[2], rs.shadow_step, lights[2].kappa);
        std::vector<GridPtr<float>> dsms = {key_grid, fill_grid, rim_grid};

        auto filename = path + name + "." + StringFuncs::get_zero_padded_number_string(frame, 4) + ".exr";
        std::ostringstream label;
        label << "octaves: " << values[0]
            << "  fade: " << values[1]
            << "  roughness: " << values[2]
            << "  freq: " << values[3]
            << "  fjump: " << values[4];
        cuda_render_frame_with_dsm(rs, dens_grid, col_grid, dsms, lights, cam, filename, label.str());   
    }
}

void render_pyro_wedges(
    std::string path,
    std::string name
){
    float radius = 2.0;
    int frames_per_param = 100;
    
    auto smoothstep = [](float x) -> float {
        x = std::clamp(x, 0.0f, 1.0f);
        return x * x * ( 3.0 - 2.0 * x);
    };
    auto lerp = [](float a, float b, float t){
        return a + t * (b - a);
    };
    auto log_lerp = [lerp](float a, float b, float t){
        return std::expf(lerp(std::log(a), std::log(b), t));
    };
    using InterpFunc = std::function<float(float, float, float)>;

    int rotation_frames = 72;
    
    int density_voxels_per_side = 600;
    int shadow_voxels_per_side = 400;
    
    // octave, gamma, freq, fjump
    std::vector<float> mins = {1, 0.01, 0.01, 1e-6, 0.01};
    std::vector<float> maxes = {8, 5, 5, 5, 5};
    std::vector<float> defaults = {3, 1, 2, 2, 2};
    std::vector<InterpFunc> interps = {lerp, lerp, lerp, lerp, lerp};
    std::vector<std::string> names = {"octave", "gamma", "freq", "fjump", "amp"};
    
    int total_frames = frames_per_param * mins.size();
    RenderSettings rs;

    rs.near_dist = rs.cam_dist - radius * 1.95;
    rs.far_dist = rs.near_dist + rs.model_halfsize * 2.75;
    rs.model_halfsize = 2.1;
    rs.set_shadow_vs(2.0 * rs.model_halfsize / (float)shadow_voxels_per_side);
    rs.set_density_vs(2.0 * rs.model_halfsize / (float)density_voxels_per_side);
    rs.min_step = 0.0001;
    rs.shadow_step = 0.001;
    rs.shadow_kappa = 5;
    //rs.kappa = 5;
    const float dens_mult = 10.0;
    Camera cam;
    Vector eye = Vector(0, 0, 10);
    Vector view = Vector(0, 0, -1);
    cam.setEyeViewUp(eye, view, Vector(0, 1, 0));
    const auto hw = rs.model_halfsize;

    // auto bunn = bunny_info();
    // auto dens = bunn.density;
    auto dens = isf_sphere({}, radius);

    Vector trans(0.1, 0, 0);
    auto vel = 0.01;
    //for (int i = 0; i < mins.size(); i++){
    auto col_grid = stamp_color_to_grid(make_constant<Color>(Color(1, 1, 1, 0)), rs.get_color_bounds(), rs.get_color_vs());
    for (int frame = 0; frame < total_frames; frame++) {
        const size_t param = frame / frames_per_param; // 0..4
        const int frame_in_segment = frame % frames_per_param;
        const float t = (float)(frame_in_segment) / (float)(frames_per_param - 1);
        const float eased_t = smoothstep(t);

        auto values = defaults;
        values[param] = interps[param](mins[param], maxes[param], eased_t);
        for (const auto& name : names){
            std::cout << name << " ";
        }
        std::cout << std::endl;
        for (const auto& val : values){
            std::cout << val << " ";
        }
        std:: cout << std::endl;
        auto pyro = make_pyro(
            dens,
            0.5,
            values[0],
            trans * vel * frame,
            values[2],
            values[3],
            values[4],
            values[1],
            1
        );
        GridPtr<float> dens_grid = time_func("dens stamping", stamp_isf_to_grid, rs.get_density_vs(), 0.00001f, pyro, rs.get_density_bounds(), "");
        auto stamped_dens = make_grid_field<float>(dens_grid);
        stamped_dens = stamped_dens * make_constant<float>(50);


        float angle = DEGTORAD(360.0 / rotation_frames * frame);
        PointLight key(Color(1.0, 0.001, 0.001, 0), rotation(Vector(0, 1.5 * hw, 1.5 * hw), Vector(0, 1, 0), angle), rs.shadow_kappa);
        PointLight fill(Color(0.001, 1.0, 0.001, 0), rotation(Vector(0, -1.5 * hw, 0), Vector(0, 1, 0), angle), rs.shadow_kappa * 0.4);
        PointLight rim(Color(0.001, 0.001, 1.0, 0), rotation(Vector(0, 0, -1.5 * hw), Vector(0, 1, 0), angle), rs.shadow_kappa * 0.1);
        std::vector<PointLight> lights = {key, fill, rim};
        

        Vector eye = Vector(0, 0, 10);
        Vector view = Vector(0, 0, -1);
        eye = rotation(eye, Vector(0, 1, 0), angle);
        view = rotation(view, Vector(0, 1, 0), angle);
        cam.setEyeViewUp(eye, view, Vector(0, 1, 0));

        auto key_grid = time_func("dsm", make_dsm_grid, stamped_dens, rs.get_shadow_vs(), rs.get_shadow_bounds(), lights[0], rs.shadow_step, lights[0].kappa);
        auto fill_grid = make_dsm_grid(stamped_dens, rs.get_shadow_vs(), rs.get_shadow_bounds(), lights[1], rs.shadow_step, lights[1].kappa);
        auto rim_grid = make_dsm_grid(stamped_dens, rs.get_shadow_vs(), rs.get_shadow_bounds(), lights[2], rs.shadow_step, lights[2].kappa);
        std::vector<GridPtr<float>> dsms = {key_grid, fill_grid, rim_grid};

        auto filename = path + name + "." + StringFuncs::get_zero_padded_number_string(frame, 4) + ".exr";
        std::ostringstream label;
        label << "octaves: " << values[0]
            << "  fade: " << values[1]
            << "  roughness: " << values[2]
            << "  freq: " << values[3]
            << "  fjump: " << values[4];
        cuda_render_frame_with_dsm(rs, dens_grid, col_grid, dsms, lights, cam, filename, label.str());   
    }
}

void render_wisp_wedges(
    std::string path,
    std::string name
){
    float radius = 2.0;
    int frames_per_param = 50;
    
    auto smoothstep = [](float x) -> float {
        x = std::clamp(x, 0.0f, 1.0f);
        return x * x * ( 3.0 - 2.0 * x);
    };
    auto lerp = [](float a, float b, float t){
        return a + t * (b - a);
    };
    auto log_lerp = [lerp](float a, float b, float t){
        return std::expf(lerp(std::log(a), std::log(b), t));
    };
    using InterpFunc = std::function<float(float, float, float)>;

    int rotation_frames = 96;
    
    int density_voxels_per_side = 750;
    int shadow_voxels_per_side = 350;

    
    // octave, gamma, freq, fjump
    std::vector<float> mins = {0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.1, 0.1};
    std::vector<float> maxes = {10, 10, 10, 10, 5, 5, 5, 5, 5, 1, 1};
    std::vector<float> defaults = {0.75, 1, 3, 5, 2, 1, 2, 1.5, 1, 0.1, 0.2};
    std::vector<InterpFunc> interps = {lerp, lerp, lerp, lerp, lerp, lerp, lerp, lerp, lerp, lerp, lerp};
    std::vector<std::string> names = {"clump", "pscale", "o1", "o2", "f1" "f2", "fj1", "fj2", "dips", "rough1", "rough2"};
    
    int total_frames = frames_per_param * mins.size();
    RenderSettings rs;
    rs.model_halfsize = 2.0;
    rs.cam_dist = 4.5;

    rs.near_dist = 0;//rs.cam_dist - rs.model_halfsize * 2;
    rs.far_dist = 10;//rs.near_dist + rs.model_halfsize * 2.75;
    rs.set_shadow_vs(2.0 * rs.model_halfsize / (float)shadow_voxels_per_side);
    rs.set_density_vs(2.0 * rs.model_halfsize / (float)density_voxels_per_side);
    rs.min_step = 0.0001;
    rs.max_step = 8.0 * rs.min_step;
    rs.shadow_step = 0.0002;
    rs.shadow_kappa = 5;
    //rs.kappa = 5;
    const float dens_mult = 10.0;
    Camera cam;
    Vector eye = Vector(0, 0, rs.cam_dist);
    Vector view = Vector(0, 0, -1);
    cam.setEyeViewUp(eye, view, Vector(0, 1, 0));
    const auto hw = rs.model_halfsize;

    // auto bunn = bunny_info();
    // auto dens = bunn.density;
    auto dens = isf_sphere({}, radius);

    Vector trans(0.1, 0, 0);
    auto vel = 0.01;
    //for (int i = 0; i < mins.size(); i++){
    auto col_grid = stamp_color_to_grid(make_constant<Color>(Color(1, 1, 1, 0)), rs.get_color_bounds(), rs.get_color_vs());
    for (int frame = 435; frame < total_frames; frame++) {
        const size_t param = frame / frames_per_param; // 0..4
        const int frame_in_segment = frame % frames_per_param;
        const float t = (float)(frame_in_segment) / (float)(frames_per_param - 1);
        const float eased_t = smoothstep(t);

        auto values = defaults;
        values[param] = interps[param](mins[param], maxes[param], eased_t);
        for (const auto& name : names){
            std::cout << name << " ";
        }
        std::cout << std::endl;
        for (const auto& val : values){
            std::cout << val << " ";
        }
        std:: cout << std::endl;
        
        // Wisps
        //std::vector<std::string> names = {"clump", "pscale", "o1", "o2", "f1" "f2", "fj1", "fj2", "dips"};
        size_t n_dots = 1e7 * 3;
        float dot_dens = -0.5;
        float clump = values[0];
        float pscale = values[1];
        FSVals fspn1(values[9], values[2], {}, values[4], values[6]);
        FSVals fspn2(values[10], values[3], {}, values[5], values[7]);
        float d = values[8];
        Vector displacement = {d, d, d};
        Vector location = {};
        CoordinateSys axis;
        int id = 1;
        auto wisp_generator = wisp_uniform(
            n_dots,
            dot_dens,
            clump,
            pscale,
            fspn1,
            fspn2,
            displacement,
            location,
            axis,
            id,
            {1, 1, 1, 0},
            -1,
            1
        );
        float scale = 3.0;
        auto dens_grid = create_grid<float>(rs.get_density_vs(), 0.0001f);
        wisp_generator.stamp_wisp_dots<float>(dens_grid);
        auto stamped_dens = make_grid_field<float>(dens_grid);
        //stamped_dens = stamped_dens * make_constant<float>(50);


        float angle = DEGTORAD(360.0 / rotation_frames * frame);
        PointLight key(Color(1.0, 0.001, 0.001, 0), rotation(Vector(0, 1.5 * hw, 1.5 * hw), Vector(0, 1, 0), angle), rs.shadow_kappa);
        PointLight fill(Color(0.001, 1.0, 0.001, 0), rotation(Vector(0, -1.5 * hw, 0), Vector(0, 1, 0), angle), rs.shadow_kappa * 0.4);
        PointLight rim(Color(0.001, 0.001, 1.0, 0), rotation(Vector(0, 0, -1.5 * hw), Vector(0, 1, 0), angle), rs.shadow_kappa * 0.1);
        std::vector<PointLight> lights = {key, fill, rim};
        

        Vector eye = Vector(0, 0, rs.cam_dist);
        Vector view = Vector(0, 0, -1);
        eye = rotation(eye, Vector(0, 1, 0), angle);
        view = rotation(view, Vector(0, 1, 0), angle);
        cam.setEyeViewUp(eye, view, Vector(0, 1, 0));

        auto key_grid = time_func("dsm", make_dsm_grid, stamped_dens, rs.get_shadow_vs(), rs.get_shadow_bounds(), lights[0], rs.shadow_step, lights[0].kappa);
        auto fill_grid = make_dsm_grid(stamped_dens, rs.get_shadow_vs(), rs.get_shadow_bounds(), lights[1], rs.shadow_step, lights[1].kappa);
        auto rim_grid = make_dsm_grid(stamped_dens, rs.get_shadow_vs(), rs.get_shadow_bounds(), lights[2], rs.shadow_step, lights[2].kappa);
        std::vector<GridPtr<float>> dsms = {key_grid, fill_grid, rim_grid};

        auto filename = path + name + "." + StringFuncs::get_zero_padded_number_string(frame, 4) + ".exr";
        std::ostringstream label;
        label << "octaves: " << values[0]
            << "  fade: " << values[1]
            << "  roughness: " << values[2]
            << "  freq: " << values[3]
            << "  fjump: " << values[4];
        time_func("render", cuda_render_frame_with_dsm, rs, dens_grid, col_grid, dsms, lights, cam, filename, label.str());   
    }
}

void cuda_render_frame(
    const RenderSettings& rs,
    const GridPtr<float> dens_grid,
    const GridPtr<Color> col_grid,
    const std::vector<PointLight>& lights,
    const Camera& cam,
    const std::string& filename,
    const std::string label
){
    cudaStream_t stream{};
    CUDA_CHECK(cudaStreamCreate(&stream));

    // Setting up some rng stuff
    const int pixelCount = rs.width * rs.height;
    RngState* device_rng_states = nullptr;
     CUDA_CHECK(create_rng_states(
        &device_rng_states,
        pixelCount,
        1234ULL,  // seed
        stream
    ));

    // density and color packing
    auto density_handle = pack_grid_to_nano(dens_grid, stream);
    auto color_handle = pack_grid_to_nano(col_grid, stream);

    auto device_shadow_array = create_shadow_maps_and_send_to_gpu(
        density_handle.deviceGrid<float>(),
        lights, rs, stream
    );
    
    std::vector<float4> host_light_colors;
    host_light_colors.reserve(lights.size());

    for (const auto& light : lights) {
        host_light_colors.push_back(make_float4(
            light.color.red(),
            light.color.green(),
            light.color.blue(),
            light.kappa
        ));
    }
    float4* device_light_colors = send_array_to_gpu(host_light_colors.data(), host_light_colors.size());

    // GPU volume for ray marching
    GPUVolume volume{};
    volume.density = density_handle.deviceGrid<float>();
    volume.color = color_handle.deviceGrid<nanovdb::Vec3f>();
    volume.shadow_maps = device_shadow_array;
    volume.shadow_map_count = lights.size();
    volume.light_colors = device_light_colors;

    // -----------------------------------------------
    // Getting the cam and image into the GPU
    // ----------------------------------------------------------------
    GPUCamera gpuCam(cam);

    std::vector<float4> pixels(rs.width * rs.height);
    float4* devicePixels = nullptr;

    CUDA_CHECK(cudaMalloc(
        reinterpret_cast<void**>(&devicePixels),
        pixels.size() * sizeof(float4))
    );

    // ----------------------------------------------------------------
    // The actual render
    // ----------------------------------------------------------------

    CUDA_CHECK(launch_cuda_raymarch(
        volume, devicePixels, rs.width, rs.height, gpuCam,
        rs.near_dist, rs.far_dist, rs.min_step, rs.max_step, rs.kappa, device_rng_states, stream));

    CUDA_CHECK(cudaStreamSynchronize(stream));
    CUDA_CHECK(cudaMemcpy(
        pixels.data(), devicePixels,
        pixels.size() * sizeof(float4),
        cudaMemcpyDeviceToHost)
    );

    // ----------------------------------------------------------------
    // Saving out the final image
    // ----------------------------------------------------------------

    ImageData img(rs.width, rs.height, 4);
    for (int i = 0; i < rs.width * rs.height; ++i) {
        const int x = i % rs.width;
        const int y = i / rs.width;
        ImageData::pixel p = {pixels[i].x, pixels[i].y, pixels[i].z, pixels[i].w};
        img.set_pixel_values(x, y, p);
    }
    //std::string frame_filename = path + name + "." + StringFuncs::get_zero_padded_number_string(0, 4) + ".exr";
    img.oiio_write_to(filename, label);
    std::cout << "Wrote to " << filename << std::endl;

    CUDA_CHECK(cudaFree(devicePixels));
    CUDA_CHECK(cudaFree(device_shadow_array));
    CUDA_CHECK(cudaFree(device_light_colors));
    CUDA_CHECK(cudaStreamDestroy(stream));
}


void cuda_render_frame_with_dsm(
    const RenderSettings& rs,
    const GridPtr<float> dens_grid,
    const GridPtr<Color> col_grid,
    const std::vector<GridPtr<float>>& shadow_grids,
    const std::vector<PointLight>& lights,
    const Camera& cam,
    const std::string& filename,
    const std::string label
){
    //log_cuda_memory("frame start");
    if (shadow_grids.size() != lights.size()) {
        throw std::invalid_argument("Expected one shadow grid per light");
    }
    cudaStream_t stream{};
    CUDA_CHECK(cudaStreamCreate(&stream));

    // Setting up some rng stuff
    const int pixelCount = rs.width * rs.height;
    RngState* device_rng_states = nullptr;
     CUDA_CHECK(create_rng_states(
        &device_rng_states,
        pixelCount,
        1234ULL,  // seed
        stream
    ));

    // density and color packing
    auto density_handle = pack_grid_to_nano(dens_grid, stream);
    auto color_handle = pack_grid_to_nano(col_grid, stream);

    // SHADOWS

    using ShadowHandle = decltype(pack_grid_to_nano(
        std::declval<const GridPtr<float>&>(),
        std::declval<cudaStream_t>()));

    std::vector<ShadowHandle> shadow_handles;
    shadow_handles.reserve(shadow_grids.size());

    std::vector<const nanovdb::NanoGrid<float>*> host_shadow_grid_ptrs;
    host_shadow_grid_ptrs.reserve(shadow_grids.size());

    for (const auto& grid : shadow_grids) {
        shadow_handles.emplace_back(pack_grid_to_nano(grid, stream));
        host_shadow_grid_ptrs.push_back(
            shadow_handles.back().deviceGrid<float>());
    }

    auto device_shadow_array = send_array_to_gpu(
        host_shadow_grid_ptrs.data(),
        host_shadow_grid_ptrs.size());
    
    std::vector<float4> host_light_colors;
    host_light_colors.reserve(lights.size());

    for (const auto& light : lights) {
        host_light_colors.push_back(make_float4(
            light.color.red(),
            light.color.green(),
            light.color.blue(),
            light.kappa
        ));
    }
    float4* device_light_colors = send_array_to_gpu(host_light_colors.data(), host_light_colors.size());

    // GPU volume for ray marching
    GPUVolume volume{};
    volume.density = density_handle.deviceGrid<float>();
    volume.color = color_handle.deviceGrid<nanovdb::Vec3f>();
    volume.shadow_maps = device_shadow_array;
    volume.shadow_map_count = lights.size();
    volume.light_colors = device_light_colors;

    // -----------------------------------------------
    // Getting the cam and image into the GPU
    // ----------------------------------------------------------------
    GPUCamera gpuCam(cam);

    std::vector<float4> pixels(rs.width * rs.height);
    float4* devicePixels = nullptr;

    CUDA_CHECK(cudaMalloc(
        reinterpret_cast<void**>(&devicePixels),
        pixels.size() * sizeof(float4))
    );

    // log_cuda_memory("after allocations");
    // ----------------------------------------------------------------
    // The actual render
    // ----------------------------------------------------------------

    CUDA_CHECK(launch_cuda_raymarch(
        volume, devicePixels, rs.width, rs.height, gpuCam,
        rs.near_dist, rs.far_dist, rs.min_step, rs.max_step, rs.kappa, device_rng_states, stream));

    CUDA_CHECK(cudaStreamSynchronize(stream));
    CUDA_CHECK(cudaMemcpy(
        pixels.data(), devicePixels,
        pixels.size() * sizeof(float4),
        cudaMemcpyDeviceToHost)
    );
    CUDA_CHECK(destroy_rng_states(device_rng_states));
    device_rng_states = nullptr;

    // log_cuda_memory("after render sync");

    // ----------------------------------------------------------------
    // Saving out the final image
    // ----------------------------------------------------------------

    ImageData img(rs.width, rs.height, 4);
    for (int i = 0; i < rs.width * rs.height; ++i) {
        const int x = i % rs.width;
        const int y = i / rs.width;
        ImageData::pixel p = {pixels[i].x, pixels[i].y, pixels[i].z, pixels[i].w};
        img.set_pixel_values(x, y, p);
    }
    //std::string frame_filename = path + name + "." + StringFuncs::get_zero_padded_number_string(0, 4) + ".exr";
    img.oiio_write_to(filename);
    std::cout << "Wrote to " << filename << std::endl;
    CUDA_CHECK(cudaStreamSynchronize(stream));
    CUDA_CHECK(cudaFree(devicePixels));
    CUDA_CHECK(cudaFree(device_shadow_array));
    CUDA_CHECK(cudaFree(device_light_colors));

    CUDA_CHECK(cudaStreamDestroy(stream));
    // log_cuda_memory("after raw frees");
}

} // end namespace lux