
%module bishop
%{
#include "ProgressMeter.h"
%}


namespace lux
{

class ProgressMeter
{
  public:

    ProgressMeter( long nb, char * ttl ) { ProgressMeter( nb, string(ttl) ); }
    void update();
};

}
