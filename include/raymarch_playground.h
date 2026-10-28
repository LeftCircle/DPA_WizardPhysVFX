#pragma once


#include <string>
#include <iostream>
#include <memory>

#include "field_interface.h"
#include "ray_marcher.h"
#include "image_data.h"
#include "command_line_parser.h"


namespace lux{

using isf_col = std::tuple<vspf, vspc>;


void raymarch_things(const std::string& filename, float t = 0);



class comfy_colors{
public:
    inline static const Color rose = Color(243, 172, 202, 0) / 256.0;
    inline static const Color champagne = Color(251, 231, 205, 0) / 256.0;
    inline static const Color purpple_eastside = Color(169, 143, 211, 0) / 255.0;
    inline static const Color green_mountain = Color(126, 199, 185, 0) / 255.0;
    inline static const Color gold_subtle = Color(246, 235, 170, 0) / 255.0;
    inline static const Color white_stout = Color(239, 235, 219, 0) / 255.0;
};


}