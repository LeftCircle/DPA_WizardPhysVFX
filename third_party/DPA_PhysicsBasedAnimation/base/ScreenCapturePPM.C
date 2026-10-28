//-------------------------------------------------------
//
//  ScreenCapturePPM.C
//
//  PbaThing for ScreenCapturePPM
//
//  Copyright (c) 2017 Jerry Tessendorf
//
//
//--------------------------------------------------------


#include <GL/gl.h>   // OpenGL itself.
#include <GL/glu.h>  // GLU support library.
#include <GL/glut.h> // GLUT support library.

#include <iostream>
#include <sstream>
#include <fstream>



#include "ScreenCapturePPM.h"


using namespace pba;



ScreenCapturePPMThing::ScreenCapturePPMThing(const std::string& fname, const std::string nam) :
   PbaThingyDingy (nam),
   nx(0),
   ny(0),
   screen_capture (0),
   base_file_name (fname),
   frame(0),
   record (false)
{
   std::cout << name << " constructed\n";
}

ScreenCapturePPMThing::~ScreenCapturePPMThing()
{
   if(screen_capture){ delete[] screen_capture; }
}

void ScreenCapturePPMThing::Init( const std::vector<std::string>& args )
{
   Reset();
}
    
void ScreenCapturePPMThing::Display()
{
   frame += 1;
   if(record)
   {
      std::stringstream ss;
      ss << frame;
      std::string fname = "./" + base_file_name;
      fname.append(std::string(".")).append(std::string( 4-ss.str().length(), '0')).append(ss.str()).append(".ppm");
      std::cout << "Recoding Frame " << frame << std::endl; 
      glReadPixels(0,0, nx, ny, GL_RGB, GL_FLOAT, screen_capture );
      // vertical flip
      for( int i=0;i<nx;i++ )
      {
         for( int j=0;j<=ny/2;j++ )
         {
            int ii = (i + nx*j)*3;
            int ii2 = (i + nx*(ny-j-1))*3;
            float tempr = screen_capture[ii];
            float tempg = screen_capture[ii+1];
            float tempb = screen_capture[ii+2];
            screen_capture[ii] = screen_capture[ii2];
            screen_capture[ii+1] = screen_capture[ii2+1];
            screen_capture[ii+2] = screen_capture[ii2+2];
            screen_capture[ii2] = tempr;
            screen_capture[ii2+1] = tempg;
            screen_capture[ii2+2] = tempb;
         }
      }
          
      ofstream fout;
      fout.open(fname.c_str());
      fout << "P3\n";
      fout << nx << " " << ny << "\n" << 255 << "\n";
      for( int j=0;j<ny;j++ )
      {
         for( int i=0;i<nx;i++ )
         {
            int ii = (i + nx*j)*3;
            int red, green, blue;
            red = (int)(255.0*screen_capture[ii]);
            green = (int)(255.0*screen_capture[ii+1]);
            blue = (int)(255.0*screen_capture[ii+2]);
            fout << red << " " << green << " " << blue << "\n";
         }
      }
      fout.close();
   }
}

void ScreenCapturePPMThing::solve(){}

void ScreenCapturePPMThing::Keyboard( unsigned char key, int x, int y )
{
   if( key == '.' )
   {
      record = !record;
      if(record)
      {
         std::cout << "Recording to files " << base_file_name << ".XXXX.ppm\n";
      }
      if(!record)
      {
         std::cout << "Stopped recording.\n";
      }
   }
}



void ScreenCapturePPMThing::Reset()
{ 
   nx = glutGet(GLUT_WINDOW_WIDTH);
   ny = glutGet(GLUT_WINDOW_HEIGHT);
   if(screen_capture){ delete[] screen_capture; }
   screen_capture = new float[3*nx*ny];
   frame = 0;
}

void ScreenCapturePPMThing::Usage()
{
   cout << "=== " << name << " ===\n";
   cout << ". (period)   toggle frame capture on/off\n";
}





pba::PbaThing pba::CreateScreenCapturePPM(const std::string& f ){ return PbaThing( new pba::ScreenCapturePPMThing(f) ); }






