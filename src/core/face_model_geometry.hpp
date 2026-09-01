/**
 * @file face_model_geometry.hpp
 * @brief Single Source of Truth for 3D Facial Model Geometry in Head Local Space
 */
#pragma once

#include "math_defs.hpp"
#include <vector>

namespace Gaze
{
    class FaceModelGeometry
    {
    public:
        // Canonical Anthropometric 3D Head Model Points (mm) in Head Local Space:
        // Origin (0,0,0) = Nose Tip
        // +X = Subject's Anatomical Right (viewer left)
        // +Y = Up towards top of head
        // -Z = Forward out of face (facing camera)

        static constexpr double EYE_X = 31.5;   // Inter-pupillary half-distance (63mm IPD)
        static constexpr double EYE_Y = 33.4;   // Eye height above nose tip
        static constexpr double EYE_Z = 18.0;   // Eye depth behind nose tip

        static constexpr double MOUTH_X = 24.2; // Mouth corner half-width (48.4mm mouth width)
        static constexpr double MOUTH_Y = -28.1;// Mouth corner height below nose tip
        static constexpr double MOUTH_Z = 10.0; // Mouth corner depth behind nose tip

        static constexpr double DEFAULT_NOSE_Y = 0.0;
        static constexpr double DEFAULT_NOSE_Z = 0.0;

        /**
         * @brief 5-point face model matching YuNet keypoints, derived directly from canonical 35-point model.
         */
        static inline std::vector<GazeVector3> get_5pt_model_points()
        {
            auto pts_35 = get_canonical_35pt_face_model();
            return {
                pts_35[5],                                                                     // 0: Nose Tip (Origin)
                GazeVector3((pts_35[0].x + pts_35[1].x) * 0.5f, (pts_35[0].y + pts_35[1].y) * 0.5f, (pts_35[0].z + pts_35[1].z) * 0.5f), // 1: Right Eye (Image Left)
                GazeVector3((pts_35[2].x + pts_35[3].x) * 0.5f, (pts_35[2].y + pts_35[3].y) * 0.5f, (pts_35[2].z + pts_35[3].z) * 0.5f), // 2: Left Eye (Image Right)
                pts_35[8],                                                                     // 3: Right Mouth Corner
                pts_35[9]                                                                      // 4: Left Mouth Corner
            };
        }

        /**
         * @brief Canonical 35-point anthropometric 3D face model matching ADAS-0002 landmarks.
         */
        static inline std::vector<GazeVector3> get_canonical_35pt_model_points()
        {
            return get_canonical_35pt_face_model();
        }

        /**
         * @brief Primary canonical 3D model points exposed to Godot for debug overlays and tracking.
         * Converts OpenCV Model Space (-X Right, -Y Up, -Z Forward) to Godot Face Local Space (+X Right, +Y Up, -Z Forward).
         */
        static inline std::vector<GazeVector3> get_canonical_godot_model_points()
        {
            auto cv_pts = get_canonical_35pt_face_model();
            std::vector<GazeVector3> godot_pts(cv_pts.size());
            for (size_t i = 0; i < cv_pts.size(); ++i) {
                godot_pts[i] = GazeVector3(-cv_pts[i].x, -cv_pts[i].y, cv_pts[i].z);
            }
            return godot_pts;
        }
    };
} // namespace Gaze
