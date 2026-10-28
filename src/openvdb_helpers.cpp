#include "openvdb_helpers.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace lux{



namespace {
void validate_voxel_size(float voxel_size) {
    if (!std::isfinite(voxel_size) || voxel_size <= 0) {
        throw std::invalid_argument("voxel_size must be positive and finite");
    }
}
int checked_coord(double value, bool round_up) {
    value = round_up ? std::ceil(value) : std::floor(value);
    if (!std::isfinite(value) || value < std::numeric_limits<int>::min() ||
        value > std::numeric_limits<int>::max()) {
        throw std::invalid_argument("grid coordinate is not finite or is out of range");
    }
    return static_cast<int>(value);
}
}

openvdb::CoordBBox world_space_to_bounds(const Vector& llc, const Vector& urc, float voxel_size){
    validate_voxel_size(voxel_size);
    if (llc.X() > urc.X() || llc.Y() > urc.Y() || llc.Z() > urc.Z()) {
        throw std::invalid_argument("lower bounds must not exceed upper bounds");
    }
    return openvdb::CoordBBox(
        openvdb::Coord(checked_coord(llc.X() / voxel_size, false),
                       checked_coord(llc.Y() / voxel_size, false),
                       checked_coord(llc.Z() / voxel_size, false)),
        openvdb::Coord(checked_coord(urc.X() / voxel_size, true),
                       checked_coord(urc.Y() / voxel_size, true),
                       checked_coord(urc.Z() / voxel_size, true)));
}

openvdb::CoordBBox world_space_to_bounds(const Vector& llc, const Vector& urc, const openvdb::FloatGrid::Ptr grid){
    if (!grid) throw std::invalid_argument("grid cannot be null");
    if (!std::isfinite(llc.X()) || !std::isfinite(llc.Y()) || !std::isfinite(llc.Z()) ||
        !std::isfinite(urc.X()) || !std::isfinite(urc.Y()) || !std::isfinite(urc.Z()))
        throw std::invalid_argument("bounds must be finite");
    if (llc.X() > urc.X() || llc.Y() > urc.Y() || llc.Z() > urc.Z()) {
        throw std::invalid_argument("lower bounds must not exceed upper bounds");
    }
    // Transform all eight corners so translated and rotated grids work too.
    openvdb::Vec3d min(std::numeric_limits<double>::max());
    openvdb::Vec3d max(std::numeric_limits<double>::lowest());
    for (int x = 0; x < 2; ++x) for (int y = 0; y < 2; ++y) for (int z = 0; z < 2; ++z) {
        auto p = grid->worldToIndex(openvdb::Vec3d(x ? urc.X() : llc.X(), y ? urc.Y() : llc.Y(), z ? urc.Z() : llc.Z()));
        for (int i = 0; i < 3; ++i) { min[i] = std::min(min[i], p[i]); max[i] = std::max(max[i], p[i]); }
    }
    return openvdb::CoordBBox(
        openvdb::Coord(checked_coord(min[0], false), checked_coord(min[1], false), checked_coord(min[2], false)),
        openvdb::Coord(checked_coord(max[0], true), checked_coord(max[1], true), checked_coord(max[2], true)));
}

openvdb::FloatGrid::Ptr create_float_grid(float voxel_size, float default_val){
    auto grid = openvdb::FloatGrid::create(default_val);
    grid->setGridClass(openvdb::GridClass::GRID_FOG_VOLUME);
    grid->setTransform(openvdb::math::Transform::createLinearTransform(voxel_size));
    return grid;
}

// TODO this should be templated
openvdb::Vec3SGrid::Ptr create_color_grid(float voxel_size, openvdb::Vec3s def_val){
    auto grid = openvdb::Vec3SGrid::create(def_val);
    grid->setGridClass(openvdb::GridClass::GRID_FOG_VOLUME);
    grid->setTransform(openvdb::math::Transform::createLinearTransform(voxel_size));
    return grid;
}

openvdb::FloatGrid::Ptr obj_mesh_to_level_set_f(const std::string& obj_path, float voxel_size, float half_width, bool recenter){
    validate_voxel_size(voxel_size);
    if (!std::isfinite(half_width) || half_width <= 0) throw std::invalid_argument("half_width must be positive and finite");
    std::ifstream input(obj_path);
    if (!input) throw std::runtime_error("Could not open OBJ file: " + obj_path);
    std::vector<openvdb::Vec3s> points;
    std::vector<openvdb::Vec3I> triangles;
    std::vector<openvdb::Vec4I> quads;
    std::string line;
    size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        auto comment = line.find('#');
        if (comment != std::string::npos) line.erase(comment);
        std::istringstream stream(line);
        std::string tag;
        stream >> tag;
        if (tag == "v") {
            float x, y, z;
            if (!(stream >> x >> y >> z) || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
                throw std::runtime_error("Invalid OBJ vertex at line " + std::to_string(line_number));
            points.emplace_back(x, y, z);
        } else if (tag == "f") {
            std::vector<int> indices;
            std::string token;
            while (stream >> token) {
                auto slash = token.find('/');
                auto value = token.substr(0, slash);
                size_t consumed = 0;
                int index;
                try { index = std::stoi(value, &consumed); }
                catch (const std::exception&) { throw std::runtime_error("Invalid OBJ face index at line " + std::to_string(line_number)); }
                if (consumed != value.size() || index == 0)
                    throw std::runtime_error("Invalid OBJ face index at line " + std::to_string(line_number));
                int resolved = index > 0 ? index - 1 : static_cast<int>(points.size()) + index;
                if (resolved < 0 || static_cast<size_t>(resolved) >= points.size())
                    throw std::runtime_error("OBJ face index out of range at line " + std::to_string(line_number));
                indices.push_back(resolved);
            }
            if (indices.size() == 3) triangles.emplace_back(indices[0], indices[1], indices[2]);
            else if (indices.size() == 4) quads.emplace_back(indices[0], indices[1], indices[2], indices[3]);
            else throw std::runtime_error("OBJ faces must be triangles or quads (line " + std::to_string(line_number) + ")");
        }
    }
    auto grid = mesh_to_levelset(voxel_size, points, triangles, quads, half_width);
    if (recenter) {
        openvdb::Vec3d center(0.0);
        for (const auto& p : points) center += openvdb::Vec3d(p.x(), p.y(), p.z());
        center /= static_cast<double>(points.size());
        grid->transform().postTranslate(-center);
    }
    return grid;
}

VSPtr<float> obj_mesh_to_grid_field(const std::string& obj_path, float voxel_size, float half_width, bool recenter){
    return make_grid_field<float>(obj_mesh_to_level_set_f(obj_path, voxel_size, half_width, recenter));
}

openvdb::FloatGrid::Ptr mesh_to_levelset(
    float voxel_size, std::vector<openvdb::Vec3s> points,
    std::vector<openvdb::Vec3I> triangles, std::vector<openvdb::Vec4I> quads, float half_width)
{
    validate_voxel_size(voxel_size);
    if (!std::isfinite(half_width) || half_width <= 0) throw std::invalid_argument("half_width must be positive and finite");
    if (points.empty() || (triangles.empty() && quads.empty())) throw std::invalid_argument("mesh must contain points and faces");
    for (const auto& p : points) if (!std::isfinite(p.x()) || !std::isfinite(p.y()) || !std::isfinite(p.z()))
        throw std::invalid_argument("mesh points must be finite");
    for (const auto& face : triangles) for (int i = 0; i < 3; ++i)
        if (face[i] < 0 || static_cast<size_t>(face[i]) >= points.size()) throw std::invalid_argument("triangle index out of range");
    for (const auto& face : quads) for (int i = 0; i < 4; ++i)
        if (face[i] < 0 || static_cast<size_t>(face[i]) >= points.size()) throw std::invalid_argument("quad index out of range");
    openvdb::initialize();
    auto transform = openvdb::math::Transform::createLinearTransform(voxel_size);
    return openvdb::tools::meshToLevelSet<openvdb::FloatGrid>(*transform, points, triangles, quads, half_width);
}


GridPtr<float> stamp_isf_to_grid(
    float voxel_size,
    float def_val,
    const vspf& a,
    const coordbbox& bounds,
    std::string filename
){
    auto func = [a](const Vector& p){ return a->eval(p); };
    auto cond = [](const float val){ return val <= 0; };
    return stamp_grid_parallel<float>(voxel_size, def_val, func, cond, bounds, std::move(filename));
}

GridPtr<Color> stamp_color_to_grid(
    const vspc& col,
    const openvdb::CoordBBox& bounds,
    float voxel_size,
    std::string filename

){
    auto func = [col](const Vector& p){ return col->eval(p); };
    auto cond = [](const Color& val){return true; };
    auto default_val = GridTypes<Color>::to_grid(Color(0, 0, 0, 0));
    auto col_grid = stamp_grid_parallel<Color>(voxel_size, default_val, func, cond, bounds);
   
    return col_grid;
}


} // end namespace lux
