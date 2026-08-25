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
         * @brief 5-point face model used specifically for YuNet initial face alignment.
         */
        static inline std::vector<GazeVector3> get_5pt_model_points()
        {
            return {
                GazeVector3(0.0, 0.0, 0.0),            // 0: Nose Tip
                GazeVector3(EYE_X, EYE_Y, EYE_Z),      // 1: Right Eye (Anatomical Right, +X)
                GazeVector3(-EYE_X, EYE_Y, EYE_Z),     // 2: Left Eye (Anatomical Left, -X)
                GazeVector3(MOUTH_X, MOUTH_Y, MOUTH_Z),// 3: Right Mouth Corner (Anatomical Right, +X)
                GazeVector3(-MOUTH_X, MOUTH_Y, MOUTH_Z)// 4: Left Mouth Corner (Anatomical Left, -X)
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
         * @brief 18-point rigid facial core model points (Eyes 0..3, Nose 4..7, Mouth 8..11, Eyebrows 12..17).
         * Excludes moving jawline contour points (18..34) to achieve expression-invariant PnP tracking.
         */
        static inline std::vector<GazeVector3> get_rigid_18pt_model_points()
        {
            auto pts_35 = get_canonical_35pt_face_model();
            return std::vector<GazeVector3>(pts_35.begin(), pts_35.begin() + 18);
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
