#ifndef _OBJ_READER_H
#define _OBJ_READER_H


// include std::ifstream
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <array>

#include "the_wheel.h"
#include "cato_vector.h"

template <typename Vec3>
class ObjReader{
public:
	ObjReader(const std::string& filename) {
		std::ifstream in(filename);

		std::vector<std::string> lines;
		std::string line;
		while (std::getline(in, line)){
			if (line.substr(0, 2) == "v "){
				_verts.push_back(_line_to_vec<double, Vec3>(line));
			} else if (line.substr(0, 2) == "f "){
				_faces.push_back(_parse_face(line));
			}
		}
	}

	auto get_verts() { return pba::span<Vec3>(_verts); }
	auto get_faces() { return pba::span<const cato::Vec3i>(_faces); }

private:
	ObjReader() = delete;

	template <typename T, typename VecType>
	VecType _line_to_vec(const std::string& l) const {
		std::istringstream ss(l.substr(2));
		T x, y, z;
		ss >> x >> y >> z;
		return VecType(x, y, z);
	}

	cato::Vec3i _parse_face(const std::string& l) const {
		std::istringstream ss(l.substr(2));
		std::array<int, 3> indices;
		for (int i = 0; i < 3; i++) {
			std::string token;
			ss >> token;
			// take only the part before the first '/'
			indices[i] = std::stoi(token.substr(0, token.find('/')));
		}
		// OBJ indices are 1-based, convert to 0-based
		return cato::Vec3i(indices[0] - 1, indices[1] - 1, indices[2] - 1);
	}

	std::vector<Vec3> _verts;
	std::vector<cato::Vec3i> _faces; 
};


#endif