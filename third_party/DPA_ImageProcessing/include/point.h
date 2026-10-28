#ifndef POINT_H
#define POINT_H

#include <cmath>

struct Point{
	double x, y;
	Point(double new_x, double new_y) { x = new_x; y = new_y; } 
	Point() {x = 0.0, y = 0.0; };

	void operator*=(double val) { x *= val, y *= val; }
	void operator*=(const Point& other) { x *= other.x, y *= other.y; }

	void operator/=(double val) { x /= val, y /= val; }
	void operator/=(const Point& other) { x /= other.x, y /= other.y; }

	void operator+=(const Point& other) { x += other.x, y += other.y; }
	void operator-=(const Point& other) { x -= other.x, y -= other.y; }

	Point operator+(const Point& other) const { return Point(x + other.x, y + other.y); }
	Point operator-(const Point& other) const { return Point(x - other.x, y - other.y); }

	Point operator*(const Point& other) const { return Point(x * other.x, y * other.y); }
	Point operator/(const Point& other) const { return Point(x / other.x, y / other.y); }

	double magnitude_sq() const {return x*x + y*y; }
	double magnitude() const {return std::sqrt(x*x + y*y); }


};

#endif