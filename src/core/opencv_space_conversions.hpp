/**
 * @file opencv_space_conversions.hpp
 * @brief Coordinate Space Transformations between OpenCV PnP Model Space and Godot Camera Space
 *
 * Defines exact mathematical transformations at the boundary between OpenCV-style PnP solvers
 * and Godot's standard Camera and Face local reference frames.
 *
 * Coordinate Systems:
 * 1. OpenCV Camera Space:
 *    - +X: Right
 *    - +Y: Down
 *    - +Z: Forward (away from camera into scene)
 *
 * 2. OpenCV Face Model Space:
 *    - +X: Subject's Anatomical Left (Viewer's Right)
 *    - +Y: Down towards Chin
 *    - +Z: Back into skull (Nose tip at Z=0 or negative Z)
 *
 * 3. Godot Camera Space:
 *    - +X: Right
 *    - +Y: Up
 *    - -Z: Forward (away from camera into room)
 *
 * 4. Godot Face Local Space:
 *    - +X: Subject's Anatomical Right (Viewer's Left)
 *    - +Y: Up towards top of head
 *    - -Z: Forward out of face (towards camera)
 */
#pragma once

#include "math_defs.hpp"

namespace Gaze
{
namespace CoordinateConversions
{

    /**
     * @brief Transformation matrix mapping OpenCV Camera Space (+X right, +Y down, +Z into scene)
     * to Godot Camera Space (+X right, +Y up, -Z into room).
     * Basis: diag(1, -1, -1)
     */
    inline const GazeBasis3D OPENCV_CAM_TO_GODOT_CAM = GazeBasis3D(
        GazeVector3(1.0,  0.0,  0.0),
        GazeVector3(0.0, -1.0,  0.0),
        GazeVector3(0.0,  0.0, -1.0)
    );

    /**
     * @brief Transformation matrix mapping Godot Face Local Space (+X right, +Y up, -Z forward)
     * to OpenCV Face Model Space (+X left eye, +Y down, +Z into head).
     * Basis: diag(-1, -1, 1)
     */
    inline const GazeBasis3D GODOT_FACE_TO_OPENCV_FACE = GazeBasis3D(
        GazeVector3(-1.0,  0.0, 0.0),
        GazeVector3( 0.0, -1.0, 0.0),
        GazeVector3( 0.0,  0.0, 1.0)
    );

    /**
     * @brief Basis mapping OpenVINO ONNX Gaze Model Output Vector (+x screen right, +y up, -z forward)
     * to Godot Camera Space (+X screen right, +Y up, +Z forward towards user).
     * Basis: diag(1, 1, -1)
     */
    inline const GazeBasis3D ONNX_GAZE_TO_GODOT_CAM = GazeBasis3D(
        GazeVector3(1.0, 0.0,  0.0),
        GazeVector3(0.0, 1.0,  0.0),
        GazeVector3(0.0, 0.0, -1.0)
    );

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

} // namespace CoordinateConversions
} // namespace Gaze
