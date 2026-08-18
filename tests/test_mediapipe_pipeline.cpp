#include "doctest.h"
#include "ort_mediapipe_face_mesh.hpp"
#include "face_model_geometry.hpp"
#include <cmath>
#include <iostream>

TEST_CASE("MediaPipe Boundary Space Conversion Invariants")
{
    // Test 1: Canonical 3D face model points
    auto pts_5 = Gaze::FaceModelGeometry::get_5pt_model_points();
    REQUIRE(pts_5.size() == 5);

    // Nose tip (0) is origin
    CHECK(pts_5[0].x == doctest::Approx(0.0));
    CHECK(pts_5[0].y == doctest::Approx(0.0));
    CHECK(pts_5[0].z == doctest::Approx(0.0));

    // Right Eye (1) in Head Local Space has positive X (+31.5mm)
    CHECK(pts_5[1].x == doctest::Approx(31.5));
    CHECK(pts_5[1].y == doctest::Approx(33.4));

    // Left Eye (2) in Head Local Space has negative X (-31.5mm)
    CHECK(pts_5[2].x == doctest::Approx(-31.5));
    CHECK(pts_5[2].y == doctest::Approx(33.4));

    auto pts_35 = Gaze::FaceModelGeometry::get_canonical_35pt_model_points();
    REQUIRE(pts_35.size() == 35);

    // Test 2: Pose Transformation Matrix in Godot Camera Space (+X right, +Y up, -Z forward)
    // Synthetic head at (0, 0, -650mm) facing camera
    Gaze::GazeVector3 true_translation(0.0, 0.0, -650.0);
    // User facing camera has 180 deg (pi rad) Y-axis Euler rotation
    Gaze::GazeBasis3D facing_cam_basis(
        Gaze::GazeVector3(-1.0, 0.0, 0.0), // +X_head (right ear) points along -X_cam
        Gaze::GazeVector3(0.0, 1.0, 0.0),  // +Y_head (top of head) points along +Y_cam
        Gaze::GazeVector3(0.0, 0.0, -1.0)  // +Z_head (back of head) points along -Z_cam
    );

    Gaze::GazeTransform3D head_pose(facing_cam_basis, true_translation);

    // Verify local forward (-Z_head = (0, 0, -1)) transforms to +Z_cam (pointing toward camera at Z=0 from Z=-650)
    Gaze::GazeVector3 local_fwd(0.0, 0.0, -1.0);
    Gaze::GazeVector3 cam_fwd = head_pose.basis.multiply_vector(local_fwd);

    CHECK(cam_fwd.x == doctest::Approx(0.0));
    CHECK(cam_fwd.y == doctest::Approx(0.0));
    CHECK(cam_fwd.z == doctest::Approx(1.0)); // Points toward camera at Z=0!

    // Verify local anatomical right (+X_head = (1, 0, 0)) transforms to -X_cam (camera left)
    Gaze::GazeVector3 local_right(1.0, 0.0, 0.0);
    Gaze::GazeVector3 cam_right = head_pose.basis.multiply_vector(local_right);

    CHECK(cam_right.x == doctest::Approx(-1.0));
    CHECK(cam_right.y == doctest::Approx(0.0));
    CHECK(cam_right.z == doctest::Approx(0.0));
}
