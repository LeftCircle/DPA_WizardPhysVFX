
#include "volume.h"
#include <iostream>

using namespace lux;
using namespace std;

const std::vector< std::vector<double> > FDGradCoefficientGenerator()
{
       std::vector<double> coeffs;
       std::vector< std::vector<double> > grad_coefficients;
       // n = 1
       coeffs.push_back( 0.5 );
       grad_coefficients.push_back(coeffs);
       coeffs.clear();

       // n = 2
       coeffs.push_back( 6.666666666666666296592e-01 );
       coeffs.push_back( -8.333333333333332870740e-02 );
       grad_coefficients.push_back(coeffs);
       coeffs.clear();

       // n = 3
       coeffs.push_back(  7.500000000000002220446e-01 );
       coeffs.push_back( -1.500000000000001332268e-01 );
       coeffs.push_back( 1.666666666666666296592e-02 );
       grad_coefficients.push_back(coeffs);
       coeffs.clear();

       // n = 4
       coeffs.push_back( 7.999999999999992672528e-01 );
       coeffs.push_back( -2.000000000000004829470e-01 );
       coeffs.push_back( 3.809523809523807785782e-02 );
       coeffs.push_back( -3.571428571428579123309e-03 );
       grad_coefficients.push_back(coeffs);
       coeffs.clear();

       // n = 5
       coeffs.push_back( 8.333333333333301506940e-01 );
       coeffs.push_back( -2.380952380952371660872e-01 );
       coeffs.push_back( 5.952380952380949968861e-02 );
       coeffs.push_back( -9.920634920635062331540e-03 );
       coeffs.push_back(7.936507936507907227594e-04 );
       grad_coefficients.push_back(coeffs);
       coeffs.clear();

       // n = 6
       coeffs.push_back( 8.571428571428428844214e-01 );
       coeffs.push_back( -2.678571428571295820475e-01 );
       coeffs.push_back( 7.936507936506739802063e-02  );
       coeffs.push_back( -1.785714285714170776465e-02 );
       coeffs.push_back( 2.597402597401952516892e-03 );
       coeffs.push_back( -1.803751803751641390964e-04 );
       grad_coefficients.push_back(coeffs);
       coeffs.clear();

       // n = 7
       coeffs.push_back( 8.749999999997666311202e-01 );
       coeffs.push_back( -2.916666666664030627132e-01 );
       coeffs.push_back( 9.722222222201161445643e-02 );
       coeffs.push_back( -2.651515151521397634093e-02 );
       coeffs.push_back( 5.303030303006155132817e-03 );
       coeffs.push_back( -6.798756798778969601127e-04 );
       coeffs.push_back( 4.162504162505081361893e-05 );
       grad_coefficients.push_back(coeffs);
       coeffs.clear();

       // n = 8
       coeffs.push_back( 8.888888888890622563821e-01 );
       coeffs.push_back( -3.111111111113007976492e-01 );
       coeffs.push_back( 1.131313131315020842349e-01 );
       coeffs.push_back( -3.535353535356308696258e-02 );
       coeffs.push_back( 8.702408702345020702351e-03 );
       coeffs.push_back( -1.554001554001562023302e-03 );
       coeffs.push_back( 1.776001776006136932424e-04 );
       coeffs.push_back( -9.712509712526897105722e-06 );
       grad_coefficients.push_back(coeffs);
       coeffs.clear();

       // n = 9
       coeffs.push_back( 8.999999999973695707922e-01 );
       coeffs.push_back( -3.272727272682948718163e-01 );
       coeffs.push_back( 1.272727272692814326494e-01 );
       coeffs.push_back( -4.405594405380468259192e-02 );
       coeffs.push_back( 1.258741258753500423528e-02 );
       coeffs.push_back( -2.797202796916069093835e-03 );
       coeffs.push_back( 4.495504494691525475096e-04 );
       coeffs.push_back( -4.627725215224719976801e-05 );
       coeffs.push_back( 2.285296402912368231850e-06 );
       grad_coefficients.push_back(coeffs);
       coeffs.clear();

       // n = 10
       coeffs.push_back( 9.090909090725343144612e-01 );
       coeffs.push_back( -3.409090908829451871398e-01 );
       coeffs.push_back( 1.398601398464504319552e-01 );
       coeffs.push_back( -5.244755243406638844927e-02 );
       coeffs.push_back( 1.678321677751935456224e-02 );
       coeffs.push_back( -4.370629367345699872738e-03 );
       coeffs.push_back( 8.814714692904230272999e-04 );
       coeffs.push_back( -1.285479226007764982226e-04 );
       coeffs.push_back( 1.202787579588283707100e-05 );
       coeffs.push_back( -5.412544109527078402898e-07 );
       grad_coefficients.push_back(coeffs);
       coeffs.clear();

       // n = 11
       coeffs.push_back( 9.166666665040884565130e-01 );
       coeffs.push_back( -3.525641023000493645689e-01 );
       coeffs.push_back( 1.510989006669366252478e-01 );
       coeffs.push_back( -6.043956026669643211147e-02 );
       coeffs.push_back( 2.115384606316183732644e-02 );
       coeffs.push_back( -6.221719517138330109163e-03 );
       coeffs.push_back( 1.481361764454423206663e-03 );
       coeffs.push_back( -2.728824305737073381908e-04 );
       coeffs.push_back( 3.638432414571888845901e-05 );
       coeffs.push_back( -3.118656339922096169245e-06 );
       coeffs.push_back( 1.288700981451494452140e-07 );
       grad_coefficients.push_back(coeffs);
       coeffs.clear();

       // n = 12
       coeffs.push_back( 9.230769225029789026848e-01 );
       coeffs.push_back( -3.626373617033492591233e-01 );
       coeffs.push_back( 1.611721583806974000819e-01 );
       coeffs.push_back( -6.799450484120614368599e-02 );
       coeffs.push_back( 2.559793110528202006448e-02 );
       coeffs.push_back( -8.295625777082871188384e-03 );
       coeffs.push_back( 2.245432612438852566783e-03 );
       coeffs.push_back( -4.911883622080408370522e-04 );
       coeffs.push_back( 8.316416682614131267431e-05 );
       coeffs.push_back( -1.020651206170876503291e-05 );
       coeffs.push_back( 8.068388089184792311508e-07 );
       coeffs.push_back( -3.081676248363273868301e-08 );
       grad_coefficients.push_back(coeffs);
       coeffs.clear();

       // n = 13
       coeffs.push_back( 9.285714267400464461133e-01 );
       coeffs.push_back( -3.714285685139248061049e-01 );
       coeffs.push_back( 1.702380751958524618406e-01 );
       coeffs.push_back( -7.510503985705131724249e-02 );
       coeffs.push_back( 3.004201550432860148843e-02 );
       coeffs.push_back( -1.054105789791678036982e-02 );
       coeffs.push_back( 3.162317968689574727154e-03 );
       coeffs.push_back( -7.905793074257984895739e-04 );
       coeffs.push_back( 1.597129844416282369157e-04 );
       coeffs.push_back( -2.499855371995233626345e-05 );
       coeffs.push_back( 2.840742476654620619051e-06 );
       coeffs.push_back( -2.083212542482251737132e-07 );
       coeffs.push_back( 7.396022670499950410872e-09 );
       grad_coefficients.push_back(coeffs);
       coeffs.clear();

       return grad_coefficients;
}

    //std::vector< std::vector<double> > FDGradHandler::_grad_coefficients =  FDGradCoefficientGenerator();
    //std::vector< std::vector<double> > FDGradHandler::_grad_coefficients;

FDGradHandler::FDGradHandler() :
    _nb_grad(1),
    _dx(0.1),
    _dy(0.1),
    _dz(0.1)
    {
       _grad_coefficients =  FDGradCoefficientGenerator();
    }
 



template<>
const Vector lux::FDGradient( const float& x, const float& y, const float& z )
{
      float xx = x;
      float yy = y;
      float zz = z;
    //   if( isnan(x) ){ cout << "Gradient NAN x\n"; xx = 0.0; }
    //   if( isnan(y) ){ cout << "Gradient NAN y\n"; yy = 0.0; }
    //   if( isnan(z) ){ cout << "Gradient NAN z\n"; zz = 0.0; }
    if ( isnan(x)) xx = 0.0f;
    if ( isnan(y)) yy = 0.0f;
    if ( isnan(z)) zz = 0.0f;
      
    return Vector(xx,yy,zz); 
}

template<>
const Matrix lux::FDGradient( const Vector& x, const Vector& y, const Vector& z )
{
      Matrix m(x,y,z); 
      return m.transpose();
}





