/**
 * @file opencv_space_conversions.hpp
 * @brief Strongly-Typed Coordinate Space Definitions and Transformations
 *
 * Single Source of Truth for reference frame transformations across:
 * 1. GodotCamera Space: +X Camera Right (Display Right), +Y Up, -Z Forward into room towards user
 * 2. GodotFace Space: +X User's Right Ear (Display Left), +Y Top of Head, -Z Forward out of face
 * 3. GodotDisplayPx Space: 2D px: Origin Top-Left (0,0), +X Right, +Y Down
 * 4. OpenCVCamera Space: +X Right (Display Right), +Y Down, +Z Away into scene
 * 5. OpenCVFaceModel Space: +X User's Left Ear (Image Right), +Y Down towards Chin, +Z Back into skull
 * 6. OpenVINOADASGaze Space: +X User's Right Ear (Display Left), +Y Up, +Z Forward towards camera
 */
#pragma once

#include "math_defs.hpp"
#include <cmath>
#include <limits>

namespace Gaze
{

/**
 * @brief Canonical Coordinate Space Identifiers.
 */
enum class Space
{
    GodotCamera,                   ///< Canonical 3D Hub: +X Display Left (Camera Right), +Y Up, -Z Forward into room / +Z towards screen
    GodotFaceLocal,                ///< Canonical Head Local: +X User's Right Ear (Display Left), +Y Top of Head, -Z Forward out of face
    GodotCameraHintRolled,         ///< Canonical 3D Hint-Rolled: Space::GodotCamera rotated by -roll_hint around Z
    GodotCameraEuler,              ///< 3D Euler angles (pitch, yaw, roll in rad) in GodotCamera frame
    GodotCameraImagePixels,        ///< Canonical 2D Sensor Pixels: (u, v) in raw camera image
    GodotCameraWorkingImagePixels, ///< 2D Sensor Pixels: (u, v) in hint-rolled working image
    GodotDisplayPx,                ///< Canonical 2D Display Pixels: Origin Top-Left (0,0), +X Right (0..W_px), +Y Down (0..H_px)
    GodotDisplayMm,                ///< Canonical 2D Display Millimeters: Origin Top-Left (0,0), +X Right (0..W_mm), +Y Down (0..H_mm)
    GodotViewportPx,               ///< Canonical 2D Viewport Pixels: Origin Top-Left (0,0), +X Right, +Y Down
    OpenCVCamera,                  ///< Spoke Model Internal: 3D mm: +X Right, +Y Down, +Z Away from camera into scene
    OpenCVCameraHintRolled,        ///< Spoke Model Internal: 3D mm with roll hint applied
    OpenCVFaceModel,               ///< Spoke Model Internal: 3D mm: +X User's Left Ear, +Y Down towards chin, +Z Back into skull
    OpenVINOADASGaze               ///< Spoke Model Internal: 3D mm: +X User's Right Ear (Display Left), +Y Up, +Z Forward towards camera
};

template <Space S>
struct SpacedVector3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    constexpr SpacedVector3() = default;
    constexpr SpacedVector3(double px, double py, double pz) : x(px), y(py), z(pz) {}

    constexpr explicit operator bool() const { return x != 0.0 || y != 0.0 || z != 0.0; }

    constexpr bool operator==(const SpacedVector3<S> &other) const {
        return x == other.x && y == other.y && z == other.z;
    }
    constexpr bool operator!=(const SpacedVector3<S> &other) const {
        return !(*this == other);
    }

    constexpr SpacedVector3<S> operator-() const { return SpacedVector3<S>(-x, -y, -z); }

    constexpr SpacedVector3<S> operator+(const SpacedVector3<S> &other) const {
        return SpacedVector3<S>(x + other.x, y + other.y, z + other.z);
    }
    constexpr SpacedVector3<S> operator-(const SpacedVector3<S> &other) const {
        return SpacedVector3<S>(x - other.x, y - other.y, z - other.z);
    }
    constexpr SpacedVector3<S> operator*(double scalar) const {
        return SpacedVector3<S>(x * scalar, y * scalar, z * scalar);
    }
    constexpr SpacedVector3<S> operator/(double scalar) const {
        return SpacedVector3<S>(x / scalar, y / scalar, z / scalar);
    }

    constexpr double dot(const SpacedVector3<S> &other) const {
        return x * other.x + y * other.y + z * other.z;
    }
    constexpr SpacedVector3<S> cross(const SpacedVector3<S> &other) const {
        return SpacedVector3<S>(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    double length_squared() const { return dot(*this); }
    double length() const { return std::sqrt(length_squared()); }
    SpacedVector3<S> normalized() const {
        double len = length();
        return len > 1e-6 ? (*this / len) : SpacedVector3<S>(0.0, 0.0, 0.0);
    }

    constexpr bool is_finite() const {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }

    static constexpr SpacedVector3<S> infinite() {
        return SpacedVector3<S>(
            std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity()
        );
    }
};

template <Space S>
inline constexpr SpacedVector3<S> operator*(double scalar, const SpacedVector3<S> &v) {
    return v * scalar;
}

template <Space S>
inline constexpr double dot(const SpacedVector3<S> &a, const SpacedVector3<S> &b) {
    return a.dot(b);
}

template <Space S>
inline constexpr SpacedVector3<S> cross(const SpacedVector3<S> &a, const SpacedVector3<S> &b) {
    return a.cross(b);
}

template <Space S>
struct SpacedVector2
{
    double x = 0.0;
    double y = 0.0;

    constexpr SpacedVector2() = default;
    constexpr SpacedVector2(double px, double py) : x(px), y(py) {}

    constexpr explicit operator bool() const { return x != 0.0 || y != 0.0; }

    constexpr bool operator==(const SpacedVector2<S> &other) const {
        return x == other.x && y == other.y;
    }
    constexpr bool operator!=(const SpacedVector2<S> &other) const {
        return !(*this == other);
    }

    constexpr SpacedVector2<S> operator-() const { return SpacedVector2<S>(-x, -y); }

    constexpr SpacedVector2<S> operator+(const SpacedVector2<S> &other) const {
        return SpacedVector2<S>(x + other.x, y + other.y);
    }
    constexpr SpacedVector2<S> operator-(const SpacedVector2<S> &other) const {
        return SpacedVector2<S>(x - other.x, y - other.y);
    }
    constexpr SpacedVector2<S> operator*(double scalar) const {
        return SpacedVector2<S>(x * scalar, y * scalar);
    }
    constexpr SpacedVector2<S> operator/(double scalar) const {
        return SpacedVector2<S>(x / scalar, y / scalar);
    }

    constexpr double dot(const SpacedVector2<S> &other) const {
        return x * other.x + y * other.y;
    }
    double length_squared() const { return dot(*this); }
    double length() const { return std::sqrt(length_squared()); }
    SpacedVector2<S> normalized() const {
        double len = length();
        return len > 1e-6 ? (*this / len) : SpacedVector2<S>(0.0, 0.0);
    }
};

template <Space S>
inline constexpr SpacedVector2<S> operator*(double scalar, const SpacedVector2<S> &v) {
    return v * scalar;
}

template <Space S>
inline constexpr double dot(const SpacedVector2<S> &a, const SpacedVector2<S> &b) {
    return a.dot(b);
}

template <Space From, Space To>
struct SpacedBasis
{
    SpacedVector3<To> x{1.0, 0.0, 0.0};
    SpacedVector3<To> y{0.0, 1.0, 0.0};
    SpacedVector3<To> z{0.0, 0.0, 1.0};

    constexpr SpacedBasis() = default;
    constexpr SpacedBasis(const SpacedVector3<To> &px, const SpacedVector3<To> &py, const SpacedVector3<To> &pz)
        : x(px), y(py), z(pz) {}

    static constexpr SpacedBasis<From, To> identity() {
        return SpacedBasis<From, To>(
            SpacedVector3<To>(1.0, 0.0, 0.0),
            SpacedVector3<To>(0.0, 1.0, 0.0),
            SpacedVector3<To>(0.0, 0.0, 1.0)
        );
    }

    SpacedVector3<To> transform(const SpacedVector3<From> &v) const {
        return SpacedVector3<To>(
            x.x * v.x + y.x * v.y + z.x * v.z,
            x.y * v.x + y.y * v.y + z.y * v.z,
            x.z * v.x + y.z * v.y + z.z * v.z
        );
    }

    SpacedVector3<To> multiply_vector(const SpacedVector3<From> &v) const {
        return transform(v);
    }

    SpacedBasis<To, From> transposed() const {
        return SpacedBasis<To, From>(
            SpacedVector3<From>(x.x, y.x, z.x),
            SpacedVector3<From>(x.y, y.y, z.y),
            SpacedVector3<From>(x.z, y.z, z.z)
        );
    }

    double determinant() const {
        return x.x * (y.y * z.z - y.z * z.y) -
               x.y * (y.x * z.z - y.z * z.x) +
               x.z * (y.x * z.y - y.y * z.x);
    }

    SpacedVector3<Space::GodotCameraEuler> get_euler_rad() const {
        double sy = std::sqrt(x.x * x.x + x.y * x.y);
        bool singular = sy < 1e-6;
        double p, y_ang, r;
        if (z.z < 0.0 && x.x < 0.0) {
            if (!singular) {
                p = std::atan2(-y.z, -z.z);
                y_ang = std::atan2(-x.z, -sy);
                r = std::atan2(-x.y, -x.x);
            } else {
                p = std::atan2(-y.y, y.x);
                y_ang = std::atan2(-x.z, -sy);
                r = 0.0;
            }
        } else {
            if (!singular) {
                p = std::atan2(y.z, z.z);
                y_ang = std::atan2(-x.z, sy);
                r = std::atan2(x.y, x.x);
            } else {
                p = std::atan2(-z.y, y.y);
                y_ang = std::atan2(-x.z, sy);
                r = 0.0;
            }
        }
        return SpacedVector3<Space::GodotCameraEuler>(p, y_ang, r);
    }
};

template <Space A, Space B, Space C>
inline SpacedBasis<A, C> operator*(const SpacedBasis<B, C> &m1, const SpacedBasis<A, B> &m2) {
    return SpacedBasis<A, C>(
        m1.transform(m2.x),
        m1.transform(m2.y),
        m1.transform(m2.z)
    );
}

template <Space From, Space To>
struct SpacedTransform3D
{
    SpacedBasis<From, To> basis;
    SpacedVector3<To> origin;

    constexpr SpacedTransform3D() = default;
    constexpr SpacedTransform3D(const SpacedBasis<From, To> &b, const SpacedVector3<To> &o)
        : basis(b), origin(o) {}

    static constexpr SpacedTransform3D<From, To> identity() {
        return SpacedTransform3D<From, To>(
            SpacedBasis<From, To>::identity(),
            SpacedVector3<To>(0.0, 0.0, 0.0)
        );
    }

    SpacedVector3<To> transform_point(const SpacedVector3<From> &p) const {
        return basis.transform(p) + origin;
    }
};

template <Space A, Space B, Space C>
inline SpacedTransform3D<A, C> operator*(const SpacedTransform3D<B, C> &t1, const SpacedTransform3D<A, B> &t2) {
    return SpacedTransform3D<A, C>(
        t1.basis * t2.basis,
        t1.transform_point(t2.origin)
    );
}

template <Space From, Space To>
inline SpacedBasis<From, To> rodrigues_to_basis(const SpacedVector3<To> &r) {
    double theta = r.length();
    if (theta < 1e-6) {
        return SpacedBasis<From, To>::identity();
    }
    double kx = r.x / theta;
    double ky = r.y / theta;
    double kz = r.z / theta;
    double c = std::cos(theta);
    double s = std::sin(theta);
    double v = 1.0 - c;

    return SpacedBasis<From, To>(
        SpacedVector3<To>(kx * kx * v + c,      kx * ky * v + kz * s,  kx * kz * v - ky * s),
        SpacedVector3<To>(kx * ky * v - kz * s, ky * ky * v + c,       ky * kz * v + kx * s),
        SpacedVector3<To>(kx * kz * v + ky * s, ky * kz * v - kx * s,  kz * kz * v + c)
    );
}

template <Space From, Space To>
inline SpacedVector3<To> basis_to_rodrigues(const SpacedBasis<From, To> &b) {
    double trace = b.x.x + b.y.y + b.z.z;
    double cos_theta = 0.5 * (trace - 1.0);
    if (cos_theta > 1.0) cos_theta = 1.0;
    else if (cos_theta < -1.0) cos_theta = -1.0;
    double theta = std::acos(cos_theta);

    if (theta < 1e-6) {
        double scale = 0.5;
        return SpacedVector3<To>(
            scale * (b.y.z - b.z.y),
            scale * (b.z.x - b.x.z),
            scale * (b.x.y - b.y.x)
        );
    }
    double scale = 0.5 * theta / std::sin(theta);
    return SpacedVector3<To>(
        scale * (b.y.z - b.z.y),
        scale * (b.z.x - b.x.z),
        scale * (b.x.y - b.y.x)
    );
}

/**
 * @brief Canonical Hub and Spoke Type Aliases.
 */
using GodotCameraVector3 = SpacedVector3<Space::GodotCamera>;
using GodotFaceVector3 = SpacedVector3<Space::GodotFaceLocal>;
using GodotCameraImageVector2 = SpacedVector2<Space::GodotCameraImagePixels>;
using GodotDisplayVector2 = SpacedVector2<Space::GodotDisplayPx>;
using GodotFaceTransform3D = SpacedTransform3D<Space::GodotFaceLocal, Space::GodotCamera>;
using OpenCVCameraVector3 = SpacedVector3<Space::OpenCVCamera>;
using OpenCVFaceVector3 = SpacedVector3<Space::OpenCVFaceModel>;
using OpenVINOGazeVector3 = SpacedVector3<Space::OpenVINOADASGaze>;

template <Space S>
inline SpacedVector2<S> rotate_point_2d(const SpacedVector2<S> &pt, double angle_rad, double img_w, double img_h)
{
    if (std::abs(angle_rad) < 1e-6) return pt;
    double cx = img_w * 0.5;
    double cy = img_h * 0.5;
    double cos_a = std::cos(angle_rad);
    double sin_a = std::sin(angle_rad);
    double dx = pt.x - cx;
    double dy = pt.y - cy;
    return SpacedVector2<S>(
        cx + dx * cos_a - dy * sin_a,
        cy + dx * sin_a + dy * cos_a
    );
}

inline GodotCameraVector3 apply_3d_bias_vector(
    const GodotCameraVector3 &dir,
    const SpacedVector2<Space::GodotCameraEuler> &bias_pitch_yaw,
    const SpacedVector2<Space::GodotCameraEuler> &scale_pitch_yaw = SpacedVector2<Space::GodotCameraEuler>(1.0, 1.0))
{
    double pitch_rad = std::asin(static_cast<double>(dir.y));
    double yaw_rad = std::atan2(static_cast<double>(dir.x), static_cast<double>(dir.z));

    double biased_pitch = (pitch_rad + bias_pitch_yaw.x) * scale_pitch_yaw.x;
    double biased_yaw = (yaw_rad + bias_pitch_yaw.y) * scale_pitch_yaw.y;

    double cos_pitch = std::cos(biased_pitch);
    return GodotCameraVector3(
        std::sin(biased_yaw) * cos_pitch,
        std::sin(biased_pitch),
        std::cos(biased_yaw) * cos_pitch
    ).normalized();
}

/**
 * @brief Projects a 3D ray in GodotCamera space onto the camera's coplanar plane (Z = 0).
 *
 * In GodotCamera space:
 * - Origin (0,0,0) is at camera optical center.
 * - +X is Camera Right (User Left), +Y is Camera Up, -Z is toward user.
 * - +Z is toward screen plane.
 *
 * If the ray points away from the plane (dir_cam.z <= 1e-6) or originates behind the plane (t < 0),
 * returns an infinite vector (is_finite() == false).
 */
inline GodotCameraVector3 project_ray_to_camera_plane(
    const GodotCameraVector3 &origin_cam,
    const GodotCameraVector3 &dir_cam)
{
    if (dir_cam.z <= 1e-6)
    {
        return GodotCameraVector3::infinite();
    }

    double t = -origin_cam.z / dir_cam.z;
    if (t < 0.0)
    {
        return GodotCameraVector3::infinite();
    }

    return GodotCameraVector3(
        origin_cam.x + t * dir_cam.x,
        origin_cam.y + t * dir_cam.y,
        0.0
    );
}

namespace CoordinateConversions
{

    inline const SpacedBasis<Space::OpenCVCamera, Space::GodotCamera> OPENCV_CAM_TO_GODOT_CAM(
        GodotCameraVector3( 1.0,  0.0,  0.0),
        GodotCameraVector3( 0.0, -1.0,  0.0),
        GodotCameraVector3( 0.0,  0.0, -1.0)
    );

    inline const SpacedBasis<Space::GodotFaceLocal, Space::OpenCVFaceModel> GODOT_FACE_TO_OPENCV_FACE(
        OpenCVFaceVector3(-1.0,  0.0, 0.0),
        OpenCVFaceVector3( 0.0, -1.0, 0.0),
        OpenCVFaceVector3( 0.0,  0.0, 1.0)
    );

    inline const SpacedBasis<Space::OpenVINOADASGaze, Space::GodotCamera> ONNX_GAZE_TO_GODOT_CAM(
        GodotCameraVector3( 1.0, 0.0,  0.0),
        GodotCameraVector3( 0.0, 1.0,  0.0),
        GodotCameraVector3( 0.0, 0.0, -1.0)
    );

    inline GodotCameraVector3 to_godot_camera(const OpenCVCameraVector3 &cv_vec)
    {
        return OPENCV_CAM_TO_GODOT_CAM.transform(cv_vec);
    }

    inline GodotCameraVector3 to_godot_camera(const OpenVINOGazeVector3 &onnx_gaze)
    {
        return ONNX_GAZE_TO_GODOT_CAM.transform(onnx_gaze);
    }

    inline OpenCVFaceVector3 to_opencv_face(const GodotFaceVector3 &godot_face_pt)
    {
        return GODOT_FACE_TO_OPENCV_FACE.transform(godot_face_pt);
    }

    inline GodotFaceVector3 to_godot_face(const OpenCVFaceVector3 &cv_face_pt)
    {
        return GODOT_FACE_TO_OPENCV_FACE.transposed().transform(cv_face_pt);
    }

    inline SpacedVector3<Space::OpenCVCamera> opencv_head_pose_to_openvino_angles_deg(const OpenCVCameraVector3 &cv_rvec)
    {
        return SpacedVector3<Space::OpenCVCamera>(
            -cv_rvec.y * RAD_TO_DEG,
             cv_rvec.x * RAD_TO_DEG,
            -cv_rvec.z * RAD_TO_DEG
        );
    }

    inline GodotFaceTransform3D opencv_pose_to_godot_camera_transform(
        const OpenCVCameraVector3 &cv_translation,
        const OpenCVCameraVector3 &cv_rvec)
    {
        SpacedBasis<Space::OpenCVFaceModel, Space::OpenCVCamera> R_cv = rodrigues_to_basis<Space::OpenCVFaceModel, Space::OpenCVCamera>(cv_rvec);
        SpacedBasis<Space::GodotFaceLocal, Space::GodotCamera> R_godot = OPENCV_CAM_TO_GODOT_CAM * R_cv * GODOT_FACE_TO_OPENCV_FACE;
        GodotCameraVector3 origin_godot = OPENCV_CAM_TO_GODOT_CAM.transform(cv_translation);
        return GodotFaceTransform3D(R_godot, origin_godot);
    }

    inline GodotFaceTransform3D godot_camera_hint_rolled_to_godot_camera(
        const GodotFaceTransform3D &transform_hint_rolled,
        float roll_hint_rad)
    {
        if (std::abs(roll_hint_rad) < 1e-5f)
        {
            return transform_hint_rolled;
        }
        float cos_a = std::cos(roll_hint_rad);
        float sin_a = std::sin(roll_hint_rad);
        SpacedBasis<Space::GodotCamera, Space::GodotCamera> R_roll(
            GodotCameraVector3(cos_a, -sin_a, 0.0),
            GodotCameraVector3(sin_a,  cos_a, 0.0),
            GodotCameraVector3( 0.0,   0.0, 1.0)
        );
        SpacedBasis<Space::GodotFaceLocal, Space::GodotCamera> R_cam = R_roll * transform_hint_rolled.basis;
        GodotCameraVector3 t_cam = R_roll.transform(transform_hint_rolled.origin);
        return GodotFaceTransform3D(R_cam, t_cam);
    }

    inline GodotFaceTransform3D godot_camera_hint_rolled_to_godot_camera(
        const SpacedTransform3D<Space::GodotFaceLocal, Space::GodotCameraHintRolled> &transform_hint_rolled,
        float roll_hint_rad)
    {
        if (std::abs(roll_hint_rad) < 1e-5f)
        {
            return GodotFaceTransform3D(
                SpacedBasis<Space::GodotFaceLocal, Space::GodotCamera>(
                    GodotCameraVector3(transform_hint_rolled.basis.x.x, transform_hint_rolled.basis.x.y, transform_hint_rolled.basis.x.z),
                    GodotCameraVector3(transform_hint_rolled.basis.y.x, transform_hint_rolled.basis.y.y, transform_hint_rolled.basis.y.z),
                    GodotCameraVector3(transform_hint_rolled.basis.z.x, transform_hint_rolled.basis.z.y, transform_hint_rolled.basis.z.z)
                ),
                GodotCameraVector3(transform_hint_rolled.origin.x, transform_hint_rolled.origin.y, transform_hint_rolled.origin.z)
            );
        }
        float cos_a = std::cos(roll_hint_rad);
        float sin_a = std::sin(roll_hint_rad);
        SpacedBasis<Space::GodotCameraHintRolled, Space::GodotCamera> r_basis(
            GodotCameraVector3(cos_a, -sin_a, 0.0),
            GodotCameraVector3(sin_a,  cos_a, 0.0),
            GodotCameraVector3( 0.0,   0.0, 1.0)
        );
        SpacedBasis<Space::GodotFaceLocal, Space::GodotCamera> R_cam = r_basis * transform_hint_rolled.basis;
        GodotCameraVector3 t_cam = r_basis.transform(transform_hint_rolled.origin);
        return GodotFaceTransform3D(R_cam, t_cam);
    }

} // namespace CoordinateConversions
} // namespace Gaze

