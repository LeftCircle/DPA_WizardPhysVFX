//-------------------------------------------------------
//
//  ScreenCapturePPM.h
//
//  PbaThing for ScreenCapturePPM
//
//  Copyright (c) 2017 Jerry Tessendorf
//
//
//--------------------------------------------------------


#include "PbaThing.h"

namespace pba{


class ScreenCapturePPMThing: public PbaThingyDingy
{
  public:

    ScreenCapturePPMThing(const std::string& fname, const std::string nam="ScreenCapturePPMThing");
   ~ScreenCapturePPMThing();

    //! Initialization, including GLUT initialization.
    void Init( const std::vector<std::string>& args );
    
    // CALLBACK FUNCTIONS

    //! Cascading callback for initiating a display event
    void Display();

    //! Cascading callback for an idle  event 
    void solve();

    //! Keyboard to toggle capture on/off
    void Keyboard( unsigned char key, int x, int y );

    //! Cascading callback for reseting parameters
    void Reset();

    //! Cascading callback for usage information
    void Usage();


  private:

    int nx, ny;
    float *screen_capture;
    std::string base_file_name;
    int frame;

    bool record;

    std::vector<pba::PbaThing> things;
 
};



PbaThing CreateScreenCapturePPM(const std::string& f );





}
