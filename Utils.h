#ifndef _UTILS_H
#define _UTILS_H

#include <iostream>
#include <Windows.h>
#include <string.h> // memset
#include <math.h>

template <typename T>
struct _Vector2
{
	T x;
	T y;

	_Vector2(const T& x = 0, const T& y = 0) : x(x), y(y) {}
	template <typename U>
	_Vector2(U x = 0, U y = 0) : x(static_cast<T>(x)), y(static_cast<T>(y)) {}
	template <typename U>
	_Vector2(const _Vector2<U>& v) : x(static_cast<const T&>(v.x)), y(static_cast<const T&>(v.y))
	{}

	T sqrMagnitude() const { return x * x + y* y; }
	double magnitude() const { return sqrt(sqrMagnitude()); }
	double distance(const _Vector2& other)const {
		auto res = other - *this;
		return res.magnitude();
	}

	_Vector2 unit() const {
		if (equalApproximately({ 0.0f, 0.0f })) return _Vector2{ 1.0f, 0.0f };
		float mag = (float)magnitude();
		return _Vector2{ this->x / mag, this->y / mag }; // {sin¥è , cos¥è}
	}

	bool equalApproximately(const _Vector2& other) const;
	// operators
	auto operator+(const _Vector2& other) const {
		return _Vector2{ this->x + other.x , this->y + other.y };
	}
	auto operator-(const _Vector2& other) const {
		return _Vector2{ this->x - other.x , this->y - other.y };
	}
	auto operator*(float scale) const {
		return _Vector2{ this->x * scale , this->y * scale };
	}

	friend auto operator*(float scale, const _Vector2& v) {
		return v * scale;
	}

	const auto& operator=(const _Vector2& other) {
		this->x = other.x; this->y = other.y;
		return *this;
	}

	auto operator==(const _Vector2& other) const {
		return this->x == other.x && this->y == other.y;
	}

	const auto& operator+=(const _Vector2& other) {
		this->x = this->x + other.x; this->y = this->y + other.y;
		return *this;
	}

	_Vector2& operator++() { // pre
		this->x++;
		this->y++;
		return *this;
	}
	_Vector2& operator++(int unused) { // post
		_Vector2 temp = *this;
		this->x++;
		this->y++;
		return temp;
	}

	void print() const;
};
typedef _Vector2<float> Vector2;
typedef _Vector2<int> Position;
typedef Position Dimension;

class Borland {
	
public:
	static void Initialize() {
		std::system("mode con:cols=100 lines=60");
		std::system("chcp 437");
		CONSOLE_CURSOR_INFO cci;
		cci.dwSize = 25;
		cci.bVisible = FALSE;
		SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cci);
	}

	static int WhereY()
	{
		CONSOLE_SCREEN_BUFFER_INFO  csbiInfo;
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbiInfo);
		return csbiInfo.dwCursorPosition.Y;
	}
	static void GotoXY(int x, int y)
	{
		SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), _COORD{ (SHORT)x, (SHORT)y });
	}
	static void GotoXY(const Position* pos)
	{
		if (!pos) return;
		GotoXY((*pos).x, (*pos).y);
	}
	static void GotoXY(const Position& pos)
	{
		GotoXY(pos.x, pos.y);
	}
};

class Debug {
	static int nDebugLine;
	static char whiteSpaces[120];

public:
	static void Log(const char* fmt, ...);
};
#endif