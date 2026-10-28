
#include "PbaViewer.h"
#include "ScreenCapturePPM.h"
#include "MyThing.h"
#include "thing_bouncing_ball.h"
#include "thing_sph.h"
#include "thing_soft_bunny.h"
#include "thing_rigid_body.h"
#include "thing_boids.h"
#include "thing_colliding_particles_and_rbd.h"

int main(int argc, char** argv)
{
   // Set up command line arguments, if any
   std::vector<std::string> args;
   for(int i=0;i<argc;i++)
   {
      args.push_back( argv[i] );
   }


   // Instantiate a viewer
   pba::PbaViewer* viewer = pba::CreateViewer();

  
   // Set up a simulation thing
   //pba::PbaThing mything = pba::CreateMyThing();
   //viewer->AddThing(mything);
   //pba::PbaThing mything = pba::create_bouncing_ball_thing("Bouncing ball thing");
   //pba::PbaThing mything = pba::create_sph_thingy_dingy("Sph thing");
   //pba::PbaThing mything = pba::create_soft_bunny_thing("Soft Bunny Sim");
	//pba::PbaThing mything = pba::create_rigid_body_thing("Rigid Body Thing");  
   //pba::PbaThing mything = pba::create_colliding_particles_rbd_thing("Colliding particles and RBD Thing");
   pba::PbaThing mything = pba::create_boid_thingy_dingy("Boid Thing");
   viewer->AddThing(mything);
   //Optional thing to capture the screen each window and write it to a file
   pba::PbaThing imager = pba::CreateScreenCapturePPM("./" + mything->Name());
   viewer->AddThing(imager);


   // Initialize viewer
   viewer->Init(args);
   // Run the (GLUT) main loop
   viewer->MainLoop();

}
