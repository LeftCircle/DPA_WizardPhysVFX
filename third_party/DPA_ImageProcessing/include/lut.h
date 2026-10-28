#ifndef LUT_H
#define LUT_H


#include <vector>

template <typename T>
class LUT {
public:
	LUT(std::initializer_list<T> values) : _table(values) {}
	~LUT() {}

	/* 
	Given a value from 0 -> 1, returns the lerped value of the two 
	surrounding LUT items that this is between. 
	IE, if the LUT only has two items, then a value of 0.5 will return
	the lerp between the two values. 
	*/
	T lerp(float val) const {
		val = std::max(0.0f, std::min(1.0f, val));

		float scaled = val * (_table.size() - 1);
		int index = static_cast<int>(scaled);
		float t = scaled - index;
		int next_index = index == _table.size() - 1 ? 0 : index + 1;

		return _table[index] * (1.0f - t) + _table[next_index] * t;
	}

	T lerp(double val) const {
		val = std::max(0.0, std::min(1.0, val));

		double scaled = val * (_table.size() - 1);
		size_t index = static_cast<size_t>(scaled);
		double t = scaled - index;
		int next_index = index == _table.size() - 1 ? 0 : index + 1;

		return _table[index] * (1.0f - t) + _table[next_index] * t;
	}



private:
	std::vector<T> _table;
};

#endif