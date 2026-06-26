/**
 * @file math_defs.hpp
 * @brief Zero-Dependency 3D/2D Vector and Matrix Math structures
 *
 * Implements lightweight vector (GazeVector2, GazeVector3), basis (GazeBasis3D),
 * and transform (GazeTransform3D) structs. Includes utilities for Euler angle
 * decompositions, Rodrigues rotation conversions, and gaze-space forward vector
 * computations.
 */
#pragma once

#include <cmath>

namespace Gaze {

static constexpr double PI = 3.14159265358979323846;
static constexpr double TAU = 6.28318530717958647692;
static constexpr double DEG_TO_RAD = PI / 180.0;
static constexpr double RAD_TO_DEG = 180.0 / PI;

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

    /**
     * @brief Computes the pitch and yaw (in degrees) of this direction vector.
     * 
     * Unlike standard spherical coordinates, this computes the yaw relative to a 
     * customizable reference direction (defaulting to the forward/negative-Z direction 
     * - GazeVector3(0, 0, -1) in OpenCV space) and normalizes the yaw 
     * difference into [-180, 180] degrees.
     */
    GazeVector2 get_pitch_yaw(const GazeVector3& relative = GazeVector3(0, 0, -1)) const {
        GazeVector3 n = normalized();
        double pitch_rad = std::asin(n.y);
        
        double yaw_rad = std::atan2(-n.x, -n.z);
        double rel_yaw_rad = std::atan2(-relative.x, -relative.z);
        double yaw_diff = yaw_rad - rel_yaw_rad;
        while (yaw_diff > PI) yaw_diff -= TAU;
        while (yaw_diff < -PI) yaw_diff += TAU;

        return GazeVector2(
            pitch_rad * RAD_TO_DEG,
            yaw_diff * RAD_TO_DEG
        );
    }
};

struct GazeBasis3D {
    GazeVector3 x;
    GazeVector3 y;
    GazeVector3 z;

    GazeBasis3D() : x(1, 0, 0), y(0, 1, 0), z(0, 0, 1) {}
    GazeBasis3D(const GazeVector3& px, const GazeVector3& py, const GazeVector3& pz) : x(px), y(py), z(pz) {}

    GazeVector3 multiply_vector(const GazeVector3& v) const {
        return x * v.x + y * v.y + z * v.z;
    }

    GazeBasis3D operator*(const GazeBasis3D& other) const {
        return GazeBasis3D(
            multiply_vector(other.x),
            multiply_vector(other.y),
            multiply_vector(other.z)
        );
    }

    static GazeBasis3D from_euler_zyx(double pitch_deg, double yaw_deg, double roll_deg) {
        double p = pitch_deg * DEG_TO_RAD;
        double y = yaw_deg * DEG_TO_RAD;
        double r = roll_deg * DEG_TO_RAD;
        double cp = std::cos(p), sp = std::sin(p);
        double cy = std::cos(y), sy = std::sin(y);
        double cr = std::cos(r), sr = std::sin(r);
        return GazeBasis3D(
            GazeVector3(cr * cy, sr * cy, -sy),
            GazeVector3(cr * sy * sp - sr * cp, sr * sy * sp + cr * cp, cy * sp),
            GazeVector3(cr * sy * cp + sr * sp, sr * sy * cp - cr * sp, cy * cp)
        );
    }

    /**
     * @brief Decomposes the basis into ZYX Euler angles (pitch, yaw, roll in degrees).
     * 
     * NON-STANDARD BEHAVIOR:
     * Choosing a standard Euler decomposition for a face pointing towards the camera results 
     * in yaw being close to 180 degrees. At this boundary, standard ZYX decompositions (which 
     * restrict yaw to [-90, 90] degrees) experience severe gimbal-lock/sign-flipping discontinuities.
     * This function is specialized to force the decomposition to select the branch where 
     * cos(yaw) < 0 (yaw centered around 180 degrees), ensuring smooth angles during direct-to-camera tracking.
     */
    GazeVector3 get_euler_deg() const {
        double sy = std::sqrt(x.x * x.x + x.y * x.y);
        bool singular = sy < 1e-6;
        double pitch = 0.0, yaw = 0.0, roll = 0.0;
        if (!singular) {
            // Since the face points towards the camera, yaw is around 180 degrees.
            // This means cos(yaw) < 0. We decompose choosing the branch where cos(yaw) is negative.
            pitch = std::atan2(-y.z, -z.z) * RAD_TO_DEG;
            yaw   = std::atan2(-x.z, -sy) * RAD_TO_DEG;
            roll  = std::atan2(-x.y, -x.x) * RAD_TO_DEG;
        } else {
            pitch = std::atan2(-y.y, y.x) * RAD_TO_DEG;
            yaw   = std::atan2(-x.z, -sy) * RAD_TO_DEG;
            roll  = 0.0;
        }
        return GazeVector3(pitch, yaw, roll);
    }

    /**
     * @brief Decomposes the basis into standard ZYX Euler angles (pitch, yaw, roll in degrees).
     * 
     * Standard ZYX decomposition that restricts the yaw to [-90, 90] degrees (choosing the 
     * branch where cos(yaw) > 0). Used specifically for feeding inputs to the ONNX gaze model 
     * which expects a standard coordinate convention (yaw centered around 0 degrees).
     */
    GazeVector3 get_euler_gaze_model_deg() const {
        double sy = std::sqrt(x.x * x.x + x.y * x.y);
        bool singular = sy < 1e-6;
        double pitch = 0.0, yaw = 0.0, roll = 0.0;
        if (!singular) {
            pitch = std::atan2(y.z, z.z) * RAD_TO_DEG;
            yaw   = std::atan2(-x.z, sy) * RAD_TO_DEG;
            roll  = std::atan2(x.y, x.x) * RAD_TO_DEG;
        } else {
            pitch = std::atan2(-y.y, y.x) * RAD_TO_DEG;
            yaw   = std::atan2(-x.z, sy) * RAD_TO_DEG;
            roll  = 0.0;
        }
        return GazeVector3(pitch, yaw, roll);
    }
};

struct GazeTransform3D {
    GazeBasis3D basis;
    GazeVector3 origin;

    GazeTransform3D() : basis(), origin() {}
    GazeTransform3D(const GazeBasis3D& b, const GazeVector3& o) : basis(b), origin(o) {}

    GazeTransform3D operator*(const GazeTransform3D& other) const {
        return GazeTransform3D(
            basis * other.basis,
            basis.multiply_vector(other.origin) + origin
        );
    }
};

/**
 * @brief Converts a 3D rotation vector (Rodrigues vector) to a 3x3 rotation basis.
 * 
 * Commonly known in graphics/game math as the Axis-Angle to Matrix conversion (where the 
 * vector's direction is the axis of rotation and its magnitude is the angle in radians). 
 * Named "rodrigues" to match the OpenCV cv::Rodrigues convention since it is used to parse 
 * head pose rotations returned by OpenCV's estimator.
 */
inline GazeBasis3D rodrigues_to_basis(const GazeVector3& r) {
    double theta = r.length();
    if (theta < 1e-6) {
        return GazeBasis3D();
    }
    double ux = r.x / theta;
    double uy = r.y / theta;
    double uz = r.z / theta;

    double s = std::sin(theta);
    double c = std::cos(theta);
    double oc = 1.0 - c;

    return GazeBasis3D(
        GazeVector3(c + oc * ux * ux,      oc * ux * uy + s * uz,  oc * ux * uz - s * uy),
        GazeVector3(oc * ux * uy - s * uz,  c + oc * uy * uy,      oc * uy * uz + s * ux),
        GazeVector3(oc * ux * uz + s * uy,  oc * uy * uz - s * ux,  c + oc * uz * uz)
    );
}


inline GazeVector3 get_head_forward_in_camera_space(const GazeVector3& rotation_deg) {
    double pitch_rad = rotation_deg.x * DEG_TO_RAD;
    double yaw_rad = rotation_deg.y * DEG_TO_RAD;
    double roll_rad = rotation_deg.z * DEG_TO_RAD;

    double cp = std::cos(pitch_rad), sp = std::sin(pitch_rad);
    double cy = std::cos(yaw_rad), sy = std::sin(yaw_rad);
    double cr = std::cos(roll_rad), sr = std::sin(roll_rad);

    double r02 = cr * sy * cp + sr * sp;
    double r12 = sr * sy * cp - cr * sp;
    double r22 = cy * cp;

    return GazeVector3(-r02, r12, r22);
}

} // namespace Gaze
