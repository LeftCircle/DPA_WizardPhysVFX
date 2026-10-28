#ifndef _DYNAMICAL_STATE_ATTRIBUTE
#define _DYNAMICAL_STATE_ATTRIBUTE

#include <vector>

#include "the_wheel.h"
#include "Vector.h"
#include "Color.h"

// modified from https://jtessen.people.clemson.edu/cpsc6190/html/_dynamical_state_8h_source.html

namespace pba{
template<typename T>
class DSAttribute
{
public:
	DSAttribute() : name("unknown") {}
	DSAttribute( const std::string& nam, const T& def ) : name(nam), defVal(def) {}
	~DSAttribute(){}

	const size_t size() const { return data.size(); }
	const bool empty() const { return data.empty(); }
	void set(size_t i, const T& value ) { data[i] = value; }
	const T& get(size_t i ) const { return data[i]; }
	T& get(size_t i ) { return data[i]; }

	auto get_span() { return span<T>(data); }
	auto get_span() const { return span<const T>(data); }

	void expand_to( size_t n )
	{
        if( data.size() >= n ){ resize_to_defaults(n); return; }
        size_t old_size = data.size();
        data.resize(n);
        for( size_t i=old_size;i<data.size();i++ )
        {
            data[i] = defVal;
        }
	}

	void resize_to_defaults(size_t n) {
		data.resize(n, defVal);
	}

	void clear() { data.clear(); }
	const std::string& attr_name() const { return name; }
	const T& default_value() const { return defVal; }
	typename std::vector<T>::const_iterator cbegin() const { return data.begin(); }
	typename std::vector<T>::const_iterator cend() const { return data.end(); }
	typename std::vector<T>::iterator begin() { return data.begin(); }
	typename std::vector<T>::iterator end() { return data.end(); }


private:
	std::vector<T> data;
	std::string name;
	T defVal;
};

using DSAv = DSAttribute<Vector>;
using DSAi = DSAttribute<int>;
using DSAf = DSAttribute<float>;
using DSAc = DSAttribute<Color>;
using DSAd = DSAttribute<double>;

}; // end namespace pba


#endif