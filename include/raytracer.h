#pragma once

#include <vector>
#include <optional>
#include <utility>
#include <type_traits>
#include <limits>

namespace lux
{


template <typename Vec3>
Vec3 cross(const Vec3& v1, const Vec3& v2){
    return Vec3(
        v1.y() * v2.z() - v1.z() * v2.y(),
        v1.z() * v2.x() - v1.x() * v2.z(),
        v1.x() * v2.y() - v1.y() * v2.x()
    );
}

template <typename Vec3>
auto dot(const Vec3& v1, const Vec3& v2){
    return v1.x() * v2.x() + v1.y() * v2.y() + v1.z() * v2.z();
}


template<typename Vec, typename Vec3i>
std::optional<Vec> get_ray_triangle_intersection(
    const Vec& start_pos,
    const Vec& ray_dir,
    const Vec3i& tri,
    const std::vector<Vec>& verts
){
    const Vec& v0 = verts[tri[0]];
    const Vec& v1 = verts[tri[1]];
    const Vec& v2 = verts[tri[2]];

    const Vec e1 = v1 - v0;
    const Vec e2 = v2 - v0;
    const Vec normal = cross(e1, e2);

    const auto denominator = dot(normal, ray_dir);

    // The ray is parallel to the triangle.
    if (std::abs(denominator) < 1e-6) {
        return std::nullopt;
    }

    const auto distance =
        dot(normal, v0 - start_pos) / denominator;

    // The intersection is behind the ray origin.
    if (distance < 0) {
        return std::nullopt;
    }

    const Vec hit = start_pos + distance * ray_dir;
    const Vec offset = hit - v0;

    const auto e1e1 = dot(e1, e1);
    const auto e1e2 = dot(e1, e2);
    const auto e2e2 = dot(e2, e2);
    const auto offset_e1 = dot(offset, e1);
    const auto offset_e2 = dot(offset, e2);

    const auto barycentric_denominator = e1e1 * e2e2 - e1e2 * e1e2;

    const auto u = (e2e2 * offset_e1 - e1e2 * offset_e2) / barycentric_denominator;

    const auto v = (e1e1 * offset_e2 - e1e2 * offset_e1) / barycentric_denominator;

    if (u < 0 || v < 0 || u + v > 1) {
        return std::nullopt;
    }
    return hit;
}

template<typename Vec, typename Vec4i>
std::optional<Vec> get_ray_face_intersection(
    const Vec& start_pos,
    const Vec& ray_dir,
    const Vec4i& face,
    const std::vector<Vec>& verts
){
    // Check intersection with the first triangle (v0, v1, v2)
    auto hit = get_ray_triangle_intersection(start_pos, ray_dir, Vec4i(face[0], face[1], face[2], 0), verts);
    if (hit) {
        return hit;
    }

    // Check intersection with the second triangle (v0, v2, v3)
    hit = get_ray_triangle_intersection(start_pos, ray_dir, Vec4i(face[0], face[2], face[3], 0), verts);
    if (hit) {
        return hit;
    }

    return std::nullopt;
};


template<typename Vec, typename Vec4i, typename Vec3i>
std::optional<Vec> get_first_hit_position(
    const Vec& start_pos,
    const Vec& ray_dir,
    const std::vector<Vec3i>& tris,
    const std::vector<Vec4i>& faces,
    const std::vector<Vec>& verts
){
    using Component = std::decay_t<decltype(std::declval<Vec>().x())>;
    using HitRes = std::optional<Vec>;
    auto tri_intersection = [&start_pos, &ray_dir, &verts](const Vec3i& tri){
        return get_ray_triangle_intersection(start_pos, ray_dir, tri, verts);
    };
    auto face_intersection = [&start_pos, &ray_dir, &verts](const Vec4i& face){
        return get_ray_face_intersection(start_pos, ray_dir, face, verts);
    };
    auto dist_sq = [&start_pos](const Vec& pos){
        const Vec offset = pos - start_pos;
        return dot(offset, offset);
    };
    auto comp_func = [&dist_sq](const HitRes& lhs, const HitRes& rhs){
        if (lhs && !rhs) return true;
        if (!lhs && rhs) return false;
        if (!lhs && !rhs) return false;
        return dist_sq(*lhs) < dist_sq(*rhs);
    };

    std::vector<HitRes> tri_hits(tris.size());
    std::vector<HitRes> face_hits(faces.size());
    std::transform(tris.begin(), tris.end(), tri_hits.begin(), tri_intersection);
    std::transform(faces.begin(), faces.end(), face_hits.begin(), face_intersection);
    tri_hits.insert(tri_hits.end(), face_hits.begin(), face_hits.end());
    // Now return the min of the two
    auto nearest = std::min_element(tri_hits.begin(), tri_hits.end(), comp_func);
    if (nearest != tri_hits.end() && *nearest){
        return *nearest;
    }
    return std::nullopt;
}

} // namespace lux
