//-------------------------------------------------------
//
//  PbaUtils.h
//
//  Functions to assist the python interface, etc.
//
//  Copyright (c) 2017 Jerry Tessendorf
//
//
//--------------------------------------------------------

#ifndef __PBA_PBAUTILS_H__
#define __PBA_PBAUTILS_H__

#include "CollisionSurface.h"
#include "PbaThing.h"




using namespace std;

namespace pba{


void AddCollisionSurface( pba::CollisionSurface& s, pba::PbaThing& p ); 

void Display( pba::CollisionSurface& s );




};


#endif


