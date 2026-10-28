//-------------------------------------------------------
//
//  PbaThing.C
//
//  Generic container for things that work
//  in cooperation with the PbaViewer
//
//  Copyright (c) 2017 Jerry Tessendorf
//
//
//--------------------------------------------------------


#include "PbaThing.h"
#include <iostream>
#include <sstream>


using namespace std;

using namespace pba;



PbaThingyDingy::PbaThingyDingy( const std::string& nam ) : 
   viewer (0),
   visible (true), 
   dt      (1.0/24.0), 
   name    (nam), 
   animate (true)
   {}


void PbaThingyDingy::Keyboard( unsigned char key, int x, int y )
{
   if( key == ' ' )
   { 
      animate = !animate; 
      if( !animate )
      { 
         std::cout << "STOP\n"; 
      } 
      else 
      { 
         std::cout << "START\n"; 
      }
   }
   if( key == 't' ){ dt /= 1.1; std::cout << "time step " << dt << std::endl; }
   if( key == 'T' ){ dt *= 1.1; std::cout << "time step " << dt << std::endl; }
}

void PbaThingyDingy::Idle(){ if( animate ){solve();} }

void PbaThingyDingy::Usage()
{
   std::cout << "=== PbaThing ===\n";
   std::cout << "SPACEBAR     start/stop animation\n";
   std::cout << "t/T          reduce/increase animation time step\n";
}

std::map<std::string,std::string> PbaThingyDingy::MetaData() const
{
   std::stringstream dtstream;
   dtstream << dt;
   std::map<std::string,std::string> md;
   md[name+":dt"] = dtstream.str(); 
   return md;
}
