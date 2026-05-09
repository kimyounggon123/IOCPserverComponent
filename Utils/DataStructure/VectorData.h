#ifndef _DATATOCONNECTWITHCLIENT_H
#define _DATATOCONNECTWITHCLIENT_H
#include <cmath>

constexpr float PIf = 3.141592f;
constexpr double PId = 3.14159265358979323846;

template <typename T>
struct _VectorData
{
	T x, y, z;

	_VectorData(const T& x = 0.0f, const T& y = 0.0f, const T& z = 0.0f) : x(x), y(y), z(z){}
	_VectorData(const _VectorData<T>& other) : x(other.x), y(other.y), z(other.z)
	{}

	template <typename U>
	_VectorData(U x = 0, U y = 0) : x(static_cast<T>(x)), y(static_cast<T>(y)) {}
	template <typename U>
	_VectorData(const _VectorData<U>& v) : x(static_cast<const T&>(v.x)), y(static_cast<const T&>(v.y))
	{}

	// basic operators
	_VectorData<T>& operator=(const _VectorData<T>& other)
	{
		if (this != &other)
		{
			x = other.x;
			y = other.y;
			z = other.z;
		}
		return *this;
	}
	_VectorData<T> operator+(const _VectorData<T>& other) const { return { x + other.x, y + other.y, z + other.z }; }
	_VectorData<T> operator-(const _VectorData<T>& other) const { return { x - other.x, y - other.y, z - other.z }; }
	_VectorData<T> operator*(T scalar) const { return { x * scalar, y * scalar, z * scalar }; }

	_VectorData<T>& operator+=(const _VectorData<T>& other) { x += other.x; y += other.y; z += other.z; return *this; }
	_VectorData<T>& operator-=(const _VectorData<T>& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
	_VectorData<T>& operator*=(T scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }

	// basic calculations
	T lengthSquared() const { return x * x + y * y + z * z; }
	T length() const { return std::sqrt(lengthSquared()); }

	_VectorData<T> normalized() const
	{
		T len = length();
		if (len == static_cast<T>(0)) return *this;
		return { x / len, y / len, z / len }; // unit vector
	}

	T dot(const _VectorData<T>& other) const { return x * other.x + y * other.y + z * other.z; }
	_VectorData<T> cross(const _VectorData<T>& other) const
	{
		return {
			y * other.z - z * other.y,
			z * other.x - x * other.z,
			x * other.y - y * other.x
		};
	}
	T distance(const _VectorData<T>& other) const { return (*this - other).length(); }

	bool equalApproximately(const _VectorData<T>& other, T eps = static_cast<T>(1e-6)) 
		const { return (*this - other).lengthSquared() < eps * eps; }

	// 기능 추가할 수 있음!
};

using Vector3i = _VectorData<int>;
using Vector3f = _VectorData<float>;
using Vector3d = _VectorData<double>;
#endif