#include "ray_marcher.h"

#include <iostream>
#include <random>


using namespace lux;


Color RayMarcher::ray_march_single_pixel(
    const Vector& start_pos,
    const Vector& direction,
    const Vector& eye,
    const VolumeSPtr<float>& density,
    const VolumeSPtr<Color>& color
) const {
    double T = 1; // Transmisivity
    Color L(0,0,0,0);
    // set s to be the distance from eye to start_pos;
    double s = (start_pos - eye).magnitude();
    Vector X = start_pos;
    thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_real_distribution<float> distribution(_min_ds, _max_ds);
    while( s < _sfar && T > _Tmin ) {
        float den = density->eval(X);
        float ds = distribution(generator);
        if( den < 0.0 ){
            float shadow_eval = 1.0;
            if (_kappa == 0.0){
                L += color->eval(X);
                T = 0;
            } else {
                Color clights(0, 0, 0, 0);
                for (int i = 0; i < _shadow_maps.size(); i++){
                    clights += _light_colors[i] * _shadow_maps[i]->eval(X);
                }
                float dT = std::exp( ds * _kappa * den ); // dens is negative here, so remove - mult;
                L += color->eval(X) * (1-dT) * T * _one_over_kappa * clights;
                //L += (1-dT) * T * _one_over_kappa * clights;
                T *= dT;
            }
        }
        X += direction * ds;
        s += ds;
    }
    L[3] = 1-T; // set the alpha channel to the opacity
    return L;
}

Color RayMarcher::ray_march_single_pixel(
        const Vector& direction,
        const Vector& eye,
        const VolumeSPtr<float>& density,
        const VolumeSPtr<Color>& color
) const {
    const Vector start_pos = eye + direction * _snear;
    return ray_march_single_pixel(start_pos, direction, eye, density, color);
}

Color RayMarcher::ray_march_single_pixel(
        const Vector& direction,
        const Vector& eye,
        const VolumeSPtr<float>& density,
        const VolumeSPtr<Color>& color,
        const openvdb::FloatGrid::Ptr& level_set
) const {
    const Vector start_pos = eye + direction * _snear;
    double T = 1; // Transmisivity
    Color L(0,0,0,0);
    // set s to be the distance from eye to start_pos;
    double s = (start_pos - eye).magnitude();
    Vector X = start_pos;
    auto vdb_x = level_set->transform().worldToIndex({X.x(), X.y(), X.z()});
    thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_real_distribution<float> distribution(_min_ds, _max_ds);

    auto hw = level_set->background();
    auto vs = level_set->voxelSize();
    auto accessor = level_set->getConstAccessor();
    float out = 0;
    float in = 0;
    const float skip = hw * 0.9;
    const float check_dist = 1e-5;
    
    while( s < _sfar && T > _Tmin ) {
        // evaluate the level_set. if default positive value then take big steps
        float den = density->eval(X);
        if (den >= 0.0 && std::abs(openvdb::tools::PointSampler::sample(level_set->tree(), vdb_x) - hw) < check_dist){
            // take a big step
            X += direction * skip;
            vdb_x = level_set->transform().worldToIndex({X.x(), X.y(), X.z()});
            s += skip;
            out += skip;
            continue;
        }
        float ds = distribution(generator);
        if( den < 0.0 ){
            float shadow_eval = 1.0;
            if (_kappa == 0.0){
                L += color->eval(X);
                T = 0;
            } else {
                Color clights(0, 0, 0, 0);
                for (int i = 0; i < _shadow_maps.size(); i++){
                    clights += _light_colors[i] * _shadow_maps[i]->eval(X);
                }
                float dT = std::exp( ds * _kappa * den ); // dens is negative here, so remove - mult;
                L += color->eval(X) * (1-dT) * T * _one_over_kappa * clights;
                //L += (1-dT) * T * _one_over_kappa * clights;
                T *= dT;
            }
        }
        X += direction * ds;
        s += ds;
        vdb_x = level_set->transform().worldToIndex({X.x(), X.y(), X.z()});
        in += ds;
    }
    L[3] = 1-T; // set the alpha channel to the opacity
    //printf("out = %g, in = %g, out percent = %g\n", out, in, (float)out / (float)(out + in));
    return L;
}


void RayMarcher::ray_march_image(
    Camera cam,
    ImageData& img,
    const VolumeSPtr<float>& density,
    const VolumeSPtr<Color>& color
) const {   
    openvdb::tools::VolumeRayIntersector<openvdb::FloatGrid> main_rayinter(*_levelset);

    const int width = img.get_width();
    const int height = img.get_height();
    #pragma omp parallel
    {
    openvdb::tools::VolumeRayIntersector<openvdb::FloatGrid> rayinter(main_rayinter);
    #pragma omp for schedule(dynamic, 32)
    for (int pixel_index = 0; pixel_index < width * height; ++pixel_index) {
        const int i = pixel_index % width;
        const int j = pixel_index / width;
        
        auto ray = get_ray(cam, i, j, width, height);
        if (!rayinter.setWorldRay(ray)) {
            //printf("no volume \n");
            continue;
        }
        Color pixel = ray_march_single_pixel(ray.dir(), ray.eye(), density, color, _levelset);
        ImageData::pixel p = {(float)pixel.red(), (float)pixel.green(), (float)pixel.blue(), (float)pixel.alpha()};
        img.set_pixel_values(i, j, p);
    }
    } // end parallel context
}

openvdb::math::Ray<double> RayMarcher::get_ray(
    Camera& cam,
    int x,
    int y,
    int width,
    int height
) {
    const float htanfov = cam.get_htanfov();
    const float vtanfov = cam.get_vtanfov();
    const float one_over_nx_pixelsf = 1.0 / (float)width;
    const float one_over_ny_pixelsf = 1.0 / (float)height;
    const Vector& rhat = cam.right();
    const Vector& vhat = cam.up();
    const Vector& ncam = cam.view();
    const Vector& eye = cam.eye();

    float u = (2.0 * x * one_over_nx_pixelsf - 1.0) * htanfov;
    float v = (2.0 * y * one_over_ny_pixelsf - 1.0) * vtanfov;

    Vector ray_dir = (u * rhat + v * vhat + ncam).unitvector();
    return openvdb::math::Ray<double>(
        openvdb::Vec3d(eye.x(), eye.y(), eye.z()),
        openvdb::Vec3d(ray_dir.x(), ray_dir.y(), ray_dir.z())
    );
}




