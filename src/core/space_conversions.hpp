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
         * @brief Map Inference Camera Space coordinates to standard Camera Space
         */
        inline GazeVector3 to_camera_space(const GazeVector3 &v_cv)
        {
            return GazeVector3(v_cv.x, -v_cv.y, -v_cv.z);
        }

        /**
         * @brief Map Inference Face-to-Camera translation and rotation to standard Camera Space GazeTransform3D
         */
        inline GazeTransform3D get_head_transform_in_camera_space(
            const GazeVector3 &inference_translation,
            const GazeVector3 &inference_rvec)
        {
            // 1. T_cv_cam_to_ggaze_cam = Transform(R_X(180), zero)
            // R_X(180) = diag(1, -1, -1)
            GazeBasis3D r_x_180(
                GazeVector3(1, 0, 0),
                GazeVector3(0, -1, 0),
                GazeVector3(0, 0, -1));
            GazeTransform3D T_cv_cam_to_ggaze_cam(r_x_180, GazeVector3(0, 0, 0));

            // 2. T_cv_face_to_cv_cam = Transform(R_cv, t_cv)
            GazeBasis3D R_cv = rodrigues_to_basis(inference_rvec);
            GazeTransform3D T_cv_face_to_cv_cam(R_cv, inference_translation);

            // 3. T_ggaze_face_to_cv_face = Transform(R_X(180), zero)
            // R_X(180) = diag(1, -1, -1)
            GazeBasis3D r_x_180_face(
                GazeVector3(1, 0, 0),
                GazeVector3(0, -1, 0),
                GazeVector3(0, 0, -1));
            GazeTransform3D T_ggaze_face_to_cv_face(r_x_180_face, GazeVector3(0, 0, 0));

            // Chain: T_ggaze_face_to_ggaze_cam = T_cv_cam_to_ggaze_cam * T_cv_face_to_cv_cam * T_ggaze_face_to_cv_face
            return T_cv_cam_to_ggaze_cam * T_cv_face_to_cv_cam * T_ggaze_face_to_cv_face;
        }

    } // namespace Inference
} // namespace Gaze
