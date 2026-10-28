#ifndef _PBA_MATH_H
#define _PBA_MATH_H


namespace pba{

template<typename T>
T factorial(T val) {
    size_t n_iters = static_cast<size_t>(val) - 1;
    if (n_iters <= 0){
        return 1;
    }
    for (T i = n_iters; i > 0; i--){
        val *= i;
    }
    return val;
};

} // end namespace pba



#endif