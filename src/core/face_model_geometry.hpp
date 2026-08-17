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

        static inline std::vector<GazeVector3> get_model_points()
        {
            return {
                GazeVector3(0.0, 0.0, 0.0),            // 0: Nose Tip
                GazeVector3(EYE_X, EYE_Y, EYE_Z),      // 1: Right Eye (Anatomical Right, +X)
                GazeVector3(-EYE_X, EYE_Y, EYE_Z),     // 2: Left Eye (Anatomical Left, -X)
                GazeVector3(MOUTH_X, MOUTH_Y, MOUTH_Z),// 3: Right Mouth Corner (Anatomical Right, +X)
                GazeVector3(-MOUTH_X, MOUTH_Y, MOUTH_Z)// 4: Left Mouth Corner (Anatomical Left, -X)
            };
        }

        static inline std::vector<GazeVector3> get_canonical_godot_model_points()
        {
            return get_model_points();
        }
    };
} // namespace Gaze
