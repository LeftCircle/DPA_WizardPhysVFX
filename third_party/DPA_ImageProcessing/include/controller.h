#ifndef CONTROLLER_H
#define CONTROLLER_H


#include <iostream>
#include <GL/glut.h>

#include "image_data.h"
#include "image_editing.h"
#include "model.h"
#include "file_utils.h"
#include "image_data_modifier.h"

// A singleton controller class that will be used to drive GLUT inputs
class Controller
{
public:

	static Controller* instance()
	{
		if(pController==nullptr)
		{
			pController = new Controller();
		}
		return pController;
	}

	~Controller();

	void keyboard( unsigned char key, int x, int y );
	void special_keys(int key, int x, int y);

	// Temp values for now
	std::vector<float> colors = {
		0.769f, 0.855f, 0.365f,
		0.953f, 0.953f, 0.757f,
		0.784f, 0.957f, 0.918f,
		0.396f, 0.612f, 0.769f,
		0.196f, 0.314f, 0.490f,
		0.129f, 0.196f, 0.290f
	};

private:

	std::pair<int, double> _get_julia_set_paramters();
	void _apply_stencil();

	static Controller* pController;

	Controller();
	Controller( const Controller& );
	Controller& operator= (const Controller& );

};


Controller* create_controller();

#endif