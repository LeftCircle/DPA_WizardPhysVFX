#ifndef _THE_WHEEL_
#define _THE_WHEEL_

// Because some times you have to reinvent it

#define REINVENTING_THE_WHEEL
#ifdef REINVENTING_THE_WHEEL
// for some reason my IDE doesn't like checking the cpp version based on the makefile, hence ^^
//#if __cplusplus < 202002L 

#include <stddef.h> // for size_t
#include <type_traits> // for std::enable_if
#include <vector>

namespace pba{

template <typename T>
class span {
public:
	span(T* data, size_t size) : _data(data), _size(size) {};
	
	// the constructors for std::vectors
	// for span<T>
	template <typename U = T,
		std::enable_if_t<!std::is_const<U>::value, bool> = true>
	span(std::vector<U>& vec) : _data(vec.data()), _size(vec.size()) {};

	// for span<const T>
	template <typename U,
		std::enable_if_t<std::is_same<T, const U>::value, bool> = true>
	span(const std::vector<U>& vec) : _data(vec.data()), _size(vec.size()) {}; 

	// Conversion from span<t> to span<const T>
	template <typename U,
		std::enable_if_t<std::is_same<T, const U>::value, bool> = true>
	span(const span<U>& other) : _data(other.data()), _size(other.size()){};


	T& operator[](size_t i) const { return _data[i]; }
	size_t size() const { return _size; }

	T* begin() const { return _data; }
	T* end() const { return _data + _size; }
	T* data() const { return _data; }

private:
	// We want a raw pointer here because a span is a lightweight
	// non owning view into the vector
	T* _data;
	size_t _size;
};

// template <typename T>
// inline T clamp(T val, T low, T high){
// 	return (val < low) ? low : (val > high) ? high : val;
// }



}// end namespace cato


// template <typename T>
// using span = cato::span<T>;

// //using cato::clamp;

// #else
// #include <span>
// template <typename T>
// using span = std::span<T>;

// #include <algorithm>
// template <typename T>
// using clamp = std::clamp<T>;

#endif






#endif