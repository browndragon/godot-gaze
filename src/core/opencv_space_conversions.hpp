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

namespace Gaze
{

/**
 * @brief Canonical Coordinate Space Identifiers.
 */
enum class Space
{
    GodotCamera,            ///< Canonical 3D Hub: +X Display Left (Camera Right), +Y Up, -Z Forward into room / +Z towards screen
    GodotFaceLocal,         ///< Canonical Head Local: +X User's Right Ear (Display Left), +Y Top of Head, -Z Forward out of face
    GodotCameraHintRolled,  ///< Canonical 3D Hint-Rolled: Space::GodotCamera rotated by -roll_hint around Z
    GodotCameraImagePixels, ///< Canonical 2D Sensor Pixels: (u, v) in raw camera image
    ImagePixelsHintRolled,  ///< Canonical 2D Sensor Pixels: (u, v) in hint-rolled image
    GodotDisplayPx,         ///< Canonical 2D Display Pixels: Origin Top-Left (0,0), +X Right (0..W_px), +Y Down (0..H_px)
    OpenCVCamera,           ///< Spoke Model Internal: 3D mm: +X Right, +Y Down, +Z Away from camera into scene
    OpenCVFaceModel,        ///< Spoke Model Internal: 3D mm: +X User's Left Ear, +Y Down towards chin, +Z Back into skull
    OpenVINOADASGaze        ///< Spoke Model Internal: 3D mm: +X User's Right Ear (Display Left), +Y Up, +Z Forward towards camera
};

/**
 * @brief Generic wrapper tagging a data structure with its explicit Coordinate Space.
 */
template <typename T, Space S>
struct Spaced
{
    T value;

    Spaced() = default;
    explicit Spaced(const T &v) : value(v) {}

    const T *operator->() const { return &value; }
    T *operator->() { return &value; }
    const T &get() const { return value; }
    T &get() { return value; }

    bool operator==(const Spaced<T, S> &other) const { return value == other.value; }
    bool operator!=(const Spaced<T, S> &other) const { return value != other.value; }
};

template <Space S>
struct SpacedVector3
{
    GazeVector3 value;

    SpacedVector3() : value(0.0, 0.0, 0.0) {}
    explicit SpacedVector3(const GazeVector3 &v) : value(v) {}
    SpacedVector3(double x, double y, double z) : value(x, y, z) {}

    const GazeVector3 *operator->() const { return &value; }
    GazeVector3 *operator->() { return &value; }
    const GazeVector3 &get() const { return value; }
    GazeVector3 &get() { return value; }

    bool operator==(const SpacedVector3<S> &other) const { return value == other.value; }
    bool operator!=(const SpacedVector3<S> &other) const { return value != other.value; }

    SpacedVector3<S> operator+(const SpacedVector3<S> &other) const { return SpacedVector3<S>(value + other.value); }
    SpacedVector3<S> operator-(const SpacedVector3<S> &other) const { return SpacedVector3<S>(value - other.value); }
    SpacedVector3<S> operator*(double scalar) const { return SpacedVector3<S>(value * scalar); }
    SpacedVector3<S> operator/(double scalar) const { return SpacedVector3<S>(value * (1.0 / scalar)); }
    SpacedVector3<S> operator-() const { return SpacedVector3<S>(-value); }

    double length() const { return value.length(); }
    double length_squared() const { return value.dot(value); }
    SpacedVector3<S> normalized() const { return SpacedVector3<S>(value.normalized()); }
};

template <Space S>
inline double dot(const SpacedVector3<S> &a, const SpacedVector3<S> &b)
{
    return a.value.dot(b.value);
}

template <Space S>
inline SpacedVector3<S> cross(const SpacedVector3<S> &a, const SpacedVector3<S> &b)
{
    return SpacedVector3<S>(a.value.cross(b.value));
}

template <Space S>
inline SpacedVector3<S> operator*(double scalar, const SpacedVector3<S> &v)
{
    return v * scalar;
}

template <Space S>
struct SpacedVector2
{
    GazeVector2 value;

    SpacedVector2() : value(0.0, 0.0) {}
    explicit SpacedVector2(const GazeVector2 &v) : value(v) {}
    SpacedVector2(double x, double y) : value(x, y) {}

    const GazeVector2 *operator->() const { return &value; }
    GazeVector2 *operator->() { return &value; }
    const GazeVector2 &get() const { return value; }
    GazeVector2 &get() { return value; }

    bool operator==(const SpacedVector2<S> &other) const { return value == other.value; }
    bool operator!=(const SpacedVector2<S> &other) const { return value != other.value; }

    SpacedVector2<S> operator+(const SpacedVector2<S> &other) const { return SpacedVector2<S>(value + other.value); }
    SpacedVector2<S> operator-(const SpacedVector2<S> &other) const { return SpacedVector2<S>(value - other.value); }
    SpacedVector2<S> operator*(double scalar) const { return SpacedVector2<S>(value * scalar); }
    SpacedVector2<S> operator/(double scalar) const { return SpacedVector2<S>(value / scalar); }
    SpacedVector2<S> operator-() const { return SpacedVector2<S>(-value); }

    double length() const { return value.length(); }
    SpacedVector2<S> normalized() const {
        double len = value.length();
        return len > 1e-6 ? SpacedVector2<S>(value.x / len, value.y / len) : SpacedVector2<S>(0.0, 0.0);
    }
};

template <Space S>
inline SpacedVector2<S> operator*(double scalar, const SpacedVector2<S> &v)
{
    return v * scalar;
}

/**
 * @brief Type-safe basis matrix transforming 3D vectors from space 'From' to space 'To'.
 */
template <Space From, Space To>
struct SpacedBasis
{
    GazeBasis3D basis;

    SpacedBasis() = default;
    explicit SpacedBasis(const GazeBasis3D &b) : basis(b) {}

    SpacedVector3<To> transform(const SpacedVector3<From> &v) const
    {
        return SpacedVector3<To>(basis.multiply_vector(v.get()));
    }
};

/**
 * @brief Type-safe rigid 3D transform mapping points from space 'From' to space 'To'.
 */
template <Space From, Space To>
struct SpacedTransform3D
{
    SpacedBasis<From, To> basis;
    SpacedVector3<To> origin;

    SpacedTransform3D() = default;
    SpacedTransform3D(const SpacedBasis<From, To> &b, const SpacedVector3<To> &o)
        : basis(b), origin(o) {}

    SpacedVector3<To> transform_point(const SpacedVector3<From> &p) const
    {
        return SpacedVector3<To>(basis.basis.multiply_vector(p.get()) + origin.get());
    }
};

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

namespace CoordinateConversions
{

    /**
     * @brief Spoke-to-Hub Transformation matrix mapping OpenCV Camera Space (+X right, +Y down, +Z into scene)
     * to Godot Camera Space (+X display left, +Y up, +Z towards screen).
     * Basis: diag(-1, -1, -1)
     */
    inline const GazeBasis3D OPENCV_CAM_TO_GODOT_CAM = GazeBasis3D(
        GazeVector3( 1.0,  0.0,  0.0),
        GazeVector3( 0.0, -1.0,  0.0),
        GazeVector3( 0.0,  0.0, -1.0)
    );

    /**
     * @brief Spoke-to-Hub Transformation matrix mapping Godot Face Local Space (+X right ear, +Y up, -Z forward)
     * to OpenCV Face Model Space (+X left ear, +Y down, +Z into head).
     * Basis: diag(-1, -1, 1)
     */
    inline const GazeBasis3D GODOT_FACE_TO_OPENCV_FACE = GazeBasis3D(
        GazeVector3(-1.0,  0.0, 0.0),
        GazeVector3( 0.0, -1.0, 0.0),
        GazeVector3( 0.0,  0.0, 1.0)
    );

    /**
     * @brief Spoke-to-Hub Basis mapping OpenVINO ADAS Gaze Model Output Vector (+x user right / display left, +y up, -z forward)
     * to Godot Camera Space (+X display left, +Y up, +Z towards screen plane).
     * Basis: diag(1, 1, -1).
     */
    inline const GazeBasis3D ONNX_GAZE_TO_GODOT_CAM = GazeBasis3D(
        GazeVector3( 1.0, 0.0,  0.0),
        GazeVector3( 0.0, 1.0,  0.0),
        GazeVector3( 0.0, 0.0, -1.0)
    );

    inline GodotCameraVector3 to_godot_camera(const OpenCVCameraVector3 &cv_vec)
    {
        return GodotCameraVector3(OPENCV_CAM_TO_GODOT_CAM.multiply_vector(cv_vec.get()));
    }

    inline GodotCameraVector3 to_godot_camera(const OpenVINOGazeVector3 &onnx_gaze)
    {
        return GodotCameraVector3(ONNX_GAZE_TO_GODOT_CAM.multiply_vector(onnx_gaze.get()));
    }

    inline OpenCVFaceVector3 to_opencv_face(const GodotFaceVector3 &godot_face_pt)
    {
        return OpenCVFaceVector3(GODOT_FACE_TO_OPENCV_FACE.multiply_vector(godot_face_pt.get()));
    }

    inline GodotFaceVector3 to_godot_face(const OpenCVFaceVector3 &cv_face_pt)
    {
        return GodotFaceVector3(GODOT_FACE_TO_OPENCV_FACE.multiply_vector(cv_face_pt.get()));
    }

    inline GazeVector3 openvino_gaze_to_godot_cam(const GazeVector3 &gaze_openvino)
    {
        return ONNX_GAZE_TO_GODOT_CAM.multiply_vector(gaze_openvino);
    }

    inline GazeVector3 opencv_head_pose_to_openvino_angles_deg(const GazeVector3 &cv_rvec)
    {
        return GazeVector3(
            -cv_rvec.y * RAD_TO_DEG, // Yaw: OpenVINO expects negative yaw for right turn
             cv_rvec.x * RAD_TO_DEG, // Pitch: OpenVINO expects positive pitch for downward pitch
            -cv_rvec.z * RAD_TO_DEG  // Roll
        );
    }

    inline GodotFaceTransform3D opencv_pose_to_godot_camera_transform(
        const OpenCVCameraVector3 &cv_translation,
        const OpenCVCameraVector3 &cv_rvec)
    {
        GazeBasis3D R_cv = rodrigues_to_basis(cv_rvec.get());
        GazeBasis3D R_godot = OPENCV_CAM_TO_GODOT_CAM * R_cv * GODOT_FACE_TO_OPENCV_FACE;
        GodotCameraVector3 origin_godot(OPENCV_CAM_TO_GODOT_CAM.multiply_vector(cv_translation.get()));
        return GodotFaceTransform3D(SpacedBasis<Space::GodotFaceLocal, Space::GodotCamera>(R_godot), origin_godot);
    }

    inline GazeTransform3D opencv_pose_to_godot_camera_transform(
        const GazeVector3 &cv_translation,
        const GazeVector3 &cv_rvec)
    {
        GazeBasis3D R_cv = rodrigues_to_basis(cv_rvec);
        GazeBasis3D R_godot = OPENCV_CAM_TO_GODOT_CAM * R_cv * GODOT_FACE_TO_OPENCV_FACE;
        GazeVector3 origin_godot = OPENCV_CAM_TO_GODOT_CAM.multiply_vector(cv_translation);
        return GazeTransform3D(R_godot, origin_godot);
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
        GazeBasis3D R_roll(
            GazeVector3(cos_a, -sin_a, 0.0f),
            GazeVector3(sin_a,  cos_a, 0.0f),
            GazeVector3( 0.0f,   0.0f, 1.0f)
        );
        GazeBasis3D R_cam = R_roll * transform_hint_rolled.basis.basis;
        GodotCameraVector3 t_cam(R_roll.multiply_vector(transform_hint_rolled.origin.get()));
        return GodotFaceTransform3D(SpacedBasis<Space::GodotFaceLocal, Space::GodotCamera>(R_cam), t_cam);
    }

    inline GodotFaceTransform3D godot_camera_hint_rolled_to_godot_camera(
        const SpacedTransform3D<Space::GodotFaceLocal, Space::GodotCameraHintRolled> &transform_hint_rolled,
        float roll_hint_rad)
    {
        if (std::abs(roll_hint_rad) < 1e-5f)
        {
            return GodotFaceTransform3D(
                SpacedBasis<Space::GodotFaceLocal, Space::GodotCamera>(transform_hint_rolled.basis.basis),
                GodotCameraVector3(transform_hint_rolled.origin.get())
            );
        }
        float cos_a = std::cos(roll_hint_rad);
        float sin_a = std::sin(roll_hint_rad);
        GazeBasis3D R_roll(
            GazeVector3(cos_a, -sin_a, 0.0f),
            GazeVector3(sin_a,  cos_a, 0.0f),
            GazeVector3( 0.0f,   0.0f, 1.0f)
        );
        GazeBasis3D R_cam = R_roll * transform_hint_rolled.basis.basis;
        GodotCameraVector3 t_cam(R_roll.multiply_vector(transform_hint_rolled.origin.get()));
        return GodotFaceTransform3D(SpacedBasis<Space::GodotFaceLocal, Space::GodotCamera>(R_cam), t_cam);
    }

    inline GazeTransform3D godot_camera_hint_rolled_to_godot_camera(
        const GazeTransform3D &transform_hint_rolled,
        float roll_hint_rad)
    {
        if (std::abs(roll_hint_rad) < 1e-5f)
        {
            return transform_hint_rolled;
        }
        float cos_a = std::cos(roll_hint_rad);
        float sin_a = std::sin(roll_hint_rad);
        GazeBasis3D R_roll(
            GazeVector3(cos_a, -sin_a, 0.0f),
            GazeVector3(sin_a,  cos_a, 0.0f),
            GazeVector3( 0.0f,   0.0f, 1.0f)
        );
        GazeBasis3D R_cam = R_roll * transform_hint_rolled.basis;
        GazeVector3 t_cam = R_roll.multiply_vector(transform_hint_rolled.origin);
        return GazeTransform3D(R_cam, t_cam);
    }

} // namespace CoordinateConversions
} // namespace Gaze

