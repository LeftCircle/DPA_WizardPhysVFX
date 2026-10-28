#ifndef COLOR_H
#define COLOR_H

struct Color{
	float r, g, b = 0.0f;
	Color() {r = 0.0f; g = 0.0f; b = 0.0f; };
	Color(float r_, float g_, float b_) {r = r_; g = g_; b = b_; }
	Color(float val) {r = val; g = val; b = val; }

	// overload the plus operator
	Color operator+(const Color& other) const {
		return Color(r + other.r, g + other.g, b + other.b);
	}
	Color operator/(const Color& other) const {
		return Color(r / other.r, g / other.g, b / other.b);
	}

	Color operator/(const float f) const {
		return Color(r / f, g / f, b / f);
	}

	bool operator==(const Color& other) const noexcept {
		return (r == other.r && g == other.g && b == other.b);
	}

	bool operator!=(const Color& other) const noexcept {
		return (r != other.r || g != other.g || b != other.b);
	}

	Color operator*(const float t) const {
		return Color(r * t, g * t, b * t);
	}
};


#endif