#pragma once

#include <cmath>

// 2D 벡터. 물리 계산의 안쪽 루프에서 매우 많이 불리므로
// 모든 함수를 헤더에 인라인으로 둔다.
class Vector2 {
public:
    double x;
    double y;
public:
    Vector2() : x(0.0), y(0.0) {}
    Vector2(double x, double y) : x(x), y(y) {}

    double length() const { return std::sqrt(x * x + y * y); }
    double length_squared() const { return x * x + y * y; }

    Vector2 normalized() const {
        double len = length();

        if(len == 0.0) return Vector2(0.0, 0.0);

        return Vector2(x / len, y / len);
    }

    Vector2 operator+(const Vector2& other) const { return Vector2(x + other.x, y + other.y); }
    Vector2 operator-(const Vector2& other) const { return Vector2(x - other.x, y - other.y); }
    Vector2 operator*(const double scalar) const { return Vector2(x * scalar, y * scalar); }
    Vector2 operator/(const double scalar) const { return Vector2(x / scalar, y / scalar); }

    Vector2& operator+=(const Vector2& other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    Vector2& operator-=(const Vector2& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    Vector2& operator*=(const double scalar) {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    Vector2& operator/=(const double scalar) {
        x /= scalar;
        y /= scalar;
        return *this;
    }

    Vector2 operator-() const { return Vector2(-x, -y); }

    void clear() {
        x = 0.0;
        y = 0.0;
    }
};
