#include "humanoid.h"
#include "string_funcs.h"

namespace lux{

#define RADTODEG(radians) ((radians) * 180.0f / 3.14159265358979)
#define DEGTORAD(degrees) ((degrees * 3.14159265358979 / 180.0))

using vecsptr = std::shared_ptr<const std::vector<vspf>>;


//std::make_shared<const std::vector<vspf>>(std::initializer_list<vspf>{box, torus})

isf_col isf_staff(VolumeSPtr<Color> cf, float t){
    auto staff = isf_cylinder(Vector(), Vector(1, 0, 0), 0.1, 3.25);
    staff = translate_fixed(staff, Vector(1, 0, 0));
    staff = dilation(staff, 0.002);
    staff = staff * make_constant(5.0f);
    auto staff_color = make_constant(comfy_colors::white_stout);
    cf = staff_color*mask(staff) + cf * mask(-staff);
    auto cone = isf_cone(Vector(3, 0, 0), Vector(-1, 0, 0), 0.75, DEGTORAD(12.5));
    auto cone_color = make_constant(comfy_colors::green_mountain);
    cf = cone_color * mask(cone) + cf * mask(-cone);
    //staff = union_fields(staff, cone);
    // staff = blinn_blend(std::make_shared<const std::vector<vspf>>(std::initializer_list<vspf>{-staff, -cone}), 0.1, 1);
    // staff = -staff;
    auto torus = isf_torus(Vector(-1.5, 0, 0), 0.75, 0.15, Vector(0, 0, 1));
    auto ramp_x = funcfield<float>([](const Vector& p) {
        // 0 at the staff junction, 1 at the far end of the torus.
        return smoothstep(-0.65f, -2.35f, p.X());
    });
    auto sin_xf = funcfield<float>([](const Vector& p){return 0.15 * std::sin(p.X() * 15); });
    auto wave_disp = ramp_x * sin_xf;
    torus = translate(torus, make_constant(Vector(0, 0, 1)) * wave_disp);
    torus = dilation(torus, 0.003);
    torus = torus * make_constant(5.0f);
    auto torus_color = make_constant(comfy_colors::green_mountain);
    cf = torus_color * mask(torus) + cf * mask(-torus);
    staff = blinn_blend(std::make_shared<const std::vector<vspf>>(std::initializer_list<vspf>{-staff, -torus, -cone}), 0.1, 1);
    staff = -staff;
    auto orb = isf_icosahedron(Vector(0, 0, 0));
    orb = rotate_fixed(orb, Vector(1, 1, 0), RADTODEG(15 * t));
    orb = scale_fixed(orb, Vector(0.1, 0.1, 0.1));
    orb = translate_fixed(orb, Vector(-1.5, 0, 0));
    orb = dilation(orb, 0.003);
    orb = orb * make_constant(5.0f);
    auto orb_color = make_constant(comfy_colors::purpple_eastside);
    cf = orb_color * mask(orb) + cf * mask(-orb);
    staff = union_fields(staff, orb);
    
    //staff = cone;
    return isf_col(staff, cf);
}


isf_col isf_head(vspc cf){
    Vector top_head_pos = Vector(0, 0.5, 0);
    float thr = 1.5;
    auto top_head = isf_sphere(top_head_pos, thr);
    auto chin = isf_ellipse(top_head_pos - Vector(0, thr * 0.65, 0), Vector(1, 0, 0), thr * 0.8, thr * 0.55);
    
    auto eye_color = make_constant(comfy_colors::green_mountain);
    auto nose_lips_color = make_constant(comfy_colors::rose);

    float eye_radius = 0.3;
    float eye_hole_radius = eye_radius + 0.1;
    float eye_x = 0.4;
    auto eye_hole = isf_sphere(Vector(eye_x, 0.45, thr - eye_hole_radius / 2), eye_hole_radius);
    auto mirrored_hole = funcfield<float>([eye_hole](const Vector& p){ return eye_hole->eval(Vector(-p.X(), p.Y(), p.Z())); });
    eye_hole = union_fields(eye_hole, mirrored_hole);

    auto eyes = isf_sphere(Vector(eye_x, 0.45, thr - eye_radius * 1.5), eye_radius);
    auto mirrored_eye = funcfield<float>([eyes](const Vector& p){ return eyes->eval(Vector(-p.X(), p.Y(), p.Z())); });
    eyes = union_fields(eyes, mirrored_eye);

    eyes = eyes * make_constant(10000.0f);

    float nose_radius = eye_radius;
    auto nose = isf_sphere(Vector(nose_radius / 6.0, 0, thr), nose_radius);
    nose = intersection(nose, isf_sphere(Vector(-nose_radius / 6.0, 0, thr), nose_radius));
    auto plane = make_plane(Vector(), Vector(0, 1, 0));
    auto p2 = make_plane(Vector(0, 0, thr), Vector(0, 0, 1));
    nose = cutout(nose, plane);
    nose = cutout(nose, p2);
    nose = nose * make_constant(10000.0f);


    auto head = blinn_blend(std::make_shared<const std::vector<vspf>>(std::initializer_list<vspf>{-top_head, -chin}), 0.2, 2);
    head = -head;
    auto head_col = make_constant(comfy_colors::champagne);
    cf = head_col * mask(head) + cf * mask(-head);
    cf = eye_color * mask(eyes) + cf * mask(-eyes);
    cf = nose_lips_color * mask(nose) + cf * mask(-nose);
    head = cutout(head, eye_hole);
    head = union_fields(head, eyes);
    head = union_fields(head, nose);

    return isf_col(head, cf);
}

isf_col isf_steiner_ring(vspc cf){
    //auto stein = isf_steiner_patch(Vector());
    int n_patches = 10;
    float r = 2.5;
    auto steins = make_constant(0.0f);
    auto stein_color = make_constant(Color(0, 0, 0, 0));
    for (int i = 0; i < n_patches; i++){
        auto new_stein = isf_steiner_patch(Vector(0, 0, r));
        new_stein = rotate_fixed(new_stein, Vector(1, 0, 0), DEGTORAD(45));
        new_stein = funcfield<float>([new_stein](const Vector& p){ return new_stein->eval(Vector(p.X() * p.X(), p.Y() * p.Y(), p.Z() * p.Z())); });
        new_stein = rotate_fixed(new_stein, Vector(0, 1, 0), RADTODEG(360.0 / n_patches * i));
        new_stein = new_stein * make_constant(300.0f);
        steins = union_fields(steins, new_stein);
    }
    auto sc = make_constant(comfy_colors::purpple_eastside);
    steins = scale_fixed(steins, Vector(1.65, 1.35, 1.65));
    cf = sc * mask(steins) + cf * mask(-steins);
    return isf_col(steins, cf);
}

isf_col human(VolumeSPtr<Color> cf){
    // Head ---------------------------------
    Vector top_head_pos = Vector(0, 1.0, 0);
    float thr = 0.3;
    // auto top_head = isf_sphere(top_head_pos, thr);
    // auto chin = isf_ellipse(top_head_pos - Vector(0, thr * 0.65, 0), Vector(1, 0, 0), thr * 0.8, thr * 0.55);
    auto [head, head_col] = isf_head(cf);
    cf = head_col;
    head = scale_fixed(head, Vector(0.2, 0.2, 0.2));
    head = translate_fixed(head, Vector(0, 1, 0));
    cf = scale_fixed(cf, Vector(0.2, 0.2, 0.2));
    cf = translate_fixed(cf, Vector(0, 1, 0));


    // Body ---------------------------------
    Vector neck_point = top_head_pos - Vector(thr / 3.0, thr, 0);
    float torso_length = thr * 2.25;
    float torso_width = torso_length / 2.25;
    auto torso = isf_ellipse(Vector(0, 0, 0), Vector(0, 1, 0), torso_length, torso_width);
    torso = rotate_fixed(torso, Vector(0, 0, 1), DEGTORAD(22));
    torso = rotate_fixed(torso, Vector(1, 0, 0), DEGTORAD(-15));
    torso = translate_fixed(torso, Vector(-0.3, 0.2, -0.2));


    // Legs ---------------------------------
    float torso_base_adj_z = -0.25;
    float thigh_length = torso_length * 0.85;
    float thigh_width = 0.15;
    auto left_thigh = isf_ellipse(Vector(), Vector(1, 0, 0), thigh_length, thigh_width);
    left_thigh = rotate_fixed(left_thigh, Vector(0, 0, 1), DEGTORAD(45));
    left_thigh = translate_fixed(left_thigh, Vector(-0.2, -0.6, torso_base_adj_z));

    auto right_thigh = isf_ellipse(Vector(), Vector(1, 0, 0), thigh_length, thigh_width);
    right_thigh = rotate_fixed(right_thigh, Vector(0, 0, 1), DEGTORAD(-35));
    right_thigh = rotate_fixed(right_thigh, Vector(-1, 1, 0), DEGTORAD(-15));
    right_thigh = translate_fixed(right_thigh, Vector(-0.83, -0.6, 0.1 + torso_base_adj_z));

    auto left_shin = rotate_fixed(left_thigh, Vector(0, 0, 1), DEGTORAD(5));
    left_shin = translate_fixed(left_shin, Vector(thigh_length, -thigh_length * 1.0, 0));

    auto right_shin = isf_ellipse(Vector(), Vector(1, 0, 0), thigh_length, thigh_width * 0.8);
    right_shin = rotate_fixed(right_shin, Vector(0, 0, 1), DEGTORAD(-55));
    right_shin = translate_fixed(right_shin, Vector(-2.35 * thigh_length, -2 * thigh_length, 0));

    // Feet
    auto right_foot = isf_box(Vector(), 1.0, 2);
    right_foot = scale_fixed(right_foot, Vector(0.2, 0.2, 0.2));
    right_foot = translate_fixed(right_foot, Vector(0.7, -1.5, -0.2));

    auto left_foot = translate_fixed(right_foot, Vector(-2.3, 0, 0.2));
    auto feet = union_fields(left_foot, right_foot);


    // Arms ---------------------------------
    float arm_length = torso_length * 0.65;
    float arm_width = thigh_width * 0.65;
    auto upper_right_arm = isf_ellipse(Vector(), Vector(1, 0, 0), arm_length, arm_width);
    upper_right_arm = rotate_fixed(upper_right_arm, Vector(0, 0, 1), DEGTORAD(35));
    upper_right_arm = translate_fixed(upper_right_arm, Vector(-1.45 * arm_length, torso_length * 1.2, 0));

    auto right_forearm = isf_ellipse(Vector(), Vector(1, 0, 0), arm_length, arm_width);
    right_forearm = rotate_fixed(right_forearm, Vector(0, 0, 1), DEGTORAD(120));
    right_forearm = translate_fixed(right_forearm, Vector(-1.8 * arm_length, torso_length + arm_length * 1.3, 0));

    auto left_arms = union_fields(right_forearm, upper_right_arm);
    left_arms = rotate_fixed(left_arms, Vector(0, 0, 1), DEGTORAD(-75));
    left_arms = translate_fixed(left_arms, Vector(1.75, 1.8 * arm_length, 0));

    //auto head = union_fields(top_head, chin);
    auto right_arm = union_fields(right_forearm, upper_right_arm);
    auto arms = union_fields(right_arm, left_arms);
    torso = union_fields(torso, arms);
    auto thighs = union_fields(left_thigh, right_thigh);
    auto shins = union_fields(left_shin, right_shin);
    auto legs = union_fields(thighs, shins);
    //auto legs = thighs;
    //auto humanoid = union_fields(head, torso);
    //auto humanoid = union_fields(torso, legs);
    // auto humanoid = blinn_blend(std::make_shared<const std::vector<vspf>>(std::initializer_list<vspf>{
    //     -right_forearm, -upper_right_arm, -left_arms, -torso, -left_thigh,
    //     -left_shin, -right_thigh, -right_shin
    // }), 0.03, 15);
    // humanoid = -humanoid;
    auto humanoid = union_fields(legs, torso);

    auto pants = shell(humanoid, 0.1);
    pants = dilation(pants, 0.5);
    auto pantsp1 = make_plane(Vector(), Vector(0, -1, 0));
    auto pantsp2 = make_plane(Vector(0, -1, 0), Vector(0, 1, 0));
    pants = cutout(pants, pantsp1);
    pants = cutout(pants, pantsp2);
    pants = pants * make_constant(10.0f);
    pants = clamp(pants, make_constant(-100.0f), make_constant(1.0f));

    humanoid = union_fields(humanoid, arms);

    humanoid = union_fields(humanoid, feet);
    humanoid = dilation(humanoid, 0.75);
    humanoid = humanoid * make_constant(1.5f);
    // humanoid = humanoid * make_constant(0.01f);
    auto body_color = make_constant(comfy_colors::champagne);
    cf = body_color * mask(humanoid) + cf * mask(-humanoid);
    auto pants_color = make_constant(comfy_colors::green_mountain);
    cf = pants_color * mask(pants) + cf * mask(-pants);
    humanoid = union_fields(humanoid, head);
    humanoid = union_fields(humanoid, pants);
    return isf_col(humanoid, cf);

}

isf_col combine_human_and_staff(float t){
    VolumeSPtr<Color> cf = make_constant(Color(0, 0, 0, 0));
    auto [staff, staff_col] = isf_staff(cf, t);
    cf = staff_col;
    staff = rotate_fixed(staff, Vector(1, 0, 0), DEGTORAD(65));
    staff = scale_fixed(staff, Vector(0.7, 0.7, 0.7));
    staff = translate_fixed(staff, Vector(-0.9, 0.2, 0));
    cf = rotate_fixed(cf, Vector(1, 0, 0), DEGTORAD(65));
    cf = scale_fixed(cf, Vector(0.7, 0.7, 0.7));
    cf = translate_fixed(cf, Vector(-0.9, 0.2, 0));
    staff = rotate_fixed(staff, Vector(0, 0, 1), DEGTORAD(30));
    staff = translate_fixed(staff, Vector(0.375, 0.425, 0));
    cf = rotate_fixed(cf, Vector(0, 0, 1), DEGTORAD(30));
    cf = translate_fixed(cf, Vector(0.375, 0.425, 0));
    
    auto [humanoid, human_col] = human(cf);
    humanoid = scale_fixed(humanoid, Vector(0.75, 0.75, 0.75));
    humanoid = translate_fixed(humanoid, Vector(0, -0.25, 0));
    human_col = scale_fixed(human_col, Vector(0.75, 0.75, 0.75));
    human_col = translate_fixed(human_col, Vector(0, -0.25, 0));
    

    cf = human_col + cf;
    
    humanoid = union_fields(humanoid, staff);
    auto [steins, scol] = isf_steiner_ring(cf);
    cf = cf + scol;
    humanoid = union_fields(humanoid, steins);
    return isf_col(humanoid, cf);
}

void raymarch_humanoid(const std::string& filename, float t){
    int n_images = 120;
    int fps = 24;
    float cam_distance = 9;
    float scene_width = 5;
    float near = cam_distance - scene_width / 2.0;
    float far = near + scene_width;
    float dt = 1.0 / (float)fps;

    RayMarcher rm;
    float min_ds = (far - near) / 330;
    float max_ds = min_ds * 3.65;
    rm.set_min_ds(min_ds);
    rm.set_max_ds(max_ds);
    rm.set_snear(near);
    rm.set_sfar(far);
    rm.set_exticntion_coefficient(3.0);
    rm.set_Tmin(0.001);
    Camera cam;
    //humanoid = -mask(humanoid);

    for (int i = 88; i < n_images; i++){
        auto [humanoid, cf] = combine_human_and_staff(t);
        // auto col = make_constant<Color>(Color(0, 0, 0, 0));
        // auto [humanoid, cf] = isf_staff(col, t);
        ImageData render_img(1920, 1080, 4);
        Vector eye = Vector(0, 0, cam_distance);
        Vector view = Vector(0, 0, -1);
        eye = rotation(eye, Vector(0, 1, 0), DEGTORAD(360.0 / n_images * i));
        view = rotation(view, Vector(0, 1, 0), DEGTORAD(360.0 / n_images * i));
        cam.setEyeViewUp(eye, view, Vector(0, 1, 0));
        rm.ray_march_image(cam, render_img, humanoid, cf);
        std::string frame_filename = filename + "." + StringFuncs::get_zero_padded_number_string(i, 4) + ".exr";
        render_img.oiio_write_to(frame_filename);
        t += dt;
    }
}

} // end namespace lux