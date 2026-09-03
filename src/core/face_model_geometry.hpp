/**
 * @file face_model_geometry.hpp
 * @brief Single Source of Truth for 3D Facial Model Geometry in Head Local Space
 */
#pragma once

#include "opencv_space_conversions.hpp"
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
         * @brief Canonical 35-point anthropometric 3D face model defined in OpenCV model space
         * (+X image right / left ear, +Y down towards chin, +Z back into skull, face facing camera at rvec = 0).
         */
        static inline std::vector<OpenCVFaceVector3> get_canonical_35pt_model_points()
        {
            std::vector<OpenCVFaceVector3> pts(35);
            // Eyes (IPD approx 63mm, positioned +35mm behind nose tip in Z)
            pts[0] = OpenCVFaceVector3(-15.0, -32.0,  35.0); // Image Left Eye Inner Canthus (Anatomical Right)
            pts[1] = OpenCVFaceVector3(-46.0, -32.0,  43.0); // Image Left Eye Outer Canthus
            pts[2] = OpenCVFaceVector3( 15.0, -32.0,  35.0); // Image Right Eye Inner Canthus (Anatomical Left)
            pts[3] = OpenCVFaceVector3( 46.0, -32.0,  43.0); // Image Right Eye Outer Canthus

            // Nose (Pt 5 is the Nose Tip Origin (0,0,0))
            pts[4] = OpenCVFaceVector3(  0.0, -22.0,  20.0); // Nose Bridge Top
            pts[5] = OpenCVFaceVector3(  0.0,   0.0,   0.0); // Nose Tip Origin (0,0,0)
            pts[6] = OpenCVFaceVector3(-16.0,   6.0,  20.0); // Right Nose Wing (Image Left)
            pts[7] = OpenCVFaceVector3( 16.0,   6.0,  20.0); // Left Nose Wing (Image Right)

            // Mouth
            pts[8]  = OpenCVFaceVector3(-25.0,  32.0,  30.0); // Right Mouth Corner (Image Left)
            pts[9]  = OpenCVFaceVector3( 25.0,  32.0,  30.0); // Left Mouth Corner (Image Right)
            pts[10] = OpenCVFaceVector3(  0.0,  26.0,  23.0); // Upper Lip Center
            pts[11] = OpenCVFaceVector3(  0.0,  40.0,  27.0); // Lower Lip Center

            // Eyebrows
            pts[12] = OpenCVFaceVector3(-12.0, -48.0,  35.0); // Right Eyebrow Inner (Image Left)
            pts[13] = OpenCVFaceVector3(-32.0, -52.0,  40.0); // Right Eyebrow Mid
            pts[14] = OpenCVFaceVector3(-50.0, -48.0,  43.0); // Right Eyebrow Outer
            pts[15] = OpenCVFaceVector3( 12.0, -48.0,  35.0); // Left Eyebrow Inner (Image Right)
            pts[16] = OpenCVFaceVector3( 32.0, -52.0,  40.0); // Left Eyebrow Mid
            pts[17] = OpenCVFaceVector3( 50.0, -48.0,  43.0); // Left Eyebrow Outer

            // 17-point Jawline Contour (Pts 18..34) from Image Left / Right Ear to Chin Apex (Pt 26) to Image Right / Left Ear
            double jaw_x[] = {-70.0, -68.0, -64.0, -58.0, -50.0, -40.0, -28.0, -15.0, 0.0, 15.0, 28.0, 40.0, 50.0, 58.0, 64.0, 68.0, 70.0};
            double jaw_y[] = {-35.0, -20.0,  -5.0,  12.0,  28.0,  44.0,  58.0,  68.0, 70.0, 68.0, 58.0, 44.0, 28.0, 12.0, -5.0, -20.0, -35.0};
            double jaw_z[] = { 70.0,  65.0,  59.0,  51.0,  43.0,  37.0,  33.0,  31.0, 35.0, 31.0, 33.0, 37.0, 43.0, 51.0, 59.0, 65.0, 70.0};

            for (int i = 0; i < 17; ++i)
            {
                pts[18 + i] = OpenCVFaceVector3(jaw_x[i], jaw_y[i], jaw_z[i]);
            }

            return pts;
        }

        /**
         * @brief 5-point face model matching YuNet keypoints, derived directly from canonical 35-point model.
         */
        static inline std::vector<OpenCVFaceVector3> get_5pt_model_points()
        {
            auto pts_35 = get_canonical_35pt_model_points();
            return {
                pts_35[5],                                                                     // 0: Nose Tip (Origin)
                (pts_35[0] + pts_35[1]) * 0.5,                                                // 1: Right Eye (Image Left)
                (pts_35[2] + pts_35[3]) * 0.5,                                                // 2: Left Eye (Image Right)
                pts_35[8],                                                                     // 3: Right Mouth Corner
                pts_35[9]                                                                      // 4: Left Mouth Corner
            };
        }

        /**
         * @brief Primary canonical 3D model points exposed to Godot for debug overlays and tracking.
         * Converts OpenCV Model Space (-X Right, -Y Up, -Z Forward) to Godot Face Local Space (+X Right, +Y Up, -Z Forward).
         */
        static inline std::vector<GodotFaceVector3> get_canonical_godot_model_points()
        {
            auto cv_pts = get_canonical_35pt_model_points();
            std::vector<GodotFaceVector3> godot_pts(cv_pts.size());
            for (size_t i = 0; i < cv_pts.size(); ++i) {
                godot_pts[i] = CoordinateConversions::to_godot_face(cv_pts[i]);
            }
            return godot_pts;
        }
    };
} // namespace Gaze
