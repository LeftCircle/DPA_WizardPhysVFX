//-------------------------------------------------------
//
//  MyThing.h
//
//  PbaThing for a collection of particles
//  each doing a random walk.
//
//  Copyright (c) 2017 Jerry Tessendorf
//
//
//--------------------------------------------------------


#include "Vector.h"
#include "Color.h"
#include "PbaThing.h"



using namespace std;

namespace pba{





class MyThing: public PbaThingyDingy
{
  public:

    // Feel free to customize the name of this thing.
    MyThing(const std::string nam = "Lecture_01_thing");
   ~MyThing();

    //! Initialization, including GLUT initialization.
    //! Called once at the beginning.  Could be used
    //! to set up things once.
    void Init( const std::vector<std::string>& args );
   
    /////////////////////////////////////////////////////////////// 
    // CASCADING CALLBACK FUNCTIONS 
    // The methods below are called as part of a bigger set
    // of similar calls.  Most of the other calls take place
    // in the viewer portion of this project.
    ///////////////////////////////////////////////////////////////

    //! Implements a display event
    //! This is where you code the opengl calls to display 
    //! your system.
    void Display();

    //! Implements responses to keyboard events 
    //! This is called when you hit a key
    void Keyboard( unsigned char key, int x, int y );

    //! Implements simulator updates during an idle period
    //! This is where the update process is coded
    //! for your dynamics problem.
    void solve();

    //! Implements reseting parameters and/or state
    //! This is called when you hit the 'r' key
    void Reset();

    //! Displays usage information on stdout
    //! If you set up actions with the Keyboard()
    //! callback, you should include a statement 
    //! here as to what the keyboard option is.
    void Usage();

  private:

    // flag for whether to create more particles
    bool emit;
   
    ////////////////////////////////////////////////
    //
    //      PARTICLE STATE
    //
    // The state of a particle is characterized by
    // (1) particle positions
    // (2) particle velocities
    // (3) particle masses
    // (4) particle colors - useful for display
    class ParticleState
    {
      public:
        ParticleState() :
         position(Vector(0,0,0)),
	 velocity(Vector(0,0,0)),
	 color(Color(1,1,1,1)),
	 mass(1.0)
	{};
       ~ParticleState(){};	   

       Vector position;
       Vector velocity;
       Color color;
       float mass;
    };

    // This is all of the particles in the system
    std::vector<ParticleState> particles;
    //
    //
    //////////////////////////////////////////////// 

};


// This function constructs the MyThing and wraps it in a 
// smart pointer called a PbaThing. 
// You need not alter this.
pba::PbaThing CreateMyThing();








}





