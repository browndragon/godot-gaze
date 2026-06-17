// Zero-dependency 3D/2D vector and matrix math structures.
#pragma once

#include <cmath>

namespace Gaze {

struct GazeVector2 {
    double x = 0.0;
    double y = 0.0;

    GazeVector2() : x(0.0), y(0.0) {}
    GazeVector2(double px, double py) : x(px), y(py) {}
};

struct GazeVector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    GazeVector3() : x(0.0), y(0.0), z(0.0) {}
    GazeVector3(double px, double py, double pz) : x(px), y(py), z(pz) {}

    GazeVector3 operator+(const GazeVector3& other) const {
        return GazeVector3(x + other.x, y + other.y, z + other.z);
    }

    GazeVector3 operator-(const GazeVector3& other) const {
        return GazeVector3(x - other.x, y - other.y, z - other.z);
    }

    GazeVector3 operator*(double scalar) const {
        return GazeVector3(x * scalar, y * scalar, z * scalar);
    }

    double dot(const GazeVector3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    GazeVector3 cross(const GazeVector3& other) const {
        return GazeVector3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    double length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    GazeVector3 normalized() const {
        double len = length();
        if (len > 0.0) {
            return GazeVector3(x / len, y / len, z / len);
        }
        return GazeVector3(0.0, 0.0, 0.0);
    }
};

} // namespace Gaze
