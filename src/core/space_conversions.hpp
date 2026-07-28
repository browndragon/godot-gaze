/**
 * @file space_conversions.hpp
 * @brief Coordinate Space Transformations (Layer 2 - Core)
 *
 * Defines mathematical conversions between Inference camera space and GodotGaze space
 * under a dedicated Gaze::Inference namespace.
 */

// TODO: Do we actually use these? Like, `get_head_transform_in_camera_space` doesn't seem to be used outside of docs & tests. That's annoying!
// TODO: Clean up unused refs.
#pragma once

#include "math_defs.hpp"

namespace Gaze
{
    namespace Inference
    {

        /**
         * @brief Standard transformation matrix mapping OpenCV Camera Space (+X right, +Y down, +Z into scene)
         * to Godot Camera Space (+X right, +Y up, +Z towards screen plane / camera front).
         * Matrix: diag(1, -1, -1)
         */
        inline const GazeTransform3D OPENCV_CAM_TO_GODOT_CAM = GazeTransform3D(
            GazeBasis3D(
                GazeVector3(-1.0, 0.0, 0.0),
                GazeVector3(0.0, -1.0, 0.0),
                GazeVector3(0.0, 0.0, -1.0)
            ),
            GazeVector3(0.0, 0.0, 0.0)
        );

        inline const GazeTransform3D CAMERA_TRANSFORM = OPENCV_CAM_TO_GODOT_CAM;

        /**
         * @brief Transformation matrix mapping Godot Face Local Space (+X anatomic right, +Y up, -Z forward)
         * to OpenCV Face Model Space (+X anatomic left eye, +Y down to mouth, +Z out of face).
         * Matrix: diag(-1, -1, 1)
         */
        inline const GazeTransform3D GODOT_FACE_TO_OPENCV_FACE = GazeTransform3D(
            GazeBasis3D(
                GazeVector3(-1.0, 0.0, 0.0),
                GazeVector3(0.0, -1.0, 0.0),
                GazeVector3(0.0, 0.0, 1.0)
            ),
            GazeVector3(0.0, 0.0, 0.0)
        );

        /**
         * @brief Basis mapping OpenVINO ONNX Gaze Model Output Vector (+x subject right, +y up, -z into scene)
         * to Godot Camera Space (-X left, +Y up, +Z towards screen plane).
         * Basis: diag(-1, 1, -1)
         */
        inline const GazeBasis3D ONNX_GAZE_TO_GODOT_CAM = GazeBasis3D(
            GazeVector3(-1.0, 0.0, 0.0),
            GazeVector3(0.0, 1.0, 0.0),
            GazeVector3(0.0, 0.0, -1.0)
        );

        /**
         * @brief Map Inference Face-to-Camera translation and rotation to standard Camera Space GazeTransform3D
         */
        inline GazeTransform3D get_head_transform_in_camera_space(
            const GazeVector3 &inference_translation,
            const GazeVector3 &inference_rvec)
        {
            // T_cv_face_to_cv_cam = Transform(R_cv, t_cv)
            GazeBasis3D R_cv = rodrigues_to_basis(inference_rvec);
            GazeTransform3D T_cv_face_to_cv_cam(R_cv, inference_translation);

            // Chain: T_ggaze_face_to_ggaze_cam = OPENCV_CAM_TO_GODOT_CAM * T_cv_face_to_cv_cam * GODOT_FACE_TO_OPENCV_FACE
            return OPENCV_CAM_TO_GODOT_CAM * T_cv_face_to_cv_cam * GODOT_FACE_TO_OPENCV_FACE;
        }

    } // namespace Inference
} // namespace Gaze
