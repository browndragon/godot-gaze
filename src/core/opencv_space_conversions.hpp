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
using SpacedVector3 = Spaced<GazeVector3, S>;

template <Space S>
using SpacedVector2 = Spaced<GazeVector2, S>;

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

    /**
     * @brief Transforms OpenVINO ADAS raw Cartesian gaze output vector (+x user right, +y up, +z forward)
     * into Godot Camera Space (+X camera right, +Y up, +Z forward towards screen plane).
     * @param gaze_openvino Raw vector from gaze-estimation-adas-0002 model.
     * @return GazeVector3 Vector in Godot Camera Space.
     */
    inline GazeVector3 openvino_gaze_to_godot_cam(const GazeVector3 &gaze_openvino)
    {
        return ONNX_GAZE_TO_GODOT_CAM.multiply_vector(gaze_openvino);
    }

    /**
     * @brief Converts OpenCV PnP Rodrigues rotation vector into OpenVINO ADAS Euler angles [yaw, pitch, roll] in degrees.
     *
     * In OpenCV model space (with canonical face geometry), the Rodrigues vector components directly express
     * head rotation in radians relative to camera image plane (where 0 is facing straight into camera):
     * - cv_rvec.y = Yaw (Turn head: viewer left / subject right is positive, viewer right / subject left is negative)
     * - cv_rvec.x = Pitch (Nod head: down is positive, up is negative)
     * - cv_rvec.z = Roll (Tilt head: right shoulder is positive, left shoulder is negative)
     *
     * @param cv_rvec Rodrigues rotation vector in OpenCV head space.
     * @return GazeVector3 containing (yaw_deg, pitch_deg, roll_deg).
     */
    inline GazeVector3 opencv_head_pose_to_openvino_angles_deg(const GazeVector3 &cv_rvec)
    {
        return GazeVector3(
            -cv_rvec.y * RAD_TO_DEG, // Yaw: OpenVINO expects negative yaw for right turn
             cv_rvec.x * RAD_TO_DEG, // Pitch: OpenVINO expects positive pitch for downward pitch
            -cv_rvec.z * RAD_TO_DEG  // Roll
        );
    }

    /**
     * @brief Converts OpenCV PnP pose (translation and Rodrigues rotation) to Godot Camera Space Transform.
     * @param cv_translation Translation vector in OpenCV camera space.
     * @param cv_rvec Rodrigues rotation vector in OpenCV model/camera space.
     * @return GazeTransform3D expressing Godot Face Local Space in Godot Camera Space.
     */
    inline GazeTransform3D opencv_pose_to_godot_camera_transform(
        const GazeVector3 &cv_translation,
        const GazeVector3 &cv_rvec)
    {
        GazeBasis3D R_cv = rodrigues_to_basis(cv_rvec);
        GazeBasis3D R_godot = OPENCV_CAM_TO_GODOT_CAM * R_cv * GODOT_FACE_TO_OPENCV_FACE;
        GazeVector3 origin_godot = OPENCV_CAM_TO_GODOT_CAM.multiply_vector(cv_translation);
        return GazeTransform3D(R_godot, origin_godot);
    }

    /**
     * @brief Maps a transform from GodotCameraHintRolled to GodotCamera space by applying roll_hint around Z.
     * @param transform_hint_rolled Face transform in hint-rolled camera space.
     * @param roll_hint_rad Roll hint angle in radians.
     * @return GazeTransform3D in unrolled GodotCamera space.
     */
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
        // In GodotCamera space (+X Right, +Y Up, +Z towards screen):
        // Rotation by +roll_hint_rad around viewing axis:
        // col 0 = (cos_a, -sin_a, 0), col 1 = (sin_a, cos_a, 0), col 2 = (0, 0, 1)
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

