#ifndef STENCIL_H
#define STENCIL_H

#include <memory>
#include <random> 
#include <iostream>

class Stencil {
public:
    Stencil(int half_width = 1);
    ~Stencil();

    void resize(int new_half_width);
    void randomize_values(float lower_bound = -0.1f, float upper_bound = 0.1f);

    int get_halfwidth() const { return _half_width; };
    int get_n_elements() const { return (2 * _half_width + 1) * (2 * _half_width + 1); };

    float& get_value_offset_from_center(int x_offset, int y_offset);
    float& operator() (int i, int j);
    const float& operator() (int i, int j) const;
    void print_stencil() const;


private:
    int _half_width;
    int _two_half_width_plus_one;
	std::unique_ptr<float[]> _stencil_values;


};


#endif