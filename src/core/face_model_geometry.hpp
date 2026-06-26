/**
 * @file face_model_geometry.hpp
 * @brief 3D Facial Model Reference Coordinates (Layer 2)
 *
 * Defines static coordinate constants for the canonical 3D facial model
 * (eyes, nose, mouth corners) used during Perspective-n-Point (solvePnP)
 * head pose estimation.
 */
#pragma once

namespace Gaze {

struct FaceModelGeometry {
    static constexpr double EYE_X = 30.0;
    static constexpr double EYE_Y = -28.676;
    static constexpr double EYE_Z = 0.0;

    static constexpr double DEFAULT_NOSE_Y = -0.5;
    static constexpr double DEFAULT_NOSE_Z = -52.0;

    static constexpr double MOUTH_X = 18.462;
    static constexpr double MOUTH_Y = 31.712;
    static constexpr double MOUTH_Z = -4.550;
};

} // namespace Gaze
