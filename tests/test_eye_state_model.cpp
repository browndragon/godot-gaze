#include "doctest.h"
#include "ort_eye_state_model.hpp"
#include <fstream>
#include <vector>
#include <cmath>

inline bool eye_state_file_exists(const std::string &filename) {
    std::ifstream f(filename.c_str());
    return f.good();
}

TEST_CASE("ORT Eye State Classifier Boundary Invariants and Signal Separation")
{
    std::string model_path = "project/addons/godot-gaze/models/mediapipe_eye_openness.ort";
    if (!eye_state_file_exists(model_path)) {
        model_path = "../project/addons/godot-gaze/models/mediapipe_eye_openness.ort";
    }

    if (!eye_state_file_exists(model_path)) {
        MESSAGE("Skipping ORT Eye State Classifier test: Model file not found");
        return;
    }

    Gaze::ORTEyeStateModel model(model_path);
    REQUIRE(model.initialize() == true);

    // Create synthetic open eye crop (60x60 BGR with dark pupil center)
    std::vector<uint8_t> open_crop(60 * 60 * 3, 80);
    // Create synthetic closed eye crop (60x60 BGR with light skin tone)
    std::vector<uint8_t> closed_crop(60 * 60 * 3, 180);

    float open_score = 0.0f;
    float closed_score = 0.0f;

    bool ok1 = model.estimate_openness(open_crop.data(), open_score);
    bool ok2 = model.estimate_openness(closed_crop.data(), closed_score);

    REQUIRE(ok1 == true);
    REQUIRE(ok2 == true);

    // Assert probability bounds [0.0, 1.0]
    CHECK(open_score >= 0.0f);
    CHECK(open_score <= 1.0f);
    CHECK(closed_score >= 0.0f);
    CHECK(closed_score <= 1.0f);

    // Assert strict signal separation between open vs closed
    CHECK(open_score >= 0.70f);
    CHECK(closed_score <= 0.20f);
    CHECK((open_score - closed_score) >= 0.50f);
}
