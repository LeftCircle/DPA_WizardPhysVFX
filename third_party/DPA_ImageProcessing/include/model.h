#ifndef MODEL_H
#define MODEL_H

#include <iostream>
#include <GL/glut.h>

#include "stencil.h"
#include "image_data.h"
#include "image_editing.h"
#include "lut.h"

const int DEFAULT_STENCIL_HALF_WIDTH = 5;

// Julia set parameters
const double RANGE_SCALE_FACTOR = 10.0;
const int JULIA_ITERATION_STEP = 50;
const int DEFAULT_JULIA_ITERATIONS = 100;
const double DEFAULT_JULIA_RANGE = 1.0;
const Point DEFAULT_CENTER(0.03811, 0.01329);
const Point DEFAULT_COMPLEX_CENTER(0.8*cos(254.3 * 3.14159265/180.0), 0.8*sin(254.3 * 3.14159265/180.0));


class Model
{
public:
	enum State {
		DEFAULT,
		JULIA_SET
	};

    static Model* instance() {
		if(pModel==nullptr)
		{
			pModel = new Model();
		}
		return pModel;
	}
    
	~Model();
    
    
    ImageEditor image_editor;
    Stencil stencil = Stencil(DEFAULT_STENCIL_HALF_WIDTH);
	
	const std::shared_ptr<ImageData> get_modified_image_ptr() const { return image_editor.get_edited_image(); }
	
	void fractal_flames();
	void apply_julia_set(const int iterations = 100, const double range = 1.0);
	void on_J_pressed();
	void on_up_arrow_pressed();
	void on_down_arrow_pressed();
	void on_right_arrow_pressed();
	void on_left_arrow_pressed();
	void convert_to_contrast_units();

private:
	std::shared_ptr<ImageData> _image_data;
	
	static Model* pModel;
	State _current_state = DEFAULT;

	Model();
	Model( const Model& );
	Model& operator= (const Model& );

	// Some private variables that could be better placed in specific state classes
	int _julia_iterations = 100;
	double _julia_range = 1.0;
};


Model* create_model(ImageData& image_data);

#endif