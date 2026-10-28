//-------------------------------------------------------
//
//  MyThing.C
//
//  PbaThing for a collection of particles
//  each doing a random walk.
//
//  Copyright (c) 2017 Jerry Tessendorf
//
//
//--------------------------------------------------------

#include "MyThing.h"
#include <cstdlib>
#include <GL/gl.h>   // OpenGL itself.
#include <GL/glu.h>  // GLU support library.
#include <GL/glut.h> // GLUT support library.
#include <iostream>



using namespace std;

using namespace pba;





MyThing::MyThing(const std::string nam) :
 PbaThingyDingy (nam),
 emit       (false)
{
    Reset();
    std::cout << name << " constructed\n";
}

MyThing::~MyThing(){}

void MyThing::Init( const std::vector<std::string>& args ) {}
    
void MyThing::Display() 
{
   glPointSize(5.0);
   glBegin(GL_POINTS);
   for( size_t i=0;i<particles.size();i++ )
   {
      const Vector& P = particles[i].position;
      const Color& ci = particles[i].color;
      glColor3f( ci.red(), ci.green(), ci.blue() );
      glVertex3f( P.X(), P.Y(), P.Z() );
   }
   glEnd();
}

void MyThing::Keyboard( unsigned char key, int x, int y )
{
       PbaThingyDingy::Keyboard(key,x,y);
       if( key == 'e' ){ emit = !emit; }
}


void MyThing::solve()
{
   // This is where the particle state - position and velocity - are updated
   // in several steps.

   // Step 1: Advance the particle positions in time
   for(size_t i=0;i<particles.size();i++)
   {
      particles[i].position += particles[i].velocity * dt;
   }

   //////////////////////////////////////////////////////////////////////////////////////////
   //
   //           THIS PART IS NOT A TYPICAL VELOCITY UPDATE
   //
   // Next the velocity is updated.  For most situations, this update comes
   // from forces on the particles. Here we do a simple, non-force, example.

   // Step 2.1: find the center
   Vector center;
   for(size_t i=0;i<particles.size();i++)
   {
      center += particles[i].position;
   }
   center = center/particles.size();

   // Step 2.2: update velocities to be perpendicular to the line from the particle to the center
   for(size_t i=0;i<particles.size();i++)
   {
      Vector n = particles[i].position - center;
      n.normalize();  // make it a unit vector
      double vmag = particles[i].velocity.magnitude();
      particles[i].velocity -= n*(n*particles[i].velocity);
      particles[i].velocity *= vmag/particles[i].velocity.magnitude(); // keep it the same magnitude
   }
   //
   //
   //////////////////////////////////////////////////////////////////////////////////////////////////

   // This concludes the solver action
   //
   //
   //
   //
   // This is where we can add more particles
   if(emit)
   {
      size_t nbincrease = 10;
      particles.resize(particles.size()+nbincrease);
      Vector P, V;
      Color C;
      std::cout << "Total Points " << particles.size() << std::endl;
      for(size_t i=particles.size()-nbincrease;i<particles.size();i++)
      {
         double s = 2.0*drand48() - 1.0;
         double ss = std::sqrt( 1.0 - s*s );
         double theta = 2.0*3.14159265*drand48();
         pba::Vector P( ss*std::cos(theta), s, ss*std::sin(theta) );
         P *= std::pow( drand48(), 1.0/6.0 );
         particles[i].position = P;
         particles[i].color = pba::Color(drand48(),drand48(),drand48(),0);
         particles[i].velocity = pba::Vector(drand48()-0.5,drand48()-0.5,drand48()-0.5);
      }
   }
}

void MyThing::Reset()
{
   // Distribute particles with random positions
   particles.clear();
   particles.resize(20);
   for(size_t i=0;i<particles.size();i++)
   {
      double s = 2.0*drand48() - 1.0;
      double ss = std::sqrt( 1.0 - s*s );
      double theta = 2.0*3.14159265*drand48();
      pba::Vector P( ss*std::cos(theta), s, ss*std::sin(theta) );
      P *= std::pow( drand48(), 1.0/6.0 );
      particles[i].position = P;
      particles[i].color = pba::Color(drand48(),drand48(),drand48(),0);
      particles[i].velocity = pba::Vector(drand48()-0.5,drand48()-0.5,drand48()-0.5);
   }
}

void MyThing::Usage()
{
   PbaThingyDingy::Usage();
   cout << "=== " << name << " ===\n";
   cout << "e            toggle particle emission on/off\n";
}


pba::PbaThing pba::CreateMyThing(){ return PbaThing( new MyThing() ); }


