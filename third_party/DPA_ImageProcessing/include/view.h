#ifndef VIEW_H
#define VIEW_H

#include <GL/glut.h>
#include "controller.h"
#include "model.h"

class View
{
public:

	static View* instance()
	{
	if(pView==nullptr)
	{
		pView = new View();
	}
	return pView;
	}

	~View();

	void init( int argc, char** argv, int _width, int _height);
	void display();
	void reshape( int w, int h );
	void idle();
	void main_loop();

private:
	
	static View* pView;

	View();
	View( const View& );
	View& operator= (const View&);
	
	int width, height;
};

View* create_view();

#endif